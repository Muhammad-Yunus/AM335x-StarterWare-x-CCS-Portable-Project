/*
 * lv_port_disp.c ??? AM3352 / ILI9341 SPI0 port of LVGL v9 display driver.
 *
 * Adapted from the STM32H7 reference at:
 *   C:\D\MY\DEV\STM32CubeIDE\workspace_1.18.1\ILI9341_NUCLEO_H753ZI\Core\Src\main.c
 *
 * Key differences from the STM32 version:
 *   - No DMA. Flush is a blocking SPI byte stream through Spi0TxBufferFast().
 *   - Frame buffers are static arrays in .bss (DDR, since the AM335x linker
 *     uses RAM_MODEL with everything in DDR_MEM). Each buffer is one full
 *     frame: 320 * 240 * 2 = 153,600 bytes. PARTIAL render mode only needs
 *     partial writes, but the full-size buffer keeps the LVGL re-fragmenting
 *     simple and matches the STM32 layout.
 *   - LVGL is initialised and ticked from main.c; this file only owns the
 *     display driver.
 */

#include "lv_port_disp.h"
#include "ili9341.h"

#define LCD_WIDTH   ILI9341_WIDTH
#define LCD_HEIGHT  ILI9341_HEIGHT
#define LCD_FB_BPP  2  /* RGB565 */

/* Block transfer helper provided by main.c. Asserts CS once and streams
 * the entire block, which is ~3x faster than the byte-per-CS path used
 * by ili9341.c for the init sequence. */
extern void Spi0TxBufferFast(const uint8_t *buf, uint32_t len);

/* Two full-frame buffers (320 * 240 * 2 = 153,600 bytes each,
 * 307 KB total). FULL render mode avoids PARTIAL tiling overhead
 * (3?? col/page/GRAM command packets per band) ??? critical for
 * ARM9 @ 600MHz where SPI tx time is dwarfed by LVGL redraw churn. */
#define LCD_FB_COLS        (LCD_WIDTH)
#define LCD_FB_ROWS        (LCD_HEIGHT)
#define LCD_FB_SIZE        (LCD_FB_COLS * LCD_FB_ROWS * LCD_FB_BPP)

static uint8_t s_frameBuffer[2][LCD_FB_SIZE] __attribute__((aligned(4)));

static void LCD_FlushDisplay(lv_display_t *disp, const lv_area_t *area,
                             uint8_t *color_p)
{
    lv_coord_t x1 = area->x1;
    lv_coord_t y1 = area->y1;
    lv_coord_t x2 = area->x2;
    lv_coord_t y2 = area->y2;
    uint8_t data[4];

    /* Column address window. */
    LcdDcLow();
    Spi0TxByte(ILI9341_CMD_COLADDR);
    LcdDcHigh();
    data[0] = (uint8_t)((x1 >> 8) & 0xFF);
    data[1] = (uint8_t)(x1 & 0xFF);
    data[2] = (uint8_t)((x2 >> 8) & 0xFF);
    data[3] = (uint8_t)(x2 & 0xFF);
    Spi0TxBufferFast(data, 4);

    /* Page address window. */
    LcdDcLow();
    Spi0TxByte(ILI9341_CMD_PAGEADDR);
    LcdDcHigh();
    data[0] = (uint8_t)((y1 >> 8) & 0xFF);
    data[1] = (uint8_t)(y1 & 0xFF);
    data[2] = (uint8_t)((y2 >> 8) & 0xFF);
    data[3] = (uint8_t)(y2 & 0xFF);
    Spi0TxBufferFast(data, 4);

    /* Memory write (GRAM). */
    LcdDcLow();
    Spi0TxByte(ILI9341_CMD_GRAM);
    LcdDcHigh();

    uint32_t send_size =
        (uint32_t)(x2 - x1 + 1) * (uint32_t)(y2 - y1 + 1) * LCD_FB_BPP;
    Spi0TxBufferFast(color_p, send_size);

    lv_disp_flush_ready(disp);
}

void lv_port_disp_init(void)
{
    /* Initialise the LCD controller (reset + boot sequence + landscape). */
    ILI9341_Init();

    lv_display_t *disp = lv_display_create(LCD_WIDTH, LCD_HEIGHT);

    lv_display_set_buffers(disp,
                           s_frameBuffer[0],
                           s_frameBuffer[1],
                           sizeof(s_frameBuffer[0]),
                           LV_DISPLAY_RENDER_MODE_FULL);

    lv_display_set_flush_cb(disp, LCD_FlushDisplay);
}
