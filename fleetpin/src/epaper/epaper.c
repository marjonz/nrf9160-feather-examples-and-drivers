#include "epaper_driver.h"
#include "GUI_Paint.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(epaper, LOG_LEVEL_DBG);

//static const size_t Imagesize = ((EPD_4in26_WIDTH % 8 == 0)? (EPD_4in26_WIDTH / 8 ): (EPD_4in26_WIDTH / 8 + 1)) * EPD_4in26_HEIGHT;
#define MAX_IMAGE_SIZE ((EPD_4in26_WIDTH/8u) * EPD_4in26_HEIGHT)
//Create a new image cache
static uint8_t DisplayImage[MAX_IMAGE_SIZE] = {0};

int epaper_init(void)
{
    EPD_4in26_Init();
    ///EPD_4in26_Init_4GRAY();
    EPD_4in26_Clear();
    k_msleep(500);

    return 0;
}

void epaper_display_test(void)
{
    LOG_DBG("ePaper Display Test");
    Paint_NewImage(DisplayImage, EPD_4in26_WIDTH, EPD_4in26_HEIGHT, 0, BLACK);
    LOG_DBG("Painting new image....");
    //EPD_4in26_Display_Fast(DisplayImage);
    EPD_4in26_Display_Base(DisplayImage);
    k_msleep(2000);
}