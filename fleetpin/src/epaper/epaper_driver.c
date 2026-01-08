/*****************************************************************************
* | File      	:  	EPD_4in26.c
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
from : https://github.com/waveshareteam/e-Paper/blob/master/RaspberryPi_JetsonNano/c/lib/e-Paper/EPD_4in26.c
******************************************************************************/
#include "epaper_driver.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(epaper_driver);

const unsigned char LUT_DATA_4Gray[112] =    //112bytes
{											
    0x80u,	0x48u,	0x4Au,	0x22u,	0x00u,	0x00u,	0x00u,	0x00u,	0x00u,	0x00u,	
    0x0Au,	0x48u,	0x68u,	0x00u,	0x00u,	0x00u,	0x00u,	0x00u,	0x00u,	0x00u,	
    0x88u,	0x48u,	0x60u,	0x00u,	0x00u,	0x00u,	0x00u,	0x00u,	0x00u,	0x00u,	
    0xA8u,	0x48u,	0x45u,	0x00u,	0x00u,	0x00u,	0x00u,	0x00u,	0x00u,	0x00u,	
    0x00u,	0x00u,	0x00u,	0x00u,	0x00u,	0x00u,	0x00u,	0x00u,	0x00u,	0x00u,	
    0x07u,	0x1Eu,	0x1Cu,	0x02u,	0x00u,						
    0x05u,	0x01u,	0x05u,	0x01u,	0x02u,						
    0x08u,	0x01u,	0x01u,	0x04u,	0x04u,						
    0x00u,	0x02u,	0x00u,	0x02u,	0x01u,						
    0x00u,	0x00u,	0x00u,	0x00u,	0x00u,						
    0x00u,	0x00u,	0x00u,	0x00u,	0x00u,						
    0x00u,	0x00u,	0x00u,	0x00u,	0x00u,						
    0x00u,	0x00u,	0x00u,	0x00u,	0x00u,						
    0x00u,	0x00u,	0x00u,	0x00u,	0x00u,						
    0x00u,	0x00u,	0x00u,	0x00u,	0x01u,						
    0x22u,	0x22u,	0x22u,	0x22u,	0x22u,						
    0x17u,	0x41u,	0xA8u,	0x32u,	0x30u,						
    0x00u,	0x00u,	
};	

#if 0
#define SPI_NODE DT_NODELABEL(epaper_spi)
static const struct device *spi_dev = DEVICE_DT_GET(DT_BUS(SPI_NODE));
static struct spi_config spi_cfg = 
{
    .frequency = 4000000,
    .operation = (SPI_OP_MODE_MASTER | SPI_WORD_SET(8) | SPI_TRANSFER_MSB),
    .slave = 0,
    .cs = {{0}}, // Manual chip select control to be used
};

static const struct gpio_dt_spec chip_select_gpio = GPIO_DT_SPEC_GET(SPI_NODE, cs-gpios);
static const struct gpio_dt_spec reset_gpio = GPIO_DT_SPEC_GET(SPI_NODE, rst-gpios);
static const struct gpio_dt_spec data_cmd_gpio = GPIO_DT_SPEC_GET(SPI_NODE, data-cmd-gpios);
static const struct gpio_dt_spec busy_gpio = GPIO_DT_SPEC_GET(SPI_NODE, busy-in-gpios);
#endif
#define TX_BUFFER_SIZE 120 
#define RX_BUFFER_SIZE 20 
static uint8_t tx_buf_data[TX_BUFFER_SIZE] = { 0 };
static uint8_t rx_buf_data[RX_BUFFER_SIZE] = { 0 };
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
static void EPD_4in26_Reset(void)
{
    //DEV_Digital_Write(EPD_RST_PIN, 1);
    gpio_pin_set_dt(&reset_gpio, 1);
    k_msleep(100);
    //DEV_Digital_Write(EPD_RST_PIN, 0);
    gpio_pin_set_dt(&reset_gpio, 0);
    k_msleep(2);
    //DEV_Digital_Write(EPD_RST_PIN, 1);
    gpio_pin_set_dt(&reset_gpio, 1);
    k_msleep(100);
}

/******************************************************************************
 * Helper function to send n bytes over SPI
 *****************************************************************************/
