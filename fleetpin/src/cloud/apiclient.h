#ifndef APICLIENT_H
#define APICLIENT_H

#include <zephyr/net/http/client.h>

#include <stdbool.h>
#include <stdint.h>

typedef void * apic_reponse_data_t;

typedef struct api_update_result 
{
    bool success;
    bool has_update;
    const char * last_etag;
    unsigned char* ruc_bmp;
} api_client_update_result_t;


/*********************************************************
 * @brief Request an update from the API client
 * @return  api_client_update_result_t structure with the result
 */
api_client_update_result_t apic_request_update(const apic_reponse_data_t p_data, const struct http_response *rsp);

#endif