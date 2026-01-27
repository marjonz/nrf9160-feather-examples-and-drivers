#ifndef DEVICE_CFG_H
#define DEVICE_CFG_H

#include <stdint.h>

#define DEV_CFG_MAX_ETAG_LENGTH             (50u)
#define DEV_CFG_MAX_SECRET_LENGTH           (40u)
#define DEV_CFG_MAX_URL_LENGTH              (50u)

typedef struct 
{
    uint32_t device_id;
    char last_etag[DEV_CFG_MAX_ETAG_LENGTH];
    char api_secret[DEV_CFG_MAX_SECRET_LENGTH];
    char api_url[DEV_CFG_MAX_URL_LENGTH];
} device_cfg_t;


// TODO: Pull from the filesystem, currently unused.
//const char * api_secret = "super-secret-key";  // Store securely in NVS ideally

#endif