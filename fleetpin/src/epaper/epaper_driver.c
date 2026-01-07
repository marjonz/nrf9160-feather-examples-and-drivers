/*****************************************************************************
* | File      	:   EPD_4in2b_V2.c
* | Author      :   Waveshare team
* | Function    :   4.2inch e-paper b V2
* | Info        :
*----------------
* |	This version:   V1.0
* | Date        :   2020-11-25
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
from : github.com/waveshareteam/e-Paper/blob/master/STM32/STM32-F103ZET6/User/e-Paper/EPD_4in2b_V2_old.c
******************************************************************************/
#include "epaper_driver.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(epaper_driver);

static bool is_old_type=true;

#define SPI_NODE DT_NODELABEL(spi2)
static const struct device *spi_dev = NULL;
static struct spi_config spi_cfg = {
    .frequency = 4000000,
    .operation =
        SPI_OP_MODE_MASTER |
        SPI_WORD_SET(8) |
        SPI_TRANSFER_MSB,
    .slave = 0,
    .cs = NULL, // Manual chip select control to be used
};

static const struct gpio_dt_spec chip_select_gpio = GPIO_DT_SPEC_GET(SPI_NODE, cs-gpios);
static const struct gpio_dt_spec reset_gpio = GPIO_DT_SPEC_GET(SPI_NODE, rst-gpios);
static const struct gpio_dt_spec data_cmd_gpio = GPIO_DT_SPEC_GET(SPI_NODE, data-cmd-gpios);
static const struct gpio_dt_spec busy_gpio = GPIO_DT_SPEC_GET(SPI_NODE, busy-in-gpios);
#define BUFFER_SIZE  2
static uint8_t tx_buf_data[BUFFER_SIZE] = { 0 };
static uint8_t rx_buf_data[BUFFER_SIZE] = { 0 };
static struct spi_buf tx_buf = 
{
    .buf = tx_buf_data,
    .len = sizeof(tx_buf_data),
};
static struct spi_buf rx_buf = 
{
    .buf = rx_buf_data,
    .len = sizeof(rx_buf_data),
};
static struct spi_buf_set tx = 
{
    .buffers = &tx_buf,
    .count = 1,
};
static struct spi_buf_set rx = 
{
    .buffers = &rx_buf,
    .count = 1,
};

/******************************************************************************
function :	Software reset
parameter:
******************************************************************************/
static void EPD_4IN2B_V2_Reset(void)
{
    //DEV_Digital_Write(EPD_RST_PIN, 1);
    gpio_pin_set_dt(&reset_gpio, 1);
    DEV_Delay_ms(200);
    //DEV_Digital_Write(EPD_RST_PIN, 0);
    gpio_pin_set_dt(&reset_gpio, 0);
    DEV_Delay_ms(2);
    //DEV_Digital_Write(EPD_RST_PIN, 1);
    gpio_pin_set_dt(&reset_gpio, 1);
    DEV_Delay_ms(200);
}

static void send_data(uint8_t data)
{
    gpio_pin_set_dt(&data_cmd_gpio, 1);
    if (spi_dev != NULL) 
    {
        tx_buf_data[0] = data;
        tx_buf.len = sizeof(data);
        rx_buf.len = sizeof(data);
        int ret = spi_transceive(spi_dev, &spi_cfg, &tx, &rx);
        if (ret != 0) 
        {
            LOG_DBG("SPI command transfer failed: %d", ret);
        }
    }
    else 
    {
        LOG_DBG("SPI device not initialized");
        return;
    }
}

/******************************************************************************
function :	send command
parameter:
     Reg : Command register
******************************************************************************/
static void EPD_4IN2B_V2_SendCommand(uint8 Reg)
{
    //DEV_Digital_Write(EPD_DC_PIN, 0);
    gpio_pin_set_dt(&data_cmd_gpio, 0);
    //DEV_Digital_Write(EPD_CS_PIN, 0);
    //DEV_SPI_SendData(Reg);
    //DEV_Digital_Write(EPD_CS_PIN, 1);
    gpio_pin_set_dt(&chip_select_gpio, 0);
    send_data(Reg);
    gpio_pin_set_dt(&chip_select_gpio, 1);
}

