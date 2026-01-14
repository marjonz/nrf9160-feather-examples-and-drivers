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

void epaper_debug_busy(void);