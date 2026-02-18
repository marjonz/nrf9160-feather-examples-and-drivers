/*
 * Copyright (c) 2020 Circuit Dojo LLC
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/pm/device.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

/* nRF Libraries */
#include <modem/nrf_modem_lib.h>
#include <modem/lte_lc.h>
#include <modem/modem_info.h>

#include <modem/nrf_modem_lib.h>
#include <nrf_modem_at.h>

/* Local */
#include "cloud/cloud.h"
#include "cloud/apiclient/api_client.h"
#include "config/device_cfg.h"
#include "date_time.h"
#include "epaper/epaper.h"
#include "flash/flash_fs.h"

#include "lib/macro.h"
#include "version.h"

#include <inttypes.h>
#include <stdint.h>

//Header Parsing 
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "cloud/miniz/miniz.h" 

#ifndef CONFIG_APN_DEFAULT_VALUE
#   define CONFIG_APN_DEFAULT_VALUE "onemondo"
#endif

#define CONFIG_RETRY_DELAY_MINUTES 1

/* Timer */
static void timeout_handler(struct k_timer *timer_id);
K_TIMER_DEFINE(timer, timeout_handler, NULL);

/* Thread control */
K_SEM_DEFINE(thread_sem, 0, 1);
K_SEM_DEFINE(lte_connected, 0, 1);
K_SEM_DEFINE(new_display, 0, 1);

/* Variables */
// Declare a static image buffer for the ePaper display
// Each byte represents 8 horizontal pixels.
#define MAX_WIDTH   800u
#define MAX_HEIGHT  480u
#define MAX_IMAGE_SIZE ((MAX_WIDTH/8u) * MAX_HEIGHT)

//Also need a receive buffer for the compressed data received from the http request 
static uint8_t http_receive_buf[MAX_IMAGE_SIZE] = {0}; 

//Create a new image cache
static uint8_t DisplayImage[MAX_IMAGE_SIZE] = {0};
static size_t current_display_image_index = 0u;
static device_cfg_t * device_cfg_ptr = NULL;

static void timeout_handler(struct k_timer *timer_id)
{
    LOG_INF("Main loop going back to sleep");
    k_sem_give(&thread_sem);
}

static void lte_handler(const struct lte_lc_evt *evt)
{
    switch (evt->type) {
    case LTE_LC_EVT_NW_REG_STATUS:
        if ((evt->nw_reg_status == LTE_LC_NW_REG_REGISTERED_HOME) ||
            (evt->nw_reg_status == LTE_LC_NW_REG_REGISTERED_ROAMING)) 
        {
            LOG_INF("LTE connected to network.");
            k_sem_give(&lte_connected);
        }
        break;

    default:
        break;
    }
}

/* Initialization of AUX pin */
#if defined(CONFIG_BOARD_CIRCUITDOJO_FEATHER_NRF9151)
#define AUXANTCFG_ENABLE "AT\%XANTCFG=1"

static inline void set_apn_name(const char * apn_name)
{
#   define GET_APN_CONFIG "AT+CGDCONT?"
#   define SET_APN_CONFIG "AT+CGDCONT=1,\"IP\","
    char response[5u] = {0};
    char at_command[60] = {0};
    ZERO_ARRAY(response);
    ZERO_ARRAY(at_command);

    // Build the command to send
    snprintf(at_command, sizeof(at_command), "%s\"%s\"\n", SET_APN_CONFIG, apn_name);

    LOG_DBG("Sending APN config: %sn", at_command);
    int err = nrf_modem_at_cmd(response, sizeof(response), "%s", at_command);
    if (err)
    {
        LOG_ERR("Failed to set APN configuration (err: %d)", err);
    }
    LOG_DBG("APN config Response: %s", response);
}