static void send_n_bytes(uint8_t *data, size_t len)
{
    if (spi_dev != NULL) 
    {
        memset(tx_buf_data, 0, sizeof(tx_buf_data));
        memset(rx_buf_data, 0, sizeof(tx_buf_data));
        size_t data_len = len < TX_BUFFER_SIZE ? len : sizeof(tx_buf_data);
        memcpy(tx_buf_data, data, data_len);
        tx_buf.len = data_len;
        rx_buf.len = data_len;
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
static void EPD_4in26_SendCommand(uint8_t Reg)
{
    //DEV_Digital_Write(EPD_DC_PIN, 0);
    //DEV_Digital_Write(EPD_CS_PIN, 0);
    //DEV_SPI_SendData(Reg);
    //DEV_Digital_Write(EPD_CS_PIN, 1);
    gpio_pin_set_dt(&data_cmd_gpio, 0);
    gpio_pin_set_dt(&chip_select_gpio, 0);
    send_n_bytes(&Reg, 1);
    gpio_pin_set_dt(&chip_select_gpio, 1);
}

/******************************************************************************
function :	send data
parameter:
    Data : Write data
******************************************************************************/
static void EPD_4in26_SendData(uint8_t Data)
{
    //DEV_Digital_Write(EPD_DC_PIN, 1);
    //DEV_Digital_Write(EPD_CS_PIN, 0);
    //DEV_SPI_SendData(Data);
    //DEV_Digital_Write(EPD_CS_PIN, 1);
    gpio_pin_set_dt(&data_cmd_gpio, 1);
    gpio_pin_set_dt(&chip_select_gpio, 0);
    send_n_bytes(&Data, 1);
    gpio_pin_set_dt(&chip_select_gpio, 1);
}

static void EPD_4in26_SendData2(uint8_t *pData, size_t len)
{
    // DEV_Digital_Write(EPD_DC_PIN, 1);
    // DEV_Digital_Write(EPD_CS_PIN, 0);
    // DEV_SPI_Write_nByte(pData, len);
    // DEV_Digital_Write(EPD_CS_PIN, 1);
    gpio_pin_set_dt(&data_cmd_gpio, 1);
    gpio_pin_set_dt(&chip_select_gpio, 0);
    send_n_bytes(pData, len);
    gpio_pin_set_dt(&chip_select_gpio, 1);
}

/******************************************************************************
function :	Wait until the busy_pin goes LOW
parameter:
******************************************************************************/
void EPD_4in26_ReadBusy(void)
{
    LOG_DBG("e-Paper busy");
	while(1)
	{	 //=1 BUSY (ACTIVE HIGH)
		if(gpio_pin_get_dt(&busy_gpio)==0) 
			break;
		k_msleep(20);
	}
	k_msleep(20);
    LOG_DBG("e-Paper busy release\r\n");
}

static inline void send_display_update_data(uint8_t data)
{
	EPD_4in26_SendCommand(0x22u); //Display Update Control
	EPD_4in26_SendData(data);
	EPD_4in26_SendCommand(0x20u); //Activate Display Update Sequence
	EPD_4in26_ReadBusy();
}

/******************************************************************************
function :	Turn On Display
parameter:
******************************************************************************/
static void EPD_4in26_TurnOnDisplay(void)
{
    send_display_update_data(0xF7u);
}

static void EPD_4in26_TurnOnDisplay_Fast(void)
{
    send_display_update_data(0xC7u);
}

static void EPD_4in26_TurnOnDisplay_Part(void)
{
    send_display_update_data(0xFFu);
}

static void EPD_4in26_TurnOnDisplay_4GRAY(void)
{
    send_display_update_data(0xC7u);
}

/******************************************************************************
function :	set the look-up tables
parameter:
******************************************************************************/
static void EPD_4in26_Lut(void)
{
    uint16_t count;
    EPD_4in26_SendCommand(0x32u); //vcom
    for(count = 0u; count < 105u ; count++) 
    {
        EPD_4in26_SendData(LUT_DATA_4Gray[count]);
    }

    EPD_4in26_SendCommand(0x03u); //VGH      
	EPD_4in26_SendData(LUT_DATA_4Gray[105u]);

	EPD_4in26_SendCommand(0x04); //      
	EPD_4in26_SendData(LUT_DATA_4Gray[106u]); //VSH1   
	EPD_4in26_SendData(LUT_DATA_4Gray[107u]); //VSH2   
	EPD_4in26_SendData(LUT_DATA_4Gray[108u]); //VSL   

	EPD_4in26_SendCommand(0x2Cu);     //VCOM Voltage
	EPD_4in26_SendData(LUT_DATA_4Gray[109u]);    //0x1C
}

static inline void send_position_data(uint16_t pos)
{
    EPD_4in26_SendData(pos & 0xFFu);
    EPD_4in26_SendData((pos >> 8u) & 0x03u);
}

/******************************************************************************
function :	Setting the display window
parameter:
******************************************************************************/
static void EPD_4in26_SetWindows(uint16_t Xstart, uint16_t Ystart, uint16_t Xend, uint16_t Yend)
{
    EPD_4in26_SendCommand(0x44u); // SET_RAM_X_ADDRESS_START_END_POSITION
    send_position_data(Xstart);
    send_position_data(Xend);
	
    EPD_4in26_SendCommand(0x45u); // SET_RAM_Y_ADDRESS_START_END_POSITION
    send_position_data(Ystart);
    send_position_data(Yend);
}

/******************************************************************************
function :	Set Cursor
parameter:
******************************************************************************/
static void EPD_4in26_SetCursor(uint16_t Xstart, uint16_t Ystart)
{
    EPD_4in26_SendCommand(0x4Eu); // SET_RAM_X_ADDRESS_COUNTER
    send_position_data(Xstart);

    EPD_4in26_SendCommand(0x4Fu); // SET_RAM_Y_ADDRESS_COUNTER
    send_position_data(Ystart);
}

/******************************************************************************
function :	Initialize the e-Paper register
parameter:
******************************************************************************/
void EPD_4in26_Init(void)
{
	EPD_4in26_Reset();
	k_msleep(100);

	EPD_4in26_ReadBusy();   
	EPD_4in26_SendCommand(0x12u);  //SWRESET
	EPD_4in26_ReadBusy();   
	
	EPD_4in26_SendCommand(0x18u); // use the internal temperature sensor
	EPD_4in26_SendData(0x80u);

	EPD_4in26_SendCommand(0x0Cu); //set soft start     
	EPD_4in26_SendData(0xAEu);
	EPD_4in26_SendData(0xC7u);
	EPD_4in26_SendData(0xC3u);
	EPD_4in26_SendData(0xC0u);
	EPD_4in26_SendData(0x80u);

	EPD_4in26_SendCommand(0x01u);   //      drive output control    
	EPD_4in26_SendData((EPD_4in26_HEIGHT-1)%256); //  Y  
	EPD_4in26_SendData((EPD_4in26_HEIGHT-1)/256); //  Y 
	EPD_4in26_SendData(0x02);

	EPD_4in26_SendCommand(0x3C);        // Border       Border setting 
	EPD_4in26_SendData(0x01);

	EPD_4in26_SendCommand(0x11);        //    data  entry  mode
	EPD_4in26_SendData(0x01);           //       X-mode  x+ y-    

	EPD_4in26_SetWindows(0, EPD_4in26_HEIGHT-1, EPD_4in26_WIDTH-1, 0);

	EPD_4in26_SetCursor(0, 0);

	EPD_4in26_ReadBusy();
}

void EPD_4in26_Init_Fast(void)
{
	EPD_4in26_Reset();
	k_msleep(100);

	EPD_4in26_ReadBusy();   
	EPD_4in26_SendCommand(0x12u);  //SWRESET
	EPD_4in26_ReadBusy();   
	
	EPD_4in26_SendCommand(0x18u); // use the internal temperature sensor
	EPD_4in26_SendData(0x80u);

	EPD_4in26_SendCommand(0x0Cu); //set soft start     
	EPD_4in26_SendData(0xAEu);
	EPD_4in26_SendData(0xC7u);
	EPD_4in26_SendData(0xC3u);
	EPD_4in26_SendData(0xC0u);
	EPD_4in26_SendData(0x80u);

	EPD_4in26_SendCommand(0x01u);   //      drive output control    
	EPD_4in26_SendData((EPD_4in26_HEIGHT-1) % 256); //  Y  
	EPD_4in26_SendData((EPD_4in26_HEIGHT-1) / 256); //  Y 
	EPD_4in26_SendData(0x02u);

	EPD_4in26_SendCommand(0x3Cu);        // Border       Border setting 
	EPD_4in26_SendData(0x01u);

	EPD_4in26_SendCommand(0x11u);        //    data  entry  mode
	EPD_4in26_SendData(0x01u);           //       X-mode  x+ y-    

	EPD_4in26_SetWindows(0, EPD_4in26_HEIGHT-1, EPD_4in26_WIDTH-1, 0);

	EPD_4in26_SetCursor(0, 0);

	EPD_4in26_ReadBusy();

	//TEMP (1.5s)
	EPD_4in26_SendCommand(0x1Au);  
    EPD_4in26_SendData(0x5Au); 

    EPD_4in26_SendCommand(0x22u);  
    EPD_4in26_SendData(0x91u); 
    EPD_4in26_SendCommand(0x20u); 
	
	EPD_4in26_ReadBusy();
}

void EPD_4in26_Init_4GRAY(void)
{
    EPD_4in26_Reset();
	k_msleep(100);

	EPD_4in26_ReadBusy();   
	EPD_4in26_SendCommand(0x12u);  //SWRESET
	EPD_4in26_ReadBusy();   
	
	EPD_4in26_SendCommand(0x18u); // use the internal temperature sensor
	EPD_4in26_SendData(0x80u);
	EPD_4in26_SendCommand(0x0Cu); //set soft start     
	EPD_4in26_SendData(0xAEu);
	EPD_4in26_SendData(0xC7u);
	EPD_4in26_SendData(0xC3u);
	EPD_4in26_SendData(0xC0u);
	EPD_4in26_SendData(0x80u);
	EPD_4in26_SendCommand(0x01u);   //      drive output control    
	EPD_4in26_SendData((EPD_4in26_WIDTH-1) %256); //  Y  
	EPD_4in26_SendData((EPD_4in26_WIDTH-1) / 256); //  Y 
	EPD_4in26_SendData(0x02u);

	EPD_4in26_SendCommand(0x3Cu);        // Border       Border setting 
	EPD_4in26_SendData(0x01u);

	EPD_4in26_SendCommand(0x11u);        //    data  entry  mode
	EPD_4in26_SendData(0x01u);           //       X-mode  x+ y-    
	EPD_4in26_SetWindows(0, EPD_4in26_HEIGHT-1, EPD_4in26_WIDTH-1, 0);

	EPD_4in26_SetCursor(0, 0);

	EPD_4in26_ReadBusy();

    EPD_4in26_Lut();
}

/******************************************************************************
function :	Clear screen
parameter:
******************************************************************************/
void EPD_4in26_Clear(void)
{
	uint16_t i;
	uint16_t height = EPD_4in26_HEIGHT;
	uint16_t width = EPD_4in26_WIDTH / 8u;	
	uint8_t image[EPD_4in26_WIDTH / 8u] = {0};
    for(i = 0u; i < width; i++) 
    {
        image[i] = 0xFFu;
    }
    
	EPD_4in26_SendCommand(0x24u);   //write RAM for black(0)/white (1)
	for(i = 0u; i < height; i++)
	{
	    EPD_4in26_SendData2(image, width);
	}

	EPD_4in26_SendCommand(0x26u);   //write RAM for black(0)/white (1)
	for(i = 0u; i < height; i++)
	{
		EPD_4in26_SendData2(image, width);
	}
	EPD_4in26_TurnOnDisplay();
}

/******************************************************************************
function :	Sends the image buffer in RAM to e-Paper and displays
parameter:
******************************************************************************/
void EPD_4in26_Display(uint8_t *Image)
{
	uint16_t i;
	uint16_t height = EPD_4in26_HEIGHT;
	uint16_t width = EPD_4in26_WIDTH/8;
	
	EPD_4in26_SendCommand(0x24u);   //write RAM for black(0)/white (1)
	for( i = 0u; i < height; i++)
	{
        EPD_4in26_SendData2((uint8_t *)(Image+i*width), width);
	}
	EPD_4in26_TurnOnDisplay();	
}

void EPD_4in26_Display_Base(uint8_t *Image)
{
	uint16_t i;
	uint16_t height = EPD_4in26_HEIGHT;
	uint16_t width = EPD_4in26_WIDTH/8;
	
	EPD_4in26_SendCommand(0x24u);   //write RAM for black(0)/white (1)
	for(i = 0u; i < height; i++)
	{
		EPD_4in26_SendData2((uint8_t *)(Image+i*width), width);
	}

	EPD_4in26_SendCommand(0x26u);   //write RAM for black(0)/white (1)
	for(i = 0u; i < height; i++)
	{
		EPD_4in26_SendData2((uint8_t *)(Image+i*width), width);
	}
	EPD_4in26_TurnOnDisplay();	
}

void EPD_4in26_Display_Fast(uint8_t *Image)
{
	uint16_t i;
	uint16_t height = EPD_4in26_HEIGHT;
	uint16_t width = EPD_4in26_WIDTH/8;
	
	EPD_4in26_SendCommand(0x24u);   //write RAM for black(0)/white (1)
	for(i = 0u; i < height; i++)
	{
		EPD_4in26_SendData2((uint8_t *)(Image+i*width), width);
	}
	EPD_4in26_TurnOnDisplay_Fast();	
}

void EPD_4in26_Display_Part(uint8_t *Image, uint16_t x, uint16_t y, uint16_t w, uint16_t l)
{
	uint16_t i;
	uint16_t height = l;
	uint16_t width =  (w % 8u == 0u) ? (w / 8u): ((w / 8u) + 1);

    EPD_4in26_Reset();

	EPD_4in26_SendCommand(0x18u); // use the internal temperature sensor
	EPD_4in26_SendData(0x80u);

	EPD_4in26_SendCommand(0x3Cu);        // Border       Border setting 
	EPD_4in26_SendData(0x80u);

	EPD_4in26_SetWindows(x, y, x+w-1, y+l-1);

	EPD_4in26_SetCursor(x, y);

	EPD_4in26_SendCommand(0x24u);   //write RAM for black(0)/white (1)
	for( i = 0u; i < height; i++)
	{
		EPD_4in26_SendData2((uint8_t *)(Image+i*width), width);
	}
	EPD_4in26_TurnOnDisplay_Part();	
}

void EPD_4in26_4GrayDisplay(uint8_t *Image)
{
    uint16_t i,j,k;
    uint8_t temp1,temp2,temp3;

    // old  data
    EPD_4in26_SendCommand(0x24);
    for(i = 0u; i < 48000u; i++) 
    {             //5808*4  46464
        temp3=0;
        for(j = 0u; j < 2u; j++) 
        {
            temp1 = Image[i*2+j];
            for(k = 0u; k < 2; k++) 
            {
                temp2 = (temp1 & 0xC0u);
                switch(temp2)
                {
                    case 0xC0u:
                        temp3 |= 0x00u;
                        break;
                    case 0x00u:
                    case 0x80u:
                        temp3 |= 0x01u;
                        break;
                    case 0x40u:
                    default:
                        temp3 |= 0x01u; 
                        break;
                }
                temp3 <<= 1u;

                temp1 <<= 2u;
                temp2 = (temp1 & 0xC0u);
                switch(temp2)
                {
                    case 0xC0u:
                        temp3 |= 0x00;
                        break;
                    case 0x00u:
                    case 0x80u:
                        temp3 |= 0x01u;
                        break;
                    case 0x40u:
                    default:
                        temp3 |= 0x00u; 
                        break;
                }

                if((j!=1u) || (k!=1))
                {
                    temp3 <<= 1u;
                }

                temp1 <<= 2u;
            }

        }
        EPD_4in26_SendData(temp3);
        // printf("%x",temp3);
    }

    EPD_4in26_SendCommand(0x26u);   //write RAM for black(0)/white (1)
    for(i = 0u; i < 48000u; i++) 
    {             //5808*4  46464
        temp3=0;
        for( j = 0; j < 2; j++) 
        {
            temp1 = Image[i*2+j];
            for(k=0; k<2; k++) 
            {
                temp2 = temp1 & 0xC0u ;
                switch(temp2)
                {
                    case 0xC0u:
                        temp3 |= 0x00u; //white
                        break;
                    case 0x00u:
                        temp3 |= 0x01u;  //black
                        break;
                    case 0x80u:
                        temp3 |= 0x00u;  //gray1
                        break;
                    case 0x40u:
                    default:
                        temp3 |= 0x01u; //gray2
                        break;
                }
                temp3 <<= 1u;

                temp1 <<= 2u;
                temp2 = (temp1 & 0xC0u);
                switch(temp2)
                {
                    case 0xC0u:
                        temp3 |= 0x00u;  //white
                        break;
                    case 0x00u:
                        temp3 |= 0x01u; //black
                        break;
                    case 0x80u:
                        temp3 |= 0x00u; //gray1
                        break;
                    case 0x40u:
                    default:
                        temp3 |= 0x01u;	//gray2
                        break;
                }

                if((j != 1u) || (k != 1u))
                {
                    temp3 <<= 1u;
                }

                temp1 <<= 2u;
            }
        }
        EPD_4in26_SendData(temp3);
        // printf("%x",temp3);
    }

    EPD_4in26_TurnOnDisplay_4GRAY();
}

/******************************************************************************
function :	Enter sleep mode
parameter:
******************************************************************************/
void EPD_4in26_Sleep(void)
{
	EPD_4in26_SendCommand(0x10u); //enter deep sleep
	EPD_4in26_SendData(0x03u); 
	k_msleep(100);
}