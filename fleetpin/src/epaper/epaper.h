/***********************************************************************
 * @file    epaper.h
 * @author  Flexware Ltd
 * @date    Jan 2026
 * @brief   Header file for ePaper display functions
 ***********************************************************************/

/**
 * @brief Initialize the ePaper display
 *
  * @return int 0 if successful, negative errno otherwise
 *
 */
int epaper_init(void);

/**
 * @brief API used to clear the display
 *
 */
void epaper_clear(void);

/**
 * @brief API used to draw a bitmap image to the display
 * @param bmp Pointer to the bitmap image data, the size of the bitmap 
 *          must match the display resolution (i.e. 800x480).
 */
void epaper_draw_bitmap(const unsigned char* bmp);

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