NRF_MODEM_LIB_ON_INIT(aux_init_hook, on_modem_lib_init, NULL);
static void on_modem_lib_init(int ret, void *ctx)
{
    ARG_UNUSED(ctx);

    if (ret != 0)
    {
        return;
    }

    LOG_DBG("*** Setting configuration: %s ***\n", AUXANTCFG_ENABLE);
    int err = nrf_modem_at_printf("%s", AUXANTCFG_ENABLE);
    if (err)
    {
        LOG_ERR("Failed to set configuration (err: %d)", err);
    }

    set_apn_name(CONFIG_APN_DEFAULT_VALUE);
}
#endif

/**
 * The received http data, pushing the data into the display image buffer. Already do this in the 
 * http response callback when we uncompress the data. 
*/
static void push_http_bmp_into_epaper_buffer(bool is_first_chunk_of_data, 
    const uint8_t * const http_bmp_data, size_t http_data_len)
{
    if (is_first_chunk_of_data)
    {
        current_display_image_index = 0u;
    }

    int status = epaper_store_data_to_buffer(DisplayImage, current_display_image_index, sizeof(DisplayImage), 
        http_bmp_data, http_data_len);
    if (status == -EDOM || status == -EINVAL)
    {
        LOG_ERR("Failed to store http bmp data.");
    }
    current_display_image_index = status;
}

static void store_epaper_buffer_to_file(const char * filename, const uint8_t * const epaper_buffer)
{
    int result = flash_fs_write_file_to_fs(filename, DisplayImage, sizeof(DisplayImage));
    if (result != 0)
    {
        LOG_ERR("Failed to write epaper bitmap to file.");
    }
}

/* Define the stack sizes for the threads */
#define STACK_SIZE                  1024
#define CLOUD_THREAD_STACK_SIZE     (2*STACK_SIZE)
#define FLASH_THREAD_STACK_SIZE     (3*STACK_SIZE)
#define EPAPER_THREAD_STACK_SIZE    (2*STACK_SIZE)
/* Define thread priorities (lower number = higher priority) */
#define CLOUD_PRIORITY 7 
#define EPAPER_PRIORITY 8
#define FLASH_FS_PRIORITY 9

static void get_time_now(void)
{
    int64_t time_now = 0;
    int err = date_time_now(&time_now);
    if (err < 0)
    {
        LOG_ERR("Failed to get date/time. Err: %i", err);
        return;
    }
    LOG_DBG("Time now: %" PRIi64 "ms", time_now);
}

static void date_time_handler(const struct date_time_evt *evt) 
{
    switch (evt->type) 
    {
        case DATE_TIME_OBTAINED_NTP:
            LOG_DBG("Time obtained from NTP\n");
            // Perform actions with time here
            break;
        case DATE_TIME_OBTAINED_MODEM:
            LOG_DBG("Time obtained from Modem\n");
            break;
        default:
            break;
    }
}

