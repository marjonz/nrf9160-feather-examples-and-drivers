# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(fleetpin)

target_sources(app PRIVATE src/main.c)
target_sources(app PRIVATE src/cloud/cloud.c)

zephyr_library_sources_ifdef(CONFIG_GNSS_SAMPLE_ASSISTANCE_NRF_CLOUD ${NRF_DIR}/samples/cellular/gnss/src/assistance.c)
zephyr_library_sources_ifdef(CONFIG_GNSS_SAMPLE_ASSISTANCE_SUPL ${NRF_DIR}/samples/cellular/gnss/src/assistance_supl.c)
zephyr_library_sources_ifdef(CONFIG_GNSS_SAMPLE_ASSISTANCE_MINIMAL ${NRF_DIR}/samples/cellular/gnss/src/assistance_minimal.c)
zephyr_library_sources_ifdef(CONFIG_GNSS_SAMPLE_ASSISTANCE_MINIMAL ${NRF_DIR}/samples/cellular/gnss/src/mcc_location_table.c)