/******************************************************************************
function :	send data
parameter:
    Data : Write data
******************************************************************************/
static void EPD_4IN2B_V2_SendData(uint8 Data)
{
    //DEV_Digital_Write(EPD_DC_PIN, 1);
    gpio_pin_set_dt(&data_cmd_gpio, 1);
    //DEV_Digital_Write(EPD_CS_PIN, 0);
    //DEV_SPI_SendData(Data);
    //DEV_Digital_Write(EPD_CS_PIN, 1);
    gpio_pin_set_dt(&chip_select_gpio, 0);
    send_data(Data);
    gpio_pin_set_dt(&chip_select_gpio, 1);
}

/******************************************************************************
function :	Wait until the busy_pin goes LOW
parameter:
******************************************************************************/
void EPD_4IN2B_V2_ReadBusy_old(void)
{
    LOG_DBG("e-Paper busy\r\n");
	do{
		EPD_4IN2B_V2_SendCommand(0x71);
		DEV_Delay_ms(20);
	//}while(!(DEV_Digital_Read(EPD_BUSY_PIN)));      //0: busy, 1: idle
    }while((gpio_pin_get_dt(&busy_gpio))==0);      //0: busy, 1: idle
    DEV_Delay_ms(20);
	LOG_DBG("e-Paper busy release");
}

void EPD_4IN2B_V2_ReadBusy_new(void)
{
    LOG_DBG("e-Paper busy");
    //while(DEV_Digital_Read(EPD_BUSY_PIN) == 1) {      //LOW: idle, HIGH: busy
    while(gpio_pin_get_dt(&busy_gpio) == 1) {      //LOW: idle, HIGH: busy
        DEV_Delay_ms(10);
    }
    LOG_DBG("e-Paper busy release");
}

void EPD_4IN2B_V2_ReadBusy(void)
{
    if(is_old_type == false)
        EPD_4IN2B_V2_ReadBusy_new();
    else
        EPD_4IN2B_V2_ReadBusy_old();
}

/******************************************************************************
function :	Setting the display window
parameter:
******************************************************************************/
static void EPD_4IN2B_V2_SetWindows(UWORD Xstart, UWORD Ystart, UWORD Xend, UWORD Yend)
{
    EPD_4IN2B_V2_SendCommand(0x44); // SET_RAM_X_ADDRESS_START_END_POSITION
    EPD_4IN2B_V2_SendData((Xstart>>3) & 0xFF);
    EPD_4IN2B_V2_SendData((Xend>>3) & 0xFF);
	
    EPD_4IN2B_V2_SendCommand(0x45); // SET_RAM_Y_ADDRESS_START_END_POSITION
    EPD_4IN2B_V2_SendData(Ystart & 0xFF);
    EPD_4IN2B_V2_SendData((Ystart >> 8) & 0xFF);
    EPD_4IN2B_V2_SendData(Yend & 0xFF);
    EPD_4IN2B_V2_SendData((Yend >> 8) & 0xFF);
}

/******************************************************************************
function :	Set Cursor
parameter:
******************************************************************************/
static void EPD_4IN2B_V2_SetCursor(UWORD Xstart, UWORD Ystart)
{
    EPD_4IN2B_V2_SendCommand(0x4E); // SET_RAM_X_ADDRESS_COUNTER
    EPD_4IN2B_V2_SendData((Xstart>>3) & 0xFF);

    EPD_4IN2B_V2_SendCommand(0x4F); // SET_RAM_Y_ADDRESS_COUNTER
    EPD_4IN2B_V2_SendData(Ystart & 0xFF);
    EPD_4IN2B_V2_SendData((Ystart >> 8) & 0xFF);
}


/******************************************************************************
function :	Initialize the e-Paper register
parameter:
******************************************************************************/
void EPD_4IN2B_V2_Init_old(void)
{
    EPD_4IN2B_V2_Reset();

    EPD_4IN2B_V2_ReadBusy();   
    EPD_4IN2B_V2_SendCommand(0x04);   // soft  reset
    EPD_4IN2B_V2_ReadBusy();
    
    EPD_4IN2B_V2_SendCommand(0x00); //BorderWavefrom
    EPD_4IN2B_V2_SendData(0x0F);
}

