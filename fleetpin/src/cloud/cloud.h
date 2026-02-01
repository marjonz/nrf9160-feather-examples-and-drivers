/*
 * Copyright (c) 2020 Circuit Dojo LLC
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _CLOUD_H
#define _CLOUD_H

#include <zephyr/net/http/client.h>

struct device_data
{
    bool do_something;
};

/**
 * @brief Publish data to the cloud
 *
 * @param data Pointer to device data
 * @return int 0 if successful, negative errno otherwise
 *
 */
//int cloud_publish(struct device_data *data);

/**
 * @brief Send a GET request to the URL endpoint
 * @param callback_fn - callback function to execute on response from endpoint.
 * @return int 0 if successful, see errno for error information.
 */
int cloud_get_from_endpoint(const char * url_endpoint, const char * http_headers,
    http_response_cb_t callback_fn, 
    uint8_t * reponse_data_buffer, size_t reponse_data_buffer_len);

/**
 * @brief Initialize the cloud
 *
 * @param callback Callback function to be called when data is received
 * @return int 0 if successful, negative errno otherwise
 *
 */
int cloud_init(void (*callback)(struct device_data *data));

#endif