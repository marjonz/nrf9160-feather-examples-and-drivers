#include "api_client.h"

#include "cloud/authentication/auth.h"
#include "date_time.h"
#include "lib/macro.h"
#include "zephyr/kernel.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define MAX_URL_LENGTH          (64U)
#define UNIX_MS_JAN_1_2021      (1609459200)
static char url_buffer[MAX_URL_LENGTH] = {0};

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
    current_timestamp = current_time;
    return true;
}

api_client_result_t api_client_request_udpate(const char * target_url_endpoint, api_device_parameters_t params)
{
    api_client_result_t result;

    int64_t current_time = 0;
    if (!is_current_time_valid(&current_time))
    {
        result.has_update = false;
        result.success = false;
        result.status_code = -1;
        snprintf(result.error_message, sizeof(result.error_message), "Time not synchronized");
        return result;
    }

    // Hash request body (empty for GET)
    char * body_hash = auth_hash_request_body(NULL, 0);
    LOG_DBG("[APIClient] Body hash (empty): %s\n", body_hash);

    // Build canonical string
    char * canonical = auth_build_canonical_string(
    "v1",
    "1",    // FIXME: Should be device id
    current_time,
    "GET",
    //resource.c_str(),
    target_url_endpoint, // FIXME: Should this only be the endpoint not including the domain?
    body_hash
    );

    ZERO_ARRAY(url_buffer);
    snprintf(url_buffer, sizeof(url_buffer), "%s\?device=%" PRIu32 "&auth=false", target_url_endpoint, params.device_id);

    int64_t current_up_time = k_uptime_get();
    char message[MAX_URL_LENGTH] = {0};
    ZERO_ARRAY(message);
    snprintf(message, sizeof(message), "%" PRIu32 ":" "%" PRIi64 ":%s:%" PRIi64, 
        params.device_id, current_up_time, params.last_etag, current_up_time);

    // TODO: Pull from the filesystem, currently unused.
    const char * deviceSecret = "super-secret-key";  // Store securely in NVS ideally
    char * hmac_result = auth_generate_hmac(message, deviceSecret);
    return result;
}