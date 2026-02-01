#ifndef EPAPER_H
#define EPAPER_H

#include <stdint.h>
#include <stdlib.h>

/***********************************************************************
 * @file    epaper.h
 * @author  Flexware Ltd
 * @date    Jan 2026
 * @brief   Header file for ePaper display functions
 ***********************************************************************/

/**
 * @brief Initialize the ePaper display
 * @param image_buffer - the image buffer to use for the epaper 
  * @return int 0 if successful, negative errno otherwise
 *
 */
int epaper_init(uint8_t * const image_buffer);

/**
 * @brief API used to clear the display
 *
 */
void epaper_clear(void);

/**
 * @brief API used to store chunks of data into the image buffer. 
 *      To be used when the caller has chunks of data being read from source, and being stored into a larger buffer.
 * @return -EINVAL if the buffer pointers are NULL, 
 *         -EDOM if the indices will exceed the buffer size
 *          non-zero = new buffer index if the store command is successful
 */
int epaper_store_data_to_buffer(uint8_t * const image_buffer, size_t current_buffer_index, 
    size_t max_image_buffer_size, const uint8_t * const current_data_ptr, size_t current_data_len);

/**
 * @brief API used to display what is currently stored in the image buffer
 * @param image_buffer - the image buffer to display, must already have the data necessary to display.
 */
void epaper_draw_current_image_buffer(uint8_t * const image_buffer);

/**
 * @brief API used to draw a bitmap image to the display
 * @param image_buffer - image buffer to use for drawing the bitmap
 * @param bmp Pointer to the bitmap image data, the size of the bitmap 
 *          must match the display resolution (i.e. 800x480).
 */
void epaper_draw_bitmap(uint8_t * const image_buffer, const unsigned char* bmp) ;

/**
 * @brief API used to display test image
 *
 */
void epaper_display_test(void);

/**
 * @brief API used to power the unit off.
 */
void epaper_poweroff(void);

/**
 * @brief API used to power the unit back on after power off.
 * Must have run epaper_init() prior, and have powered off prior.
 *
 */
void epaper_re_poweron(void);

/**
 * @brief ONLY USED FOR DEBUGGING THE BUSY PIN. DO NOT CALL IN PRODUCTION CODE.
 *
 */
void epaper_debug_busy(void);

#endif