/*
 * Copyright (c) 2020 Circuit Dojo LLC
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _CLOUD_H
#define _CLOUD_H

#include "apiclient.h"

struct device_data
{
    bool do_something;
};

typedef void (*cloud_callback_t)(const apic_reponse_data_t data, const struct http_response *rsp);

/**
 * @brief Publish data to the cloud
 *
 * @param data Pointer to device CJSON data
 * @return int 0 if successful, negative errno otherwise
 *
 */
int cloud_publish_cjson(struct device_data *data);

/**
 * @brief Initialize the cloud
 *
 * @param callback Callback function to be called when data is received
 * @return int 0 if successful, negative errno otherwise
 *
 */
int cloud_init(cloud_callback_t callback);

#endif