#if 0
static void dump_of_request_update_response_handler_from_arduino(void)
{
      // Check for connection/timeout errors
  if (code < 0) {
    printf("[APIClient] HTTP error: %d\n", code);
    result.success = false;
    result.hasUpdate = false;
    return result;
  }
  
    std::map<String, String> headers = readAllHeaders(http);
  if (headers.find("ETag") != headers.end()) {
    printf("[APIClient] Last ETag %s\n", headers["ETag"]);
    result.lastETag = headers["ETag"];
  }

  // Extract X-Config-Version
  if (headers.find("X-Config-Version") != headers.end()) {
    result.configVersion = headers["X-Config-Version"];
    printf("[APIClient] X-Config-Version: %s\n", result.configVersion.c_str());
  }

  // Extract X-Sleep-Seconds
  if (headers.find("X-Sleep-Seconds") != headers.end()) {
    result.sleepForSeconds = headers["X-Sleep-Seconds"].toInt();
    printf("[APIClient] X-Sleep-Seconds: %d seconds\n", result.sleepForSeconds);
  } else {
    result.sleepForSeconds = 0;
  }

  if (code == 200)
  {
    printf("[APIClient] Update available, status code: %d\n", code);
    String response = http.responseBody();
    int compressedLen = response.length();
    printf("[APIClient] Compressed response length: %d\n", compressedLen);

    // Allocate destination buffer (assume uncompressed will not exceed 48000)
    mz_ulong uncompressedLen = 48000;
    uint8_t* uncompressed = new uint8_t[uncompressedLen];
    int resultCode = mz_uncompress(uncompressed, &uncompressedLen, (const uint8_t*)response.c_str(), compressedLen);

    if (resultCode == Z_OK) {
      printf("[APIClient] Uncompressed BMP length: %lu\n", uncompressedLen);
      result.rucBMP = uncompressed;
      result.hasUpdate = true;
      result.success = true;
    } else {
      printf("[APIClient] Failed to decompress BMP, zlib error: %d\n", resultCode);
      result.errorMessage = "Decompression failed";
      delete[] uncompressed;
      result.hasUpdate = false;
      result.success = false;
    }
  }
  else if (code == 304)
  {
    printf("[APIClient] No update available, status code: %d\n", code);
    result.hasUpdate = false;
    result.success = true;
  }
  else if (code >= 400)
  {
    printf("[APIClient] Error HTTP status: %d\n", code);
    result.hasUpdate = false;
    result.success = false;
    
    // Read and log the response body
    String response = http.responseBody();
    printf("[APIClient] Error response body: %s\n", response.c_str());
    
    // Try to parse JSON error message
    DynamicJsonDocument doc(256);
    DeserializationError error = deserializeJson(doc, response);
    
    if (!error) {
      const char* errorCode = doc["error"];
      const char* message = doc["message"];
      
      if (errorCode && message) {
        printf("[APIClient] Parsed error - Code: %s, Message: %s\n", errorCode, message);
        result.errorMessage = String(errorCode) + ": " + String(message);
      } else if (message) {
        result.errorMessage = String(message);
      } else if (errorCode) {
        result.errorMessage = String(errorCode);
      } else {
        result.errorMessage = "HTTP " + String(code);
      }
    } else {
      printf("[APIClient] Failed to parse JSON error: %s\n", error.c_str());
      result.errorMessage = "HTTP " + String(code);
    }
  }
  else
  {
    printf("[APIClient] Unexpected HTTP status: %d\n", code);
    result.hasUpdate = false;
    result.success = false;
    result.errorMessage = "HTTP " + String(code);
  }
}

static void dump_of_request_config_response_handler_from_arduino(void)
{
    // Check for connection/timeout errors
    if (code < 0) {
        printf("[APIClient] Config HTTP error: %d\n", code);
        result.errorMessage = "HTTP error: " + String(code);
        return result;
    }

    if (code == 200) {
        printf("[APIClient] Config fetch successful, parsing JSON\n");
        String response = http.responseBody();
        printf("[APIClient] Config response: %s\n", response.c_str());

        // Parse JSON response
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, response);

        if (error) {
        printf("[APIClient] JSON parse error: %s\n", error.c_str());
        result.errorMessage = "JSON parse error: " + String(error.c_str());
        return result;
        }

        // Extract config fields
        result.apn = doc["apn"] | "";
        result.wifiSSID = doc["wifiSSID"] | "";
        result.wifiPassword = doc["wifiPassword"] | "";
        result.apiSecret = doc["apiSecret"] | "";
        result.apiUrl = doc["apiUrl"] | "";
        result.configVersion = doc["configVersion"] | "";

        // Validate required fields
        if (result.apiUrl.length() == 0 || result.configVersion.length() == 0) {
        printf("[APIClient] Invalid config: missing required fields\n");
        result.errorMessage = "Missing required config fields";
        return result;
        }

        result.success = true;
        printf("[APIClient] Config parsed successfully\n");
    } else {
    printf("[APIClient] Config fetch failed with status: %d\n", code);
    result.errorMessage = "HTTP " + String(code);
    }
}
#endif

static void update_with_new_string(char * const target_string, char * const source_string, size_t max_len)
{
    // Update the target string
    memset(target_string, 0, max_len);
    strncpy(target_string, source_string, max_len - 1u);
}

