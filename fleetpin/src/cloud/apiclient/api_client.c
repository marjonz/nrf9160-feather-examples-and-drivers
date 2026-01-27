#include "api_client.h"

#include "cloud/authentication/auth.h"
#include "lib/macro.h"
#include "zephyr/kernel.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define MAX_URL_LENGTH  (64U)
static char url_buffer[MAX_URL_LENGTH] = {0};

api_client_result_t api_client_request_udpate(const char * target_url_endpoint, api_device_parameters_t params)
{
    api_client_result_t result;

    ZERO_ARRAY(url_buffer);
    snprintf(url_buffer, sizeof(url_buffer), "%s\?device=%" PRIu32 "&auth=false", target_url_endpoint, params.device_id);

    int64_t current_up_time = k_uptime_get();
    char message[MAX_URL_LENGTH] = {0};
    ZERO_ARRAY(message);
    snprintf(message, sizeof(message), "%" PRIu32 ":" "%" PRIi64 ":%s:%" PRIi64, 
        params.device_id, current_up_time, params.last_etag, current_up_time);

    char * hmac_result = auth_generate_hmac(message, strlen(message), current_up_time);
    return result;
}