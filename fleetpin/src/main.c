/*
 * Copyright (c) 2020 Circuit Dojo LLC
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/pm/device.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

/* nRF Libraries */
#include <modem/nrf_modem_lib.h>
#include <modem/lte_lc.h>
#include <modem/modem_info.h>

#include <modem/nrf_modem_lib.h>
#include <nrf_modem_at.h>

/* Local */
#include "cloud/cloud.h"
#include "date_time.h"
#include "epaper/epaper.h"
#include "flash/flash_fs.h"

#include "lib/macro.h"
#include "version.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#ifndef CONFIG_APN_DEFAULT_VALUE
#   define CONFIG_APN_DEFAULT_VALUE "onemondo"
#endif

#define CONFIG_RETRY_DELAY_MINUTES 1

/* Timer */
static void timeout_handler(struct k_timer *timer_id);
K_TIMER_DEFINE(timer, timeout_handler, NULL);

/* Thread control */
K_SEM_DEFINE(thread_sem, 0, 1);
K_SEM_DEFINE(lte_connected, 0, 1);

/* Variables */
// Declare a static image buffer for the ePaper display
// Each byte represents 8 horizontal pixels.
#define MAX_WIDTH   800u
#define MAX_HEIGHT  480u
#define MAX_IMAGE_SIZE ((MAX_WIDTH/8u) * MAX_HEIGHT)
//Create a new image cache
static uint8_t DisplayImage[MAX_IMAGE_SIZE] = {0};
static size_t current_display_image_index = 0u;

static struct device_data data = {
    .do_something = true,
};

void cloud_cb(struct device_data *p_data)
{
    LOG_INF("Cloud callback");

    /* TODO: handle data here */
}

static void timeout_handler(struct k_timer *timer_id)
{
    LOG_INF("Main loop going back to sleep");
    k_sem_give(&thread_sem);
}

static void lte_handler(const struct lte_lc_evt *evt)
{
    switch (evt->type) {
    case LTE_LC_EVT_NW_REG_STATUS:
        if ((evt->nw_reg_status == LTE_LC_NW_REG_REGISTERED_HOME) ||
            (evt->nw_reg_status == LTE_LC_NW_REG_REGISTERED_ROAMING)) 
        {
            LOG_INF("LTE connected to network.");
            k_sem_give(&lte_connected);
        }
        break;

    default:
        break;
    }
}

/* Initialization of AUX pin */
#if defined(CONFIG_BOARD_CIRCUITDOJO_FEATHER_NRF9151)
#define AUXANTCFG_ENABLE "AT\%XANTCFG=1"

static inline void set_apn_name(const char * apn_name)
{
#   define GET_APN_CONFIG "AT+CGDCONT?"
#   define SET_APN_CONFIG "AT+CGDCONT=1,\"IP\","
    char response[5u] = {0};
    char at_command[60] = {0};
    ZERO_ARRAY(response);
    ZERO_ARRAY(at_command);

    // Build the command to send
    snprintf(at_command, sizeof(at_command), "%s\"%s\"\n", SET_APN_CONFIG, apn_name);

    LOG_DBG("Sending APN config: %sn", at_command);
    int err = nrf_modem_at_cmd(response, sizeof(response), "%s", at_command);
    if (err)
    {
        LOG_ERR("Failed to set APN configuration (err: %d)", err);
    }
    LOG_DBG("APN config Response: %s", response);
}

NRF_MODEM_LIB_ON_INIT(aux_init_hook, on_modem_lib_init, NULL);
static void on_modem_lib_init(int ret, void *ctx)
{
    ARG_UNUSED(ctx);

    if (ret != 0)
    {
        return;
    }

    LOG_DBG("*** Setting configuration: %s ***\n", AUXANTCFG_ENABLE);
    int err = nrf_modem_at_printf("%s", AUXANTCFG_ENABLE);
    if (err)
    {
        LOG_ERR("Failed to set configuration (err: %d)", err);
    }

    set_apn_name(CONFIG_APN_DEFAULT_VALUE);
}
#endif

static void push_http_bmp_into_epaper_buffer(bool is_first_chunk_of_data, 
    const uint8_t * const http_bmp_data, size_t http_data_len)
{
    if (is_first_chunk_of_data)
    {
        current_display_image_index = 0u;
    }

    int status = epaper_store_data_to_buffer(DisplayImage, current_display_image_index, sizeof(DisplayImage), 
        http_bmp_data, http_data_len);
    if (status == -EDOM || status == -EINVAL)
    {
        LOG_ERR("Failed to store http bmp data.");
    }
    current_display_image_index = status;
}

static void store_epaper_buffer_to_file(const char * filename, const uint8_t * const epaper_buffer)
{
    int result = flash_fs_write_file_to_fs("this_filename_001", DisplayImage, sizeof(DisplayImage));
    if (result != 0)
    {
        LOG_ERR("Failed to write epaper bitmap to file.");
    }
}

