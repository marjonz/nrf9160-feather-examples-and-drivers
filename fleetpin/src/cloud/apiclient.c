#include "apiclient.h"

#include <stdio.h>
#include <string.h>

#define MAX_HEADER_LEN      50
#define MAX_NUM_HEADERS     10

// String nonce = String(millis());
// String message = cfg->getDeviceId() + ":" + nonce + ":" + cfg->getAPILastETag() + ":" + nonce;
const char * nonce = "1234";
const char * device_id = "1";
const char * message = "1 : 1234 : W/\"4d-gBCcUSSHqLZELfG9QkjI/6bogiE\" : 1234";
static char headers[][MAX_HEADER_LEN] = 
{
    "User-Agent : Fleetpin EPD Client/1.0",
    "X-Nonce : 1234",
    "X-Device-ID: 1",
    "X-Signature : blah", // HMAC encryted signature 
    "If-None-Match :  W/\"4d-gBCcUSSHqLZELfG9QkjI/6bogiE\"",
};

#if 0
const char * read_all_headers(const struct device_data *p_data) 
{
  while (http.headerAvailable()) {
    String name = http.readHeaderName();
    String value = http.readHeaderValue();
    headers[name] = value;
    printf("[APIClient] Header %s: %s\n", name.c_str(), value.c_str());
  }
  return headers;
}
#endif

api_client_update_result_t apic_request_update(const apic_reponse_data_t p_data, const struct http_response *rsp)
{
    api_client_update_result_t result;
    // Dummy implementation for illustration
    result.success = true;
    result.has_update = false;
    result.last_etag = "dummy-etag";
    result.ruc_bmp = NULL;

    /* Find out how to look for ETag in the p_data response. */
    // if (headers.find("ETag") != headers.end()) 
    // {
    //     LOG_DBG("[APIClient] Last ETag %s\n", headers["ETag"]);
    //     result.lastETag = headers["ETag"];
    // }
    uint16_t i = 0u;
    char * data_ptr = (char *)p_data;
    memset(headers, 0, sizeof(headers));
    memcpy((char *)&headers[0][0], (char *)data_ptr, MAX_HEADER_LEN);

    // Look at what the response looks like.
    while (data_ptr != NULL)
    {
        printf("[%d]: %s", i, &headers[0][0]);
        data_ptr += MAX_HEADER_LEN;
        i++;
        memset(headers, 0, sizeof(headers));
        memcpy((char *)&headers[0][0], (char *)data_ptr, MAX_HEADER_LEN);
    }

    return result;
}