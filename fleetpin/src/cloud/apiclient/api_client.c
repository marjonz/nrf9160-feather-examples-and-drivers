#include "api_client.h"

#include "cloud/authentication/auth.h"
#include "cloud/apiclient/api_client.h"
#include "cloud/cloud.h"
#include "date_time.h"
#include "lib/macro.h"
#include "version.h"
#include "zephyr/kernel.h"
#include <zephyr/logging/log.h>

#include <inttypes.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

LOG_MODULE_REGISTER(api_client, LOG_LEVEL_DBG);

#define MAX_DEVICE_CONFIG_STRING_LEN    (10u) // Number of characters in UINT32_MAX
#define MAX_HEADER_FIELD_LEN            (20U)
#define UNIX_MS_JAN_1_2021              (1609459200)
#define MAX_HTTP_HEADER_INFO_LENGTH     (1024U)
static char http_header_info[MAX_HTTP_HEADER_INFO_LENGTH] = {0};

enum 
{
    hdr_auth_version = 0u,
    hdr_device_id,
    hdr_timestamp,
    hdr_signature,
    hdr_user_agent,
    hdr_none_match,
    hdr_config_version,
    hdr_firmware_build,
    hdr_max_len // MUST BE LAST
} header_index;

// Static const stores to the flash (code space) instead of ram space
static const char * user_agent_field_value = "Fleetpin EPD Client/1.0";
static const char * auth_version = "v1";
static const char * http_headers[hdr_max_len][MAX_HEADER_FIELD_LEN] = 
{
    [hdr_auth_version]      = {"X-Auth-Version: "}, 
    [hdr_device_id]         = {"X-Device-ID: "}, 
    [hdr_timestamp]         = {"X-Timestamp: "}, 
    [hdr_signature]         = {"X-Signature: "}, 
    [hdr_user_agent]        = {"User-Agent: "}, 
    [hdr_none_match]        = {"If-None-Match: "},
    [hdr_config_version]    = {"X-Config-Version: "},
    [hdr_firmware_build]    = {"X-Firmware-Build: "},
};

static bool is_current_time_valid(int64_t * current_timestamp)
{
    int64_t current_time = 0; 
    // Returns UNIX time since Jan 1 1970 if successful
    int err = date_time_now(&current_time);
    if ((err < 0) || (current_time <= 0) || (current_time < UNIX_MS_JAN_1_2021))
    {
        LOG_ERR("Date/time still invalid. Err: %i", err);
        current_timestamp = 0;
        return false;
    }

    LOG_DBG("Date/time: %" PRIi64, current_time);
    *current_timestamp = current_time;
    return true;
}

api_client_result_t api_client_request_udpate(const char * target_url_endpoint, 
    const device_cfg_t * config, http_response_cb_t response_handler,
    uint8_t * reponse_data_buffer, size_t reponse_data_buffer_len)
{
    api_client_result_t result = 
    {
        .has_update = false,
        .success = false,
        .status_code = -1,
    };

    int64_t current_time = 0;
    if (!is_current_time_valid(&current_time))
    {
        snprintf(result.error_message, sizeof(result.error_message), "Time not synchronized");
        return result;
    }

    // Hash request body (empty for GET)
    char * body_hash = auth_hash_request_body(NULL, 0);
    LOG_DBG("[APIClient] Body hash (empty): %s\n", body_hash);

    // Build canonical string
    char device_id_string[MAX_DEVICE_CONFIG_STRING_LEN];
    ZERO_ARRAY(device_id_string);
    snprintf(device_id_string, sizeof(device_id_string),"%" PRIu32, config->device_id);
    ARG_UNUSED(device_id_string);
    char * canonical = auth_build_canonical_string(
        "v1",
        device_id_string,
        current_time,
        "GET",
        target_url_endpoint,
        body_hash
    );
    if (canonical == NULL)
    {
        LOG_ERR("Failed to build canonical string. Aborting http request.");
        return result;
    }
    // Generate signature
    char * hmac_signature = auth_generate_hmac(canonical, config->api_secret);
    printf("HMAC Signature: %s\n", hmac_signature);

    // HTTP GET from target_url_endpoint, build header info first
    // Build HTTP request headers, this will be appended to the http request structure.
    ZERO_ARRAY(http_header_info);
    snprintf(http_header_info, sizeof(http_header_info), "%s%s\r\n", *http_headers[hdr_auth_version], auth_version);
    char temp_buffer[100u];
    ZERO_ARRAY(temp_buffer);
    snprintf(temp_buffer, sizeof(temp_buffer), "%s%" PRIu32 "\r\n", *http_headers[hdr_device_id], config->device_id);
    strcat(http_header_info, temp_buffer);
    ZERO_ARRAY(temp_buffer);
    snprintf(temp_buffer, sizeof(temp_buffer), "%s%" PRIi64 "\r\n", *http_headers[hdr_timestamp], current_time);
    strcat(http_header_info, temp_buffer);
    ZERO_ARRAY(temp_buffer);
    snprintf(temp_buffer, sizeof(temp_buffer), "%s%s\r\n", *http_headers[hdr_signature], hmac_signature);
    strcat(http_header_info, temp_buffer);
    ZERO_ARRAY(temp_buffer);
    snprintf(temp_buffer, sizeof(temp_buffer), "%s%s\r\n", *http_headers[hdr_user_agent], user_agent_field_value);
    strcat(http_header_info, temp_buffer);
    ZERO_ARRAY(temp_buffer);
    snprintf(temp_buffer, sizeof(temp_buffer), "%s\"%s\"\r\n", *http_headers[hdr_none_match], config->last_etag);
    strcat(http_header_info, temp_buffer);
    ZERO_ARRAY(temp_buffer);
    snprintf(temp_buffer, sizeof(temp_buffer), "%s%s\r\n", *http_headers[hdr_config_version], config->version);
    strcat(http_header_info, temp_buffer);
    ZERO_ARRAY(temp_buffer);
    snprintf(temp_buffer, sizeof(temp_buffer), "%s%s\r\n", *http_headers[hdr_firmware_build], VERSION);
    strcat(http_header_info, temp_buffer);
    /* Don't keep connection open.. */
    strcat(http_header_info, "Connection: close\r\n");

    // Create the socket then send HTTP GET
    char * http_headers_ptr = http_header_info;
    char * http_headers_dbl_ptr = http_headers_ptr;
    int err = cloud_get_from_endpoint(target_url_endpoint, http_headers_dbl_ptr, response_handler,
                reponse_data_buffer, reponse_data_buffer_len);
    ARG_UNUSED(err);
    result.status_code = err;
    return result;
}

