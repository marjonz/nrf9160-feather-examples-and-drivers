#include "apiclient.h"

#include <stdio.h>

#define MAX_HEADER_LEN      50
#define MAX_NUM_HEADERS     10
static char * headers[MAX_NUM_HEADERS][MAX_HEADER_LEN] = {0};

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
    while (*p_data != NULL)
    {
        printf("[%d]%s",i,*p_data[i]);
        i++;
    }

    return result;
}