static bool is_strings_same(char * str1, const char * str2, size_t max_len)
{
    if (strncmp(str1, str2, max_len) != 0)
    {
        LOG_DBG("New string detected: %s", str2);
        return true;
    }
    return false;
}

int get_header_value(const char *response_start,
                     const char *body_start,
                     const char *field_name,
                     char *out,
                     size_t out_size)
{
    size_t header_len = (size_t)(body_start - response_start);
    size_t field_len  = strlen(field_name);

    const char *p   = response_start;
    const char *end = response_start + header_len;

    while (p < end) {
        const char *eol = strstr(p, "\r\n");
        if (!eol || eol > end) break;

        /* Check if this line starts with "FieldName: " */
        if ((size_t)(eol - p) > field_len + 2 &&
            strncmp(p, field_name, field_len) == 0 &&
            p[field_len] == ':' && p[field_len + 1] == ' ')
        {
            const char *val     = p + field_len + 2;
            size_t      val_len = (size_t)(eol - val);

            if (val_len >= out_size) val_len = out_size - 1;
            memcpy(out, val, val_len);
            out[val_len] = '\0';
            return 0;
        }

        p = eol + 2;
    }

    return -1;  /* not found */
}

static void response_callback(struct http_response *rsp,
				   enum http_final_call final_data,
				   void *user_data)
{
    LOG_INF("HTTP Status %d", rsp->http_status_code);

    /* Check status */
    if (rsp->http_status_code != 200 && rsp->http_status_code != 201)
    {
        LOG_INF("Bad Request, returning"); 
        return;
    }

    if (final_data == HTTP_DATA_MORE) 
    {
        LOG_INF("Partial data received (%zd bytes)", rsp->data_len);
        LOG_INF("Data Processed: %zd", rsp->processed);
        LOG_INF("Max Data Len: %zd", rsp->recv_buf_len); 
    }
    if (final_data == HTTP_DATA_FINAL)
    {
        #ifdef DEBUG_HTTP_RESPONSE
        LOG_INF("All the data received (%zd bytes)", rsp->data_len);
        //Hexdump seems to cause stack overflow 
        //LOG_HEXDUMP_INF(rsp->recv_buf, rsp->recv_buf_len, "Response data");

        //Print out as much info as I can 
        LOG_INF("Data Start Address: 0x%x", rsp->recv_buf); 
        LOG_INF("Body Frag Start Address: 0x%x", rsp->body_frag_start); 
        LOG_INF("Body Frag Length: %d", rsp->body_frag_len);
        LOG_INF("Max Data Len: %zd", rsp->recv_buf_len); 
        LOG_INF("Data Processed: %zd", rsp->processed);
        LOG_INF("CL Present: %d Body Found: %d Message Complete: %d", rsp->cl_present, rsp->body_found, rsp->message_complete); 

        printf("Header Dump\r\n"); 
        for (int i = 0; i < (rsp->data_len - rsp->body_frag_len); i++) 
        {
            printf("%02x ", rsp->recv_buf[i]);  
        }
        printf("\r\n");

        printf("Body Dump\r\n");
        for (int i = 0; i < (rsp->body_frag_len); i++) 
        { 
            printf("%02x ", rsp->body_frag_start[i]); 

        }
        printf("\r\n"); 
        #endif 

        /*
        //Extract ETag and Version. On the stack for now but need to change and put in main
        device_cfg_t http_dev_cfg = {0}; 
        //Copy existing values into this so we dont overwrite legit values in other fields
        memcpy(&http_dev_cfg, device_cfg_ptr, sizeof(device_cfg_t)); 
        char etag[DEV_CFG_MAX_ETAG_LENGTH]; 
        char version[DEV_CFG_MAX_VERSION_LENGTH]; 
        bool update = false; 

        if (get_header_value(rsp->recv_buf, rsp->body_frag_start, "ETag", etag, sizeof(etag)) == 0)
        {
            printf("ETag: %s\n", etag); 
            //Compare to existing etag val in the device cfg 
            if (strcmp(etag, device_cfg_ptr->last_etag) != 0)
            {
                //Update the val
                strcpy(http_dev_cfg.last_etag, etag); 
                printf("New Etag: %s\r\n", http_dev_cfg.last_etag);
                //Flag update? 
                update = true; 
            }
        }

        if (get_header_value(rsp->recv_buf, rsp->body_frag_start, "X-Config-Version", version, sizeof(version)) == 0)
        {
            printf("Version: %s\n", version); 
            //Compare to existing etag val in the device cfg 
            if (strcmp(version, device_cfg_ptr->version) != 0)
            {
                //Update the val
                strcpy(http_dev_cfg.version, version); 
                printf("New Version: %s\r\n", http_dev_cfg.version);
                //Flag update? 
                update = true; 
            }
        }

        if (update) 
        {
            //Have to make a copy 
            device_cfg_set(&http_dev_cfg); 
        }
        */

        if (!rsp->body_found)
        {
            LOG_INF("Body not found");
            return;
        }

        // FIXME: Process response
        ARG_UNUSED(user_data);

        //Do the decompression, extract the http receive buffer to the display image 
        mz_ulong uncompressed_len = MAX_IMAGE_SIZE;  
        int ret_code = mz_uncompress(DisplayImage, &uncompressed_len, rsp->body_frag_start, rsp->body_frag_len); 

        if (ret_code == Z_OK)
        {
            #ifdef DEBUG_HTTP_RESPONSE
            LOG_INF("Uncompressed Data!"); 
            LOG_INF("Dest Len: %zd", uncompressed_len); 
            printf("Decompressed Data Dump\r\n");
            for (int i = 0; i < MAX_IMAGE_SIZE; i++) 
            { 
                printf("%02x", DisplayImage[i]); 
            }
            printf("\r\n");
            #endif 
        }
        else 
        {
            LOG_INF("Failed to decompress :( Err Code: %d", ret_code); 
        }

        
        /*
        // Store the updated device configuration to flash
        if (must_update_device_cfg)
        {
            int err = device_cfg_set(device_cfg_ptr);
            if (err < 0)
            {
                LOG_ERR("Failed to store updated device configuration. Err: %i", err);
            }
        }
            

        // FIXME: Do I still need to push this, if the actual response is already in the DisplayImage buffer via the callback?
        //          The other question is, also, do I need to have a separate buffer for the HTTP response and then copy to DisplayImage here?
        //push_http_bmp_into_epaper_buffer(<response buffer>, DisplayImage);
        store_epaper_buffer_to_file(device_cfg_ptr->version, DisplayImage);
        */
    }
}

