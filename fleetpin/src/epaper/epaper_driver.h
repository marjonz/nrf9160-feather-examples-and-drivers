/*****************************************************************************
* | File      	:   EPD_4in26.h
* | Author      :   Waveshare team
* | Function    :   4.26inch e-paper test demo
* | Info        :
*----------------
* |	This version:   V1.0
* | Date        :   2023-12-19
* | Info        :
* -----------------------------------------------------------------------------
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documnetation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to  whom the Software is
# furished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
******************************************************************************
from : https://github.com/waveshareteam/e-Paper/blob/master/RaspberryPi_JetsonNano/c/lib/e-Paper/EPD_4in26.h
******************************************************************************/
#ifndef __EPD_4in26_H_
#define __EPD_4in26_H_

#include <stdint.h>

// Display resolution
#define EPD_4in26_WIDTH       (800u)
#define EPD_4in26_HEIGHT      (480u)

void EPD_4in26_Init(void);
void EPD_4in26_Init_Fast(void);
void EPD_4in26_Init_4GRAY(void);
void EPD_4in26_Clear(void);
void EPD_4in26_Display(uint8_t *Image);
void EPD_4in26_Display_Base(uint8_t *Image);
void EPD_4in26_Display_Fast(uint8_t *Image);
void EPD_4in26_Display_Part(uint8_t *Image, uint16_t x, uint16_t y, uint16_t w, uint16_t l);
void EPD_4in26_4GrayDisplay(uint8_t *Image);
void EPD_4in26_Sleep(void);
void EPD_4in26_ReadBusy_Debug(void);

#endif