/* Define the stack sizes for the threads */
#define STACK_SIZE                  1024
#define CLOUD_THREAD_STACK_SIZE     (2*STACK_SIZE)
#define FLASH_THREAD_STACK_SIZE     (3*STACK_SIZE)
#define EPAPER_THREAD_STACK_SIZE    (2*STACK_SIZE)
/* Define thread priorities (lower number = higher priority) */
#define CLOUD_PRIORITY 7 
#define EPAPER_PRIORITY 8
#define FLASH_FS_PRIORITY 9

static void get_time_now(void)
{
    int64_t time_now = 0;
    int err = date_time_now(&time_now);
    if (err < 0)
    {
        LOG_ERR("Failed to get date/time. Err: %i", err);
        return;
    }
    LOG_DBG("Time now: %" PRIi64 "ms", time_now);
}

static void date_time_handler(const struct date_time_evt *evt) 
{
    switch (evt->type) 
    {
        case DATE_TIME_OBTAINED_NTP:
            LOG_DBG("Time obtained from NTP\n");
            // Perform actions with time here
            break;
        case DATE_TIME_OBTAINED_MODEM:
            LOG_DBG("Time obtained from Modem\n");
            break;
        default:
            break;
    }
}

/* Thread entry function for the first thread (e.g., blinking an LED) */
void cloud_thr(void *p1, void *p2, void *p3) 
{

    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    
    // Register date time handler
    date_time_register_handler(date_time_handler);

    /* Register callback handler to handler LTE events */
    lte_lc_register_handler(lte_handler);

    /* Init modem lib */
    int err = nrf_modem_lib_init();
    if (err < 0)
    {
        LOG_ERR("Failed to init modem lib. (err: %i)", err);
        return;
    }

    /* Cloud init */
    err = cloud_init(cloud_cb);
    if (err < 0)
    {
        LOG_ERR("Unable to set callback. Err: %i", err);
        return;
    }

    /* Power saving is turned on */
    lte_lc_psm_req(false);

    /* Connect */
    LOG_INF("Connecting to LTE...");
    err = lte_lc_connect();
    /* Wait indefinitely (or add timeout?) */
    k_sem_take(&lte_connected, K_FOREVER);
    if (err < 0)
    {
        LOG_ERR("Failed to connect. Err: %i", err);
        return;
    }

    get_time_now();

    /* Wait for a while, because with IPv4v6 PDN the IPv6 activation takes a bit more time. */
	k_sleep(K_SECONDS(1));

    LOG_INF("Safe to use sockets now. LTE is connected.");

    get_time_now();

    /* Start timer to periodically wake the device and publish data */
    k_timer_start(&timer, K_MINUTES(CONFIG_DEFAULT_DELAY), K_MINUTES(CONFIG_DEFAULT_DELAY));

    /* Allow for instant publish */
    k_sem_give(&thread_sem);

    while (1)
    {
        k_sem_take(&thread_sem, K_FOREVER);

        /* Publish and sleep .. */
        err = cloud_publish(&data);
        if (err < 0)
        {
            LOG_ERR("Unable to publish. Err: %i", err);
            // Stop the current timer and attempt to republish in CONFIG_RETRY_DELAY_MINUTES minutes
            k_timer_stop(&timer);
            k_timer_start(&timer, K_MINUTES(CONFIG_RETRY_DELAY_MINUTES), K_MINUTES(CONFIG_RETRY_DELAY_MINUTES));
        }
        else
        {
            LOG_INF("Cloud publish successful.");
            // Stop the current timer and republish in the normal default delay interval
            k_timer_stop(&timer);
            k_timer_start(&timer, K_MINUTES(CONFIG_DEFAULT_DELAY), K_MINUTES(CONFIG_DEFAULT_DELAY));
        }
    }
}

/* Thread entry function for the third thread */
void epaper_thr(void *p1, void *p2, void *p3) 
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    int err = epaper_init(DisplayImage);
    if (err < 0)
    {
        LOG_ERR("Failed to init epaper. (err: %i)", err);
        return;
    }
    k_sleep(K_SECONDS(1));

    // ePAPER test display
    epaper_display_test();
}

/* Thread entry function for the fourth thread */
void flash_fs_thr(void *p1, void *p2, void *p3) 
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    flash_fs_init();
}

/* Define the threads using K_THREAD_DEFINE */
K_THREAD_DEFINE(cloud_thread_id, CLOUD_THREAD_STACK_SIZE, cloud_thr, NULL, NULL, NULL, CLOUD_PRIORITY, 0, 0);
K_THREAD_DEFINE(epaper_thread_id, EPAPER_THREAD_STACK_SIZE, epaper_thr, NULL, NULL, NULL, EPAPER_PRIORITY, 0, 0);
K_THREAD_DEFINE(flash_fs_thread_id, FLASH_THREAD_STACK_SIZE, flash_fs_thr, NULL, NULL, NULL, FLASH_FS_PRIORITY, 0, 0);

int main(void)
{
    LOG_INF("Fleetpin Project. Board: %s", CONFIG_BOARD);

    /* The main thread can also perform work or go to sleep */
    while (1) 
    {
        k_sleep(K_FOREVER); /* Sleep the main thread indefinitely */
    }
}
