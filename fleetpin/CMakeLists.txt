# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(fleetpin)

target_sources(app PRIVATE src/main.c)

# LTE Cloud files
target_sources(app PRIVATE src/cloud/cloud.c)
# Authentication
target_sources(app PRIVATE src/cloud/authentication/auth.c)

# Waveshare epaper files
target_sources(app PRIVATE src/epaper/epaper_driver.c)
target_sources(app PRIVATE src/epaper/epaper.c)
target_sources(app PRIVATE src/epaper/GUI_Paint.c)
target_sources(app PRIVATE src/epaper/Fonts/font8.c)
target_sources(app PRIVATE src/epaper/Fonts/font12.c)
target_sources(app PRIVATE src/epaper/Fonts/font16.c)
target_sources(app PRIVATE src/epaper/Fonts/font20.c)
target_sources(app PRIVATE src/epaper/Fonts/font24.c)

# Flash filesystem
target_sources(app PRIVATE src/flash/flash_fs.c)

# Date / Time - the libraries are include via the defines in the prj.conf file
