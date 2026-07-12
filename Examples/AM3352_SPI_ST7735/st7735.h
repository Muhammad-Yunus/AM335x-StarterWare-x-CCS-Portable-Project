/*
 * st7735.h — Bare-metal ST7735 1.44" (128x128) driver for AM3352.
 *
 * Ported/condensed from STM32 BSP (WEACT WB55CG reference) down to a
 * self-contained driver that talks through ST7735_WriteCommand /
 * ST7735_WriteData / ST7735_DelayMs (implemented by main.c using the
 * existing McSPI0 + GPIO primitives).
 *
 * Datasheet sequences taken from ST7735 reference.
 */

#ifndef ST7735_H_
#define ST7735_H_

#include <stdint.h>

/* ---- Public IO contract (implemented in main.c) ---------------------- */

void ST7735_WriteCommand(uint8_t cmd);
void ST7735_WriteData(const uint8_t *data, uint32_t length);
void ST7735_DelayMs(uint32_t ms);

/* ---- Public API ------------------------------------------------------ */

void     ST7735_Init(void);
void     ST7735_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void     ST7735_FillScreen(uint16_t color);
void     ST7735_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void     ST7735_DrawHLine(uint16_t x, uint16_t y, uint16_t w, uint16_t color);
void     ST7735_DrawVLine(uint16_t x, uint16_t y, uint16_t h, uint16_t color);
void     ST7735_SetPixel(uint16_t x, uint16_t y, uint16_t color);
void     ST7735_DrawChar(uint16_t x, uint16_t y, char c, uint16_t fg, uint16_t bg, uint8_t size);
void     ST7735_DrawString(uint16_t x, uint16_t y, const char *s, uint16_t fg, uint16_t bg, uint8_t size);
void     ST7735_DrawBitmap(uint16_t x, uint16_t y, const uint8_t *bmp);
uint16_t ST7735_Width(void);
uint16_t ST7735_Height(void);

/* ---- Colors (RGB565) ------------------------------------------------- */

#define ST7735_BLACK   0x0000U
#define ST7735_BLUE    0x001FU
#define ST7735_RED     0xF800U
#define ST7735_GREEN   0x07E0U
#define ST7735_CYAN    0x07FFU
#define ST7735_MAGENTA 0xF81FU
#define ST7735_YELLOW  0xFFE0U
#define ST7735_WHITE   0xFFFFU

#endif /* ST7735_H_ */
