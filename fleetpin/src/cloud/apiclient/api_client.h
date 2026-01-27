#ifndef API_CLIENT_H
#define API_CLIENT_H

#include <stdbool.h>
#include <stdint.h>

#define API_CLIENT_MAX_ETAG_LENGTH  (50u)
typedef struct {
    bool success;
    bool has_update;
    char last_etag[API_CLIENT_MAX_ETAG_LENGTH];
    uint8_t * ruc_bitmap;
} api_client_result_t;



#endif