void EPD_4IN2B_V2_Init_new(void)
{
    EPD_4IN2B_V2_Reset();

    EPD_4IN2B_V2_ReadBusy();   
    EPD_4IN2B_V2_SendCommand(0x12);   // soft  reset
    EPD_4IN2B_V2_ReadBusy();
	
    // EPD_4IN2B_V2_SendCommand(0x01); //Driver output control      
    // EPD_4IN2B_V2_SendData((EPD_4IN2B_V2_HEIGHT-1)%256);   
    // EPD_4IN2B_V2_SendData((EPD_4IN2B_V2_HEIGHT-1)/256);
    // EPD_4IN2B_V2_SendData(0x00);

    EPD_4IN2B_V2_SendCommand(0x3C); //BorderWavefrom
    EPD_4IN2B_V2_SendData(0x05);

    EPD_4IN2B_V2_SendCommand(0x18); //Read built-in temperature sensor
    EPD_4IN2B_V2_SendData(0x80);	
	
    EPD_4IN2B_V2_SendCommand(0x11);	// data  entry  mode
    EPD_4IN2B_V2_SendData(0x03);		// X-mode   
		
	EPD_4IN2B_V2_SetWindows(0, 0, EPD_4IN2B_V2_WIDTH-1, EPD_4IN2B_V2_HEIGHT-1);
	 
	EPD_4IN2B_V2_SetCursor(0, 0);
	
    EPD_4IN2B_V2_ReadBusy();
}

void EPD_4IN2B_V2_Init()
{
    uint8_t i=0u;

    // Configure additional GPIOs for e-Paper
    gpio_pin_configure_dt(&reset_gpio, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&data_cmd_gpio, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&busy_gpio, GPIO_INPUT);

    spi_dev = DEVICE_DT_GET(SPI_NODE);
    // FIXME: do we need to check???
    if (!device_is_ready(spi_dev)) {
        LOG_DBG("SPI device not ready\n");
        return;
    }

    EPD_4IN2B_V2_Reset();

    //DEV_Digital_Write(EPD_DC_PIN, 0);
    gpio_pin_set_dt(&data_cmd_gpio, 0);
    //DEV_SPI_SendData(0x2F);
    send_data(0x2F);
    DEV_Delay_ms(50);
    
    //DEV_Digital_Write(EPD_DC_PIN, 1);
    gpio_pin_set_dt(&data_cmd_gpio, 1);

    i = DEV_SPI_ReadData();
    // printf("%02x\n",i);

    if(i == 0x01u)
    {
        is_old_type = false;
        EPD_4IN2B_V2_Init_new();
    }
    else
    {
        is_old_type = true;
        EPD_4IN2B_V2_Init_old();
    }
}



/******************************************************************************
function :	Clear screen
parameter:
******************************************************************************/
void EPD_4IN2B_V2_Clear_old(void)
{
    UWORD Width, Height;
    Width = (EPD_4IN2B_V2_WIDTH % 8 == 0)? (EPD_4IN2B_V2_WIDTH / 8 ): (EPD_4IN2B_V2_WIDTH / 8 + 1);
    Height = EPD_4IN2B_V2_HEIGHT;

    EPD_4IN2B_V2_SendCommand(0x10);
    for (UWORD j = 0; j < Height; j++) {
        for (UWORD i = 0; i < Width; i++) {
            EPD_4IN2B_V2_SendData(0xFF);
        }
    }

    EPD_4IN2B_V2_SendCommand(0x13);
    for (UWORD j = 0; j < Height; j++) {
        for (UWORD i = 0; i < Width; i++) {
            EPD_4IN2B_V2_SendData(0xFF);
        }
    }

    EPD_4IN2B_V2_SendCommand(0x12);
    DEV_Delay_ms(100);
    EPD_4IN2B_V2_ReadBusy();
}


void EPD_4IN2B_V2_Clear_new(void)
{
    UWORD Width, Height;
    Width = (EPD_4IN2B_V2_WIDTH % 8 == 0)? (EPD_4IN2B_V2_WIDTH / 8 ): (EPD_4IN2B_V2_WIDTH / 8 + 1);
    Height = EPD_4IN2B_V2_HEIGHT;

    EPD_4IN2B_V2_SendCommand(0x24);
    for (UWORD j = 0; j < Height; j++) {
        for (UWORD i = 0; i < Width; i++) {
            EPD_4IN2B_V2_SendData(0xFF);
        }
    }

    EPD_4IN2B_V2_SendCommand(0x26);
    for (UWORD j = 0; j < Height; j++) {
        for (UWORD i = 0; i < Width; i++) {
            EPD_4IN2B_V2_SendData(0x00);
        }
    }

    EPD_4IN2B_V2_SendCommand(0x22);
	EPD_4IN2B_V2_SendData(0xF7);
    EPD_4IN2B_V2_SendCommand(0x20);
    EPD_4IN2B_V2_ReadBusy();
}