/* Thread entry function for the first thread (e.g., blinking an LED) */
void cloud_thr(void *p1, void *p2, void *p3) 
{

    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    
    // Register date time handler
    ARG_UNUSED(date_time_handler);
    date_time_register_handler(date_time_handler);

    /* Register callback handler to handler LTE events */
    lte_lc_register_handler(lte_handler);

    /* Init modem lib */
    int err = nrf_modem_lib_init();
    if (err < 0)
    {
        LOG_ERR("Failed to init modem lib. (err: %i)", err);
        return;
    }

    /* Cloud init */
    err = cloud_init();
    if (err < 0)
    {
        LOG_ERR("Unable to set callback. Err: %i", err);
        return;
    }

    /* Power saving is turned on */
    lte_lc_psm_req(false);

    /* Connect */
    LOG_INF("Connecting to LTE...");
    err = lte_lc_connect();
    /* Wait indefinitely (or add timeout?) */
    k_sem_take(&lte_connected, K_FOREVER);
    if (err < 0)
    {
        LOG_ERR("Failed to connect. Err: %i", err);
        return;
    }

    get_time_now();

    /* Wait for a while, because with IPv4v6 PDN the IPv6 activation takes a bit more time. */
	k_sleep(K_SECONDS(1));

    LOG_INF("Safe to use sockets now. LTE is connected.");

    get_time_now();

    /* Start timer to periodically wake the device and publish data */
    k_timer_start(&timer, K_MINUTES(CONFIG_DEFAULT_DELAY_MINUTES), K_MINUTES(CONFIG_DEFAULT_DELAY_MINUTES));

    /* Allow for instant publish */
    k_sem_give(&thread_sem);

    device_cfg_ptr = device_cfg_get();

    while (1)
    {
        k_sem_take(&thread_sem, K_FOREVER);

        /* Publish and wait for response */
        // api_client_request_udpate does have a timeout so should release the semaphore in the callback eventually.
        // See cloud.c : const int32_t get_timeout_ms = 30000; // As per fleetpin implementation, 30 second timeout.
        // FIXME: Implement the configugration callback handler separately
        //api_client_fetch_config("configuration", device_cfg_ptr, dump_of_request_config_response_handler_from_arduino, DisplayImage, sizeof(DisplayImage));
        api_client_request_udpate("/ruc/label.bmp", device_cfg_ptr, response_callback, http_receive_buf, sizeof(http_receive_buf));

        k_sem_give(&new_display);
        //Returned from the cloud functionality. Should have freed the display image 
        
        // Wait until the socket times out or a response is received.
        k_sem_take(&thread_sem, K_FOREVER);

        if (err < 0)
        {
            LOG_ERR("Unable to publish. Err: %i", err);
            // Stop the current timer and attempt to republish in CONFIG_RETRY_DELAY_MINUTES minutes
            k_timer_stop(&timer);
            k_timer_start(&timer, K_MINUTES(CONFIG_RETRY_DELAY_MINUTES), K_MINUTES(CONFIG_RETRY_DELAY_MINUTES));
        }
        else
        {
            LOG_INF("Cloud publish successful.");
            // Stop the current timer and republish in the normal default delay interval
            k_timer_stop(&timer);
            k_timer_start(&timer, K_MINUTES(CONFIG_DEFAULT_DELAY_MINUTES), K_MINUTES(CONFIG_DEFAULT_DELAY_MINUTES));
        }
    }
}

