#ifndef API_CLIENT_H
#define API_CLIENT_H

#include "config/device_cfg.h"

#include <stdbool.h>
#include <stdint.h>

#define API_CLIENT_CFG_VER_LENGTH   (20u)
#define API_CLIENT_ERR_MSG_LENGTH   (50u)
#define API_CLIENT_MAX_ETAG_LENGTH  (DEV_CFG_MAX_ETAG_LENGTH)
typedef struct 
{
    bool success;
    bool has_update;
    int status_code;
    char last_etag[API_CLIENT_MAX_ETAG_LENGTH];
    uint8_t * ruc_bitmap;
    char config_version[API_CLIENT_CFG_VER_LENGTH];
    uint32_t sleepForSeconds;
    char error_message[API_CLIENT_ERR_MSG_LENGTH];
} api_client_result_t;


/*************************************************************************************************
 * @brief Build and send an HTTP GET to the given endpoint
 * @param target_url_endpoint - http endpoint to send the GET request to.
 * @param params - the device parameters to be used to build the message to send.
 * @return the result of the operation.
 */
api_client_result_t api_client_request_udpate(const char * target_url_endpoint, device_cfg_t config);

#endif