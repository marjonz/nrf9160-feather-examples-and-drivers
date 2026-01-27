#include "device_cfg.h"

static device_cfg_t device_configuration = {0};

void device_cfg_init(void)
{
    device_configuration.device_id = CONFIG_DEVICE_ID;
}

device_cfg_t * device_cfg_get(void)
{
    return &device_configuration;
}