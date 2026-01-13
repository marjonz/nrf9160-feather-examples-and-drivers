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

    #if 0
    Paint_NewImage(DisplayImage, EPD_4in26_WIDTH, EPD_4in26_HEIGHT, 0, BLACK);
    LOG_DBG("Painting new image....");
    //EPD_4in26_Display_Fast(DisplayImage);
    EPD_4in26_Display_Base(DisplayImage);
    k_msleep(2000);
    #endif

    #if 1
    Paint_SelectImage(DisplayImage);
    Paint_Clear(WHITE);

    Paint_DrawPoint(10, 80, BLACK, DOT_PIXEL_1X1, DOT_STYLE_DFT);
    Paint_DrawPoint(10, 90, BLACK, DOT_PIXEL_2X2, DOT_STYLE_DFT);
    Paint_DrawPoint(10, 100, BLACK, DOT_PIXEL_3X3, DOT_STYLE_DFT);
    Paint_DrawLine(20, 70, 70, 120, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
    Paint_DrawLine(70, 70, 20, 120, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
    Paint_DrawRectangle(20, 70, 70, 120, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
    Paint_DrawRectangle(80, 70, 130, 120, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    Paint_DrawCircle(45, 95, 20, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
    Paint_DrawCircle(105, 95, 20, WHITE, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    Paint_DrawLine(85, 95, 125, 95, BLACK, DOT_PIXEL_1X1, LINE_STYLE_DOTTED);
    Paint_DrawLine(105, 75, 105, 115, BLACK, DOT_PIXEL_1X1, LINE_STYLE_DOTTED);
    Paint_DrawString_EN(10, 1, "waveshare", &Font16, BLACK, WHITE);
    Paint_DrawString_EN(10, 20, "Flexware is LOVE.", &Font12, WHITE, BLACK);
    Paint_DrawNum(10, 33, 123456789, &Font12, BLACK, WHITE);
    Paint_DrawNum(10, 50, 987654321, &Font16, WHITE, BLACK);
    EPD_4in26_Display_Base(DisplayImage);
    k_msleep(2000);
    #endif
}