api_client_result_t api_client_fetch_config(const char * target_url_endpoint, 
    const device_cfg_t * config, http_response_cb_t response_handler,
    uint8_t * reponse_data_buffer, size_t reponse_data_buffer_len)
{
    api_client_result_t result = 
    {
        .has_update = false,
        .success = false,
        .status_code = -1,
    };

    int64_t current_time = 0;
    if (!is_current_time_valid(&current_time))
    {
        snprintf(result.error_message, sizeof(result.error_message), "Time not synchronized");
        return result;
    }

    // Hash request body (empty for GET)
    char * body_hash = auth_hash_request_body(NULL, 0);
    LOG_DBG("[APIClient] Body hash (empty): %s\n", body_hash);

    // Build canonical string
    char device_id_string[MAX_DEVICE_CONFIG_STRING_LEN];
    ZERO_ARRAY(device_id_string);
    snprintf(device_id_string, sizeof(device_id_string),"%" PRIu32, config->device_id);
    ARG_UNUSED(device_id_string);
    char * canonical = auth_build_canonical_string(
        "v1",
        device_id_string,
        current_time,
        "GET",
        target_url_endpoint,
        body_hash
    );
    if (canonical == NULL)
    {
        LOG_ERR("Failed to build canonical string. Aborting http request.");
        return result;
    }
    // Generate signature
    char * hmac_signature = auth_generate_hmac(canonical, config->api_secret);
    printf("HMAC Signature: %s\n", hmac_signature);

    // HTTP GET from target_url_endpoint, build header info first
    // Build HTTP request headers, this will be appended to the http request structure.
    ZERO_ARRAY(http_header_info);
    snprintf(http_header_info, sizeof(http_header_info), "%s%s\r\n", *http_headers[hdr_auth_version], auth_version);
    char temp_buffer[100u];
    ZERO_ARRAY(temp_buffer);
    snprintf(temp_buffer, sizeof(temp_buffer), "%s%" PRIu32 "\r\n", *http_headers[hdr_device_id], config->device_id);
    strcat(http_header_info, temp_buffer);
    ZERO_ARRAY(temp_buffer);
    snprintf(temp_buffer, sizeof(temp_buffer), "%s%" PRIi64 "\r\n", *http_headers[hdr_timestamp], current_time);
    strcat(http_header_info, temp_buffer);
    ZERO_ARRAY(temp_buffer);
    snprintf(temp_buffer, sizeof(temp_buffer), "%s%s\r\n", *http_headers[hdr_signature], hmac_signature);
    strcat(http_header_info, temp_buffer);
    ZERO_ARRAY(temp_buffer);
    snprintf(temp_buffer, sizeof(temp_buffer), "%s%s\r\n", *http_headers[hdr_user_agent], user_agent_field_value);
    strcat(http_header_info, temp_buffer);
    ZERO_ARRAY(temp_buffer);
    snprintf(temp_buffer, sizeof(temp_buffer), "%s%s\r\n", *http_headers[hdr_firmware_build], VERSION);
    strcat(http_header_info, temp_buffer);
    /* Don't keep connection open.. */
    strcat(http_header_info, "Connection: close\r\n");

    // Create the socket then send HTTP GET
    char * http_headers_ptr = http_header_info;
    char * http_headers_dbl_ptr = http_headers_ptr;
    int err = cloud_get_from_endpoint(target_url_endpoint, http_headers_dbl_ptr, response_handler,
                reponse_data_buffer, reponse_data_buffer_len);
    ARG_UNUSED(err);
    result.status_code = err;
    return result;
}