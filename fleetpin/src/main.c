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
#include <stdio.h>

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

/* Variables */
// Declare a static image buffer for the ePaper display
// Each byte represents 8 horizontal pixels.
#define MAX_WIDTH   800u
#define MAX_HEIGHT  480u
#define MAX_IMAGE_SIZE ((MAX_WIDTH/8u) * MAX_HEIGHT)
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

static void response_callback(struct http_response *rsp,
				   enum http_final_call final_data,
				   void *user_data)
{
    LOG_INF("HTTP Status %d", rsp->http_status_code);

    /* Check status */
    if (rsp->http_status_code != 200 && rsp->http_status_code != 201)
    {
        return;
    }

    if (final_data == HTTP_DATA_FINAL)
    {
        LOG_HEXDUMP_INF(rsp->recv_buf, rsp->recv_buf_len, "Response data");

        if (!rsp->body_found)
        {
            LOG_ERR("Body not found");
            return;
        }

        // FIXME: Process response
        ARG_UNUSED(user_data);

        // FIXME: Essentianlly, implement dump_of_request_update_response_handler_from_arduino() in here.
        //        I dumped the code above for reference from the customer's Arduino project.

        // If we need to store the response, copy the response to DisplayImage
        char new_version[DEV_CFG_MAX_VERSION_LENGTH];
        ZERO_ARRAY(new_version);
        // FIXME: copy the response version string to new_version.
        // You'll have to extract it from the key + value pair of the response body.
        // This is currently wrong, but a placeholder
        strncpy(new_version, (char *)rsp->body_frag_start, MIN(rsp->body_frag_len, sizeof(new_version) - 1u));
        
        // Set default value to "no update"
        bool must_update_device_cfg = false;
        if (is_strings_same(new_version, device_cfg_ptr->version, DEV_CFG_MAX_VERSION_LENGTH))
        {
            LOG_INF("New version detected: %s", new_version);
            // Update the version in device configuration
            update_with_new_string(device_cfg_ptr->version, new_version, strlen(new_version));
            must_update_device_cfg = true;
        }
        // FIXME: Do the same for the etag field if needed.
        // If we need to store the response, copy the response to DisplayImage
        char new_etag[DEV_CFG_MAX_ETAG_LENGTH];
        ZERO_ARRAY(new_etag);
        // FIXME: copy the response etag string to new_etag.
        // You'll have to extract it from the key + value pair of the response body.
        // This is currently wrong, but a placeholder
        strncpy(new_etag, (char *)rsp->body_frag_start, MIN(rsp->body_frag_len, sizeof(new_etag) - 1u));
        if (is_strings_same(new_etag, device_cfg_ptr->last_etag, DEV_CFG_MAX_ETAG_LENGTH))
        {
            LOG_INF("New etag detected: %s", new_etag);
            // Update the etag in device configuration
            update_with_new_string(device_cfg_ptr->last_etag, new_etag, strlen(new_etag));
            must_update_device_cfg = true;
        }
        
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
        api_client_request_udpate("device", device_cfg_ptr, response_callback, DisplayImage, sizeof(DisplayImage));
        
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
