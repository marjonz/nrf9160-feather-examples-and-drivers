#include "api_client.h"

#include "macro.h"

#define MAX_URL_LENGTH  (64U)
static char url_buffer[MAX_URL_LENGTH] = {0};

api_client_result_t api_client_request_udpate(const char * target_url_endpoint)
{
    api_client_result_t result;

    ZERO_ARRAY(url_buffer);
    uint16_t device_id = 1; // FIXME: Test value
    snprintf(url_buffer, sizeof(url_buffer), "%s\?device=%d&auth=false", target_url_endpoint, device_id);

    return result;
}