void EPD_4IN2B_V2_Clear(void)
{
    if(is_old_type == false)
        EPD_4IN2B_V2_Clear_new();
    else
        EPD_4IN2B_V2_Clear_old();
}

/******************************************************************************
function :	Sends the image buffer in RAM to e-Paper and displays
parameter:
******************************************************************************/
void EPD_4IN2B_V2_Display_old(const UBYTE *blackimage, const UBYTE *ryimage)
{
    UWORD Width, Height;
    Width = (EPD_4IN2B_V2_WIDTH % 8 == 0)? (EPD_4IN2B_V2_WIDTH / 8 ): (EPD_4IN2B_V2_WIDTH / 8 + 1);
    Height = EPD_4IN2B_V2_HEIGHT;

    EPD_4IN2B_V2_SendCommand(0x10);
    for (UWORD j = 0; j < Height; j++) {
        for (UWORD i = 0; i < Width; i++) {
            EPD_4IN2B_V2_SendData(blackimage[i + j * Width]);
        }
    }

    EPD_4IN2B_V2_SendCommand(0x13);
    for (UWORD j = 0; j < Height; j++) {
        for (UWORD i = 0; i < Width; i++) {
            EPD_4IN2B_V2_SendData(ryimage[i + j * Width]);
        }
    }

    EPD_4IN2B_V2_SendCommand(0x12);
    DEV_Delay_ms(100);
    EPD_4IN2B_V2_ReadBusy();
}

void EPD_4IN2B_V2_Display_new(const UBYTE *blackimage, const UBYTE *ryimage)
{
    UWORD Width, Height;
    Width = (EPD_4IN2B_V2_WIDTH % 8 == 0)? (EPD_4IN2B_V2_WIDTH / 8 ): (EPD_4IN2B_V2_WIDTH / 8 + 1);
    Height = EPD_4IN2B_V2_HEIGHT;

    EPD_4IN2B_V2_SendCommand(0x24);
    for (UWORD j = 0; j < Height; j++) {
        for (UWORD i = 0; i < Width; i++) {
            EPD_4IN2B_V2_SendData(blackimage[i + j * Width]);
        }
    }

    EPD_4IN2B_V2_SendCommand(0x26);
    for (UWORD j = 0; j < Height; j++) {
        for (UWORD i = 0; i < Width; i++) {
            EPD_4IN2B_V2_SendData(~ryimage[i + j * Width]);
        }
    }

    EPD_4IN2B_V2_SendCommand(0x22);
	EPD_4IN2B_V2_SendData(0xF7);
    EPD_4IN2B_V2_SendCommand(0x20);
    EPD_4IN2B_V2_ReadBusy();
}

void EPD_4IN2B_V2_Display(const UBYTE *blackimage, const UBYTE *ryimage)
{
    if(is_old_type == false)
        EPD_4IN2B_V2_Display_new(blackimage, ryimage);
    else
        EPD_4IN2B_V2_Display_old(blackimage, ryimage);
}

/******************************************************************************
function :	Enter sleep mode
parameter:
******************************************************************************/
void EPD_4IN2B_V2_Sleep_old(void)
{
    EPD_4IN2B_V2_SendCommand(0X50);  	
    EPD_4IN2B_V2_SendData(0xf7);
    EPD_4IN2B_V2_SendCommand(0x02);  
    EPD_4IN2B_V2_ReadBusy();
    EPD_4IN2B_V2_SendCommand(0x07);  
    EPD_4IN2B_V2_SendData(0XA5);
}
void EPD_4IN2B_V2_Sleep_new(void)
{
    EPD_4IN2B_V2_SendCommand(0X10);  	//deep sleep
    EPD_4IN2B_V2_SendData(0x03);
}

void EPD_4IN2B_V2_Sleep(void)
{
    if(is_old_type == false)
        EPD_4IN2B_V2_Sleep_new();
    else
        EPD_4IN2B_V2_Sleep_old();
}