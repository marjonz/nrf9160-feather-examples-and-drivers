#include "epaper_driver.h"
#include "GUI_Paint.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(epaper, LOG_LEVEL_DBG);

//#define EPAPER_TEST_FLEXWARE_LOGO true
//#define DISPLAY_FLEETPIN_SAMPLE_IMAGE true
//#define CUSTOMER_LABEL true

#ifdef EPAPER_TEST_FLEXWARE_LOGO
#include "flexware_logo.h"
#endif
#ifdef DISPLAY_FLEETPIN_SAMPLE_IMAGE
#include "fleetpin_sample.h"
#endif
#ifdef CUSTOMER_LABEL
#include "label.h"
#endif

int epaper_init(uint8_t * const image_buffer)
{
    if (image_buffer == NULL)
    {
        return -1;
    }

    LOG_DBG("ePaper Initialized");
    EPD_4in26_Init();
    Paint_NewImage(image_buffer, EPD_4in26_WIDTH, EPD_4in26_HEIGHT, 0, WHITE);
    k_msleep(500);

    return 0;
}

void epaper_clear(void)
{
    LOG_DBG("Clear Screen");
    EPD_4in26_Clear();
}

void epaper_poweroff(void) 
{
    LOG_DBG("Power Off");
    EPD_4in26_Sleep();
}

/* NOTE: Init must already have been run prior to calling this API */
void epaper_re_poweron(void)
{
    LOG_DBG("re-Power On");
    EPD_4in26_RePowerOn();
}

int epaper_store_data_to_buffer(uint8_t * const image_buffer, size_t current_buffer_index, 
    size_t max_image_buffer_size, const uint8_t * const current_data_ptr, size_t current_data_len)
{
    if (image_buffer == NULL || current_data_ptr == NULL)
    {
        return -EINVAL;
    }

    if (current_buffer_index + current_data_len > max_image_buffer_size)
    {
        return -EDOM;
    }

    memcpy(&image_buffer[current_buffer_index], current_data_ptr, current_data_len);
    size_t total_bytes_in_buffer = current_buffer_index + current_data_len;
    return (int) total_bytes_in_buffer;
}

void epaper_draw_current_image_buffer(uint8_t * const image_buffer)
{
    if (image_buffer != NULL)
    {
        LOG_DBG("Drawing bitmap image from buffer");
        Paint_SelectImage(image_buffer);
        Paint_SetScale(2);
        Paint_Clear(WHITE);
        EPD_4in26_Display(image_buffer);        
    }
}


void epaper_draw_bitmap(uint8_t * const image_buffer, const unsigned char* bmp) 
{
    LOG_DBG("Draw bitmap image");
    Paint_SelectImage(image_buffer);
    Paint_SetScale(2);
    Paint_Clear(WHITE);
    Paint_DrawBitMap(bmp);
    EPD_4in26_Display(image_buffer);
}

/************************************************************************
 *  TEST CODE for displaying Flexware logo on ePaper
 */
void epaper_display_test(void)
{
    LOG_DBG("ePaper Display Test...");

    #ifdef EPAPER_TEST_FLEXWARE_LOGO
    epaper_draw_bitmap(flexware_logo_800_x_480_v4_bits);
    k_msleep(2000);
    #endif

    #ifdef DISPLAY_FLEETPIN_SAMPLE_IMAGE
    epaper_draw_bitmap(gImage_4in26);
    k_msleep(2000);
    #endif

    #ifdef CUSTOMER_LABEL
    epaper_draw_bitmap(fleetpin_label);
    k_msleep(2000);
    #endif

    #if 0
    Paint_NewImage(DisplayImage, EPD_4in26_WIDTH, EPD_4in26_HEIGHT, 0, WHITE);
    LOG_DBG("Painting new image....");
    EPD_4in26_Display_Base(DisplayImage);
    k_msleep(2000);
    #endif

    #if 0
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

    #if 0 // show image for array
    EPD_4in26_Init_4GRAY();
    LOG_DBG("4 grayscale display");
    Paint_NewImage(DisplayImage, EPD_4in26_WIDTH, EPD_4in26_HEIGHT/2, 90, WHITE);
    Paint_SetScale(4);
    Paint_Clear(0xff);
    
    Paint_DrawPoint(10, 80, GRAY4, DOT_PIXEL_1X1, DOT_STYLE_DFT);
    Paint_DrawPoint(10, 90, GRAY4, DOT_PIXEL_2X2, DOT_STYLE_DFT);
    Paint_DrawPoint(10, 100, GRAY4, DOT_PIXEL_3X3, DOT_STYLE_DFT);
    Paint_DrawLine(20, 70, 70, 120, GRAY4, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
    Paint_DrawLine(70, 70, 20, 120, GRAY4, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
    Paint_DrawRectangle(20, 70, 70, 120, GRAY4, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
    Paint_DrawRectangle(80, 70, 130, 120, GRAY4, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    Paint_DrawCircle(45, 95, 20, GRAY4, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
    Paint_DrawCircle(105, 95, 20, GRAY2, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    Paint_DrawLine(85, 95, 125, 95, GRAY4, DOT_PIXEL_1X1, LINE_STYLE_DOTTED);
    Paint_DrawLine(105, 75, 105, 115, GRAY4, DOT_PIXEL_1X1, LINE_STYLE_DOTTED);
    Paint_DrawString_EN(10, 0, "waveshare", &Font16, GRAY4, GRAY1);
    Paint_DrawString_EN(10, 20, "hello world", &Font12, GRAY3, GRAY1);
    Paint_DrawNum(10, 33, 123456789, &Font12, GRAY4, GRAY2);
    Paint_DrawNum(10, 50, 987654321, &Font16, GRAY1, GRAY4);
    EPD_4in26_4GrayDisplay(DisplayImage);
    k_msleep(3000);

#endif
}

void epaper_debug_busy(void)
{
    EPD_4in26_ReadBusy_Debug();
}