/* Thread entry function for the third thread */
void epaper_thr(void *p1, void *p2, void *p3) 
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    int err = epaper_init(DisplayImage);
    if (err < 0)
    {
        LOG_ERR("Failed to init epaper. (err: %i)", err);
        return;
    }
    k_sleep(K_SECONDS(1));

    // ePAPER test display
    epaper_display_test();

    while(1)
    {
        k_sem_take(&new_display, K_FOREVER);
        LOG_INF("New Image!!!"); 
        /*
        printf("Decompressed Data Dump\r\n");
        for (int i = 0; i < MAX_IMAGE_SIZE; i++) 
        { 
            printf("%02x", DisplayImage[i]); 
        }
        printf("\r\n");
        */
        epaper_draw_current_image_buffer(DisplayImage); 
        //Set display image to zeros to avoid re writing?? 

    }

}

/* Thread entry function for the fourth thread */
void flash_fs_thr(void *p1, void *p2, void *p3) 
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    flash_fs_init();

    // Try and read the device configuration from the file system.
    device_cfg_init();
}

/* Define the threads using K_THREAD_DEFINE */
K_THREAD_DEFINE(cloud_thread_id, CLOUD_THREAD_STACK_SIZE, cloud_thr, NULL, NULL, NULL, CLOUD_PRIORITY, 0, 0);
K_THREAD_DEFINE(epaper_thread_id, EPAPER_THREAD_STACK_SIZE, epaper_thr, NULL, NULL, NULL, EPAPER_PRIORITY, 0, 0);
K_THREAD_DEFINE(flash_fs_thread_id, FLASH_THREAD_STACK_SIZE, flash_fs_thr, NULL, NULL, NULL, FLASH_FS_PRIORITY, 0, 0);

int main(void)
{
    LOG_INF("Fleetpin Project. Board: %s", CONFIG_BOARD);

    /* The main thread can also perform work or go to sleep */
    while (1) 
    {
        k_sleep(K_FOREVER); /* Sleep the main thread indefinitely */
    }
}
