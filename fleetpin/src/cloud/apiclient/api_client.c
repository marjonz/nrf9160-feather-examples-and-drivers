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
#define MAX_HEADER_FIELD_LEN            (256u)//(20U)
#define UNIX_MS_JAN_1_2021              (1609459200)
#define MAX_HTTP_HEADER_INFO_LENGTH     (1024U)

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
    hdr_connection, 
    hdr_NULL, 
    hdr_max_len // MUST BE LAST
} header_index;

// Static const stores to the flash (code space) instead of ram space
static const char * user_agent_field_value = "Fleetpin EPD Client/1.0";
static const char * auth_version = "v1";
static const char * http_headers[hdr_max_len][MAX_HEADER_FIELD_LEN] = 
{
    [hdr_auth_version]      = {"X-auth-version: "}, 
    [hdr_device_id]         = {"X-device-id: "}, 
    [hdr_timestamp]         = {"X-timestamp: "}, 
    [hdr_signature]         = {"X-signature: "}, 
    [hdr_user_agent]        = {"User-agent: "}, 
    [hdr_none_match]        = {"If-none-match: "},
    [hdr_config_version]    = {"X-config-version: "},
    [hdr_firmware_build]    = {"X-firmware-build: "},
    [hdr_connection]        = {"Connection: "},
};

static char http_header_info[hdr_max_len][MAX_HEADER_FIELD_LEN] = {0};

