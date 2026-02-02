#include "device_cfg.h"

#include "flash/flash_fs.h"
#include "lib/macro.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(device_cfg, LOG_LEVEL_DBG);

static device_cfg_t device_configuration = {0};

void device_cfg_init(void)
{
    // Set default values in case the file does not exists
    device_configuration.device_id = CONFIG_DEVICE_ID;
    ZERO_ARRAY(device_configuration.last_etag);
    ZERO_ARRAY(device_configuration.api_secret);
    ZERO_ARRAY(device_configuration.version);

    if (flash_fs_is_file_exist("device_config"))
    {
        // Fetch the values
        uint8_t * device_config_ptr = (uint8_t *) &device_configuration;
        int err = flash_fs_read_file_to_fs("device_config", device_config_ptr, sizeof(device_configuration));
        if (err != 0)
        {
            LOG_ERR("Failed to read device config from file.");
        }
    }
}

device_cfg_t * device_cfg_get(void)
{
    return &device_configuration;
}

int device_cfg_set(const device_cfg_t * new_value)
{
    device_configuration = *new_value;
    const uint8_t * const device_config_ptr = (const uint8_t * const ) &device_configuration;
    int result = flash_fs_write_file_to_fs("device_config", device_config_ptr, sizeof(device_configuration));
    if (result != 0)
    {
        LOG_ERR("Failed to write device config to file.");
    }
    return result;
}