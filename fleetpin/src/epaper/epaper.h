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
 * @brief API used to display test image
 *
 */
void epaper_display_test(void);

void epaper_debug_busy(void);