static char *http_header_req[hdr_max_len] = {0}; 

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

    LOG_DBG("Date/time: %" PRIi64 "millis, %" PRIi64 "s", current_time, (current_time / 10u));
    *current_timestamp = (current_time / 1000u);
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

    uint64_t current_time = 0;
    if (!is_current_time_valid(&current_time))
    {
        snprintf(result.error_message, sizeof(result.error_message), "Time not synchronized");
        return result;
    }

    // Hash request body (empty for GET)
    char * body_hash = auth_hash_request_body(NULL, 0);
    if (body_hash != NULL )
    {
        LOG_DBG("[APIClient] Body hash (empty): %s, len %d\n", body_hash, strlen(body_hash));
    }
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
    else 
    {
        printf("Canonical String (length %d): \n%s  \n", strlen(canonical), canonical); 
    }
    // Generate signature
    char * hmac_signature = auth_generate_hmac(canonical, config->api_secret);
    printf("HMAC Signature: %s\n", hmac_signature);

    //Temp buffer to create strings from ints
    char temp_buffer[256u];
    ZERO_ARRAY(temp_buffer);

    //Loop through to init the header info
    for (int i = 0; i < (hdr_max_len - 1); i++) 
    {
        snprintf(http_header_info[i], sizeof(http_header_info[i]), http_headers[i][0]); 
    }

    //Method - http header info array is "static", we form each string for the header then assign a pointer to the beginning of each string
    strncat(&(http_header_info[hdr_auth_version][0]), auth_version, sizeof(http_header_info[hdr_auth_version]) - strlen(auth_version) - 1); 
    
    snprintf(temp_buffer, sizeof(temp_buffer), "%" PRIu32 "", config->device_id);
    strncat(&(http_header_info[hdr_device_id][0]), temp_buffer, sizeof(http_header_info[hdr_device_id]) - strlen(temp_buffer) - 1); 
    ZERO_ARRAY(temp_buffer);

    snprintf(temp_buffer, sizeof(temp_buffer), "%" PRIi64 "", current_time);
    strncat(&(http_header_info[hdr_timestamp][0]), temp_buffer, sizeof(http_header_info[hdr_timestamp]) - strlen(temp_buffer) - 1); 
    ZERO_ARRAY(temp_buffer); 

    strncat(&(http_header_info[hdr_signature][0]), hmac_signature, sizeof(http_header_info[hdr_signature]) - strlen(hmac_signature) - 1); 

    strncat(&(http_header_info[hdr_user_agent][0]), user_agent_field_value, sizeof(http_header_info[hdr_user_agent]) - strlen(user_agent_field_value) - 1);

    strncat(&(http_header_info[hdr_none_match][0]), config->last_etag, sizeof(http_header_info[hdr_none_match]) - strlen(config->last_etag) - 1);

    strncat(&(http_header_info[hdr_config_version][0]), config->version, sizeof(http_header_info[hdr_config_version]) - strlen(config->version) - 1);

    strncat(&(http_header_info[hdr_firmware_build][0]), VERSION, sizeof(http_header_info[hdr_firmware_build]) - strlen(VERSION) - 1);

    strncat(&(http_header_info[hdr_connection][0]), "close", sizeof(http_header_info[hdr_connection]) - strlen("close") - 1);

    //Add \r\n to each string 
    for (int i = 0; i < (hdr_max_len - 1); i++) 
    {
        strncat(&(http_header_info[i][0]), "\r\n", sizeof(http_header_info[i]) - strlen("\r\n") - 1);
    }
    

    for (int i = 0; i < (hdr_max_len - 1); i++) 
    {
        http_header_req[i] = http_header_info[i]; 
        printf("HTTP Headers[%d]: %s", i, http_header_req[i]); 
    }

    //Needs to be a NULL terminated list for the http req 
    http_header_req[hdr_NULL] = NULL; 

    // Create the socket then send HTTP GET
    int err = cloud_get_from_endpoint(target_url_endpoint, http_header_req, response_handler,
                reponse_data_buffer, reponse_data_buffer_len);
    ARG_UNUSED(err);
    result.status_code = err;

    //Zero the info array for sending next time
    memset(http_header_info, 0, sizeof(http_header_info));

    printf("Return from cloud send"); 
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
    if (body_hash != NULL )
    {
        LOG_DBG("[APIClient] Body hash (empty): %s, len %d\n", body_hash, strlen(body_hash));
    }

    //Debug: check api secret has been written 
    LOG_DBG("API Secret: %s", config->api_secret); 

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
    else 
    {
        printf("Canonical String (length %d): \n%s  \n", strlen(canonical), canonical); 
    }

    // Generate signature
    char * hmac_signature = auth_generate_hmac(canonical, config->api_secret);
    printf("HMAC Signature: %s\n", hmac_signature);

    //Temp buffer to create strings from ints
    char temp_buffer[256u];
    ZERO_ARRAY(temp_buffer);

    //Loop through to init the header info
    for (int i = 0; i < (hdr_max_len - 1); i++) 
    {
        snprintf(http_header_info[i], sizeof(http_header_info[i]), http_headers[i][0]); 
    }

    //Method - http header info array is "static", we form each string for the header then assign a pointer to the beginning of each string
    strncat(&(http_header_info[hdr_auth_version][0]), auth_version, sizeof(http_header_info[hdr_auth_version]) - strlen(auth_version) - 1); 
    
    snprintf(temp_buffer, sizeof(temp_buffer), "%" PRIu32 "", config->device_id);
    strncat(&(http_header_info[hdr_device_id][0]), temp_buffer, sizeof(http_header_info[hdr_device_id]) - strlen(temp_buffer) - 1); 
    ZERO_ARRAY(temp_buffer);

    snprintf(temp_buffer, sizeof(temp_buffer), "%" PRIi64 "", current_time);
    strncat(&(http_header_info[hdr_timestamp][0]), temp_buffer, sizeof(http_header_info[hdr_timestamp]) - strlen(temp_buffer) - 1); 
    ZERO_ARRAY(temp_buffer); 

    strncat(&(http_header_info[hdr_signature][0]), hmac_signature, sizeof(http_header_info[hdr_signature]) - strlen(hmac_signature) - 1); 

    strncat(&(http_header_info[hdr_user_agent][0]), user_agent_field_value, sizeof(http_header_info[hdr_user_agent]) - strlen(user_agent_field_value) - 1);

    strncat(&(http_header_info[hdr_config_version][0]), config->version, sizeof(http_header_info[hdr_config_version]) - strlen(config->version) - 1);

    strncat(&(http_header_info[hdr_connection][0]), "close", sizeof(http_header_info[hdr_connection]) - strlen("close") - 1);

    //Add \r\n to each string 
    for (int i = 0; i < (hdr_max_len - 1); i++) 
    {
        strncat(&(http_header_info[i][0]), "\r\n", sizeof(http_header_info[i]) - strlen("\r\n") - 1);
    }
    

    for (int i = 0; i < (hdr_max_len - 1); i++) 
    {
        http_header_req[i] = http_header_info[i]; 
        printf("HTTP Headers[%d]: %s", i, http_header_req[i]); 
    }

    //Needs to be a NULL terminated list for the http req 
    http_header_req[hdr_NULL] = NULL; 

    int err = cloud_get_from_endpoint(target_url_endpoint, http_header_req, response_handler,
                reponse_data_buffer, reponse_data_buffer_len);
    ARG_UNUSED(err);
    result.status_code = err;
    return result;
}