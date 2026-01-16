/*
 * Copyright (c) 2020 Circuit Dojo LLC
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/pm/device.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main);

/* nRF Libraries */
#include <modem/nrf_modem_lib.h>
#include <modem/lte_lc.h>
#include <modem/modem_info.h>

#include <modem/nrf_modem_lib.h>
#include <nrf_modem_at.h>

/* Local */
#include "cloud/cloud.h"
#include "gnss/gnss.h"
#include "epaper/epaper.h"

#define CONFIG_RETRY_DELAY_MINUTES 1

/* Timer */
static void timeout_handler(struct k_timer *timer_id);
K_TIMER_DEFINE(timer, timeout_handler, NULL);

/* Thread control */
K_SEM_DEFINE(thread_sem, 0, 1);
K_SEM_DEFINE(lte_connected, 0, 1);

/* Variables */
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

NRF_MODEM_LIB_ON_INIT(aux_init_hook, on_modem_lib_init, NULL);

static void on_modem_lib_init(int ret, void *ctx)
{
    ARG_UNUSED(ctx);

    if (ret != 0)
    {
        return;
    }

    printk("*** Setting configuration: %s ***\n", AUXANTCFG_ENABLE);
    int err = nrf_modem_at_printf("%s", AUXANTCFG_ENABLE);
    if (err)
    {
        LOG_ERR("Failed to set configuration (err: %d)", err);
    }
}
#endif

/* Define the stack sizes for the threads */
#define STACK_SIZE 512
#define EPAPER_THREAD_STACK_SIZE 4096
/* Define thread priorities (lower number = higher priority) */
#define CLOUD_PRIORITY 7 
#define GNSS_PRIORITY  8 
#define EPAPER_PRIORITY 9

/* Thread entry function for the first thread (e.g., blinking an LED) */
void cloud_thr(void *p1, void *p2, void *p3) 
{

    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    /* Cloud init */
    int err = cloud_init(cloud_cb);
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

    /* Wait for a while, because with IPv4v6 PDN the IPv6 activation takes a bit more time. */
	k_sleep(K_SECONDS(1));

    LOG_INF("Safe to use sockets now. LTE is connected.");
    
    /* Initialize GNSS module*/
    err = gnss_init();
    if (err < 0)
    {
        LOG_ERR("Failed to initialize GNSS. Err: %i", err);
        return;
    }

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

/* Thread entry function for the second thread */
void gnss_thr(void *p1, void *p2, void *p3) 
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    gnss_thread();
}

/* Thread entry function for the second thread */
void epaper_thr(void *p1, void *p2, void *p3) 
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    int err = epaper_init();
    if (err < 0)
    {
        LOG_ERR("Failed to init epaper. (err: %i)", err);
        return;
    }
    k_sleep(K_SECONDS(1));

    // ePAPER test display
    epaper_display_test();
}

/* Define the threads using K_THREAD_DEFINE */
K_THREAD_DEFINE(cloud_thread_id, STACK_SIZE, cloud_thr, NULL, NULL, NULL, CLOUD_PRIORITY, 0, 0);
K_THREAD_DEFINE(gnss_thread_id, STACK_SIZE, gnss_thr, NULL, NULL, NULL, GNSS_PRIORITY, 0, 0);
K_THREAD_DEFINE(epaper_thread_id, EPAPER_THREAD_STACK_SIZE, epaper_thr, NULL, NULL, NULL, EPAPER_PRIORITY, 0, 0);

int main(void)
{
    LOG_INF("Fleetpin Project. Board: %s", CONFIG_BOARD);

    // /* GNSS pre-init functions */
    (void) gnss_pre_init();

    /* Register callback handler to handler LTE events */
    lte_lc_register_handler(lte_handler);

    /* Init modem lib */
    int err = nrf_modem_lib_init();
    if (err < 0)
    {
        LOG_ERR("Failed to init modem lib. (err: %i)", err);
        return err;
    }
  
    /* The main thread can also perform work or go to sleep */
    while (1) 
    {
        k_sleep(K_FOREVER); /* Sleep the main thread indefinitely */
    }
}
