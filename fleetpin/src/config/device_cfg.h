#ifndef DEVICE_CFG_H
#define DEVICE_CFG_H

#include <stdint.h>

#define DEV_CFG_MAX_ETAG_LENGTH             (50u)
#define DEV_CFG_MAX_SECRET_LENGTH           (40u)
#define DEV_CFG_MAX_URL_LENGTH              (50u)
#define DEV_CFG_MAX_VERSION_LENGTH          (10u)
typedef struct 
{
    uint32_t device_id;
    char last_etag[DEV_CFG_MAX_ETAG_LENGTH];
    char api_secret[DEV_CFG_MAX_SECRET_LENGTH];
    char version[DEV_CFG_MAX_VERSION_LENGTH];
} device_cfg_t;


// FIXME: Pull from the filesystem, currently unused. Maybe add to the structure above so that it can be saved?
//const char * api_secret = "super-secret-key";  // Store securely in NVS ideally

/**
 * #@brief Initialise the device configuration.
 *          Reads from the filesystem if available.
 */
void device_cfg_init(void);

/**
 * #@brief Fetch the current device configuration.
 */
device_cfg_t * device_cfg_get(void);

/**
 * #@brief Updates the current device configuration and writes to the filesystem.
 */
int device_cfg_set(const device_cfg_t * new_value);

#endif