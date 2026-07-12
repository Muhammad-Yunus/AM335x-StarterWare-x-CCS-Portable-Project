/*
 * st7735.c — Bare-metal ST7735 1.44" driver implementation.
 *
 * Panel: 1.44" (128x128). Initialization sequence, gamma curves,
 * and MADCTL values adapted from the WeAct WB55CG reference driver
 * (STMicroelectronics BSD-3-Clause).
 *
 * The driver calls three IO functions exposed by main.c:
 *      ST7735_WriteCommand(cmd)              -- DC=LOW + 1 byte on SPI
 *      ST7735_WriteData(buf, len)            -- DC=HIGH + len bytes on SPI
 *      ST7735_DelayMs(ms)                    -- blocking delay
 */

#include "st7735.h"

/* ---- ST7735 command set (subset) ------------------------------------- */

#define ST7735_SWRESET              0x01U
#define ST7735_SLPOUT               0x11U
#define ST7735_NORON                0x13U
#define ST7735_INVOFF               0x20U
#define ST7735_INVON                0x21U
#define ST7735_DISPOFF              0x28U
#define ST7735_DISPON               0x29U
#define ST7735_CASET                0x2AU
#define ST7735_RASET                0x2BU
#define ST7735_RAMWR                0x2CU
#define ST7735_TEOFF                0x34U
#define ST7735_MADCTL               0x36U
#define ST7735_COLMOD               0x3AU

#define ST7735_FRMCTR1              0xB1U
#define ST7735_FRMCTR2              0xB2U
#define ST7735_FRMCTR3              0xB3U
#define ST7735_INVCTR               0xB4U

#define ST7735_PWCTR1               0xC0U
#define ST7735_PWCTR2               0xC1U
#define ST7735_PWCTR3               0xC2U
#define ST7735_PWCTR4               0xC3U
#define ST7735_PWCTR5               0xC4U
#define ST7735_VMCTR1               0xC5U

#define ST7735_GMCTRP1              0xE0U
#define ST7735_GMCTRN1              0xE1U

/* ---- Panel geometry --------------------------------------------------- */

#define ST7735_PANEL_W              128U
#define ST7735_PANEL_H              128U
/* 1.44" panel needs a +2 X / +1 Y offset for full visible area in portrait. */
#define ST7735_XOFF                2U
#define ST7735_YOFF                1U

/* ---- Tiny helpers ---------------------------------------------------- */

static void write_cmd(uint8_t cmd)
{
    ST7735_WriteCommand(cmd);
}

static void write_data(const uint8_t *p, uint32_t n)
{
    ST7735_WriteData(p, n);
}

static void write_byte(uint8_t v)
{
    ST7735_WriteData(&v, 1);
}

/* MADCTL byte: MY (bit7) + MX (bit6) for portrait orientation,
 * plus RGB order (bit3).  bit3=0 -> BGR, bit3=1 -> RGB.
 * The 1.44" panel we have requires RGB order, so set bit3
 * -> 0xC8.  Without this swap, ST7735_RED renders as blue,
 * ST7735_BLUE as red, ST7735_CYAN as yellow; only ST7735_GREEN
 * (symmetric in RGB565) looks correct either way. */
static const uint8_t madctl_value = 0xC8U;

/* ---- Public API ------------------------------------------------------ */

uint16_t ST7735_Width(void)  { return (uint16_t)ST7735_PANEL_W; }
uint16_t ST7735_Height(void) { return (uint16_t)ST7735_PANEL_H; }

void ST7735_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint8_t b[4];

    x0 = (uint16_t)(x0 + ST7735_XOFF);
    x1 = (uint16_t)(x1 + ST7735_XOFF);
    y0 = (uint16_t)(y0 + ST7735_YOFF);
    y1 = (uint16_t)(y1 + ST7735_YOFF);

    write_cmd(ST7735_CASET);
    b[0] = (uint8_t)(x0 >> 8);
    b[1] = (uint8_t)(x0 & 0xFFU);
    b[2] = (uint8_t)(x1 >> 8);
    b[3] = (uint8_t)(x1 & 0xFFU);
    write_data(b, 4);

    write_cmd(ST7735_RASET);
    b[0] = (uint8_t)(y0 >> 8);
    b[1] = (uint8_t)(y0 & 0xFFU);
    b[2] = (uint8_t)(y1 >> 8);
    b[3] = (uint8_t)(y1 & 0xFFU);
    write_data(b, 4);

    write_cmd(ST7735_RAMWR);
}

void ST7735_SetPixel(uint16_t x, uint16_t y, uint16_t color)
{
    uint8_t b[2];

    if (x >= ST7735_PANEL_W || y >= ST7735_PANEL_H)
    {
        return;
    }

    ST7735_SetWindow(x, y, x, y);
    b[0] = (uint8_t)(color >> 8);
    b[1] = (uint8_t)(color & 0xFFU);
    write_data(b, 2);
}

void ST7735_DrawHLine(uint16_t x, uint16_t y, uint16_t w, uint16_t color)
{
    uint8_t  hi = (uint8_t)(color >> 8);
    uint8_t  lo = (uint8_t)(color & 0xFFU);
    uint32_t i;
    uint8_t  buf[2];

    if (y >= ST7735_PANEL_H) return;
    if ((uint32_t)x + w > ST7735_PANEL_W) w = (uint16_t)(ST7735_PANEL_W - x);

    ST7735_SetWindow(x, y, (uint16_t)(x + w - 1U), y);
    buf[0] = hi;
    buf[1] = lo;
    for (i = 0; i < w; i++)
    {
        write_data(buf, 2);
    }
}

void ST7735_DrawVLine(uint16_t x, uint16_t y, uint16_t h, uint16_t color)
{
    uint16_t yy;

    if (x >= ST7735_PANEL_W) return;
    if ((uint32_t)y + h > ST7735_PANEL_H) h = (uint16_t)(ST7735_PANEL_H - y);

    for (yy = y; yy < (uint16_t)(y + h); yy++)
    {
        ST7735_SetPixel(x, yy, color);
    }
}

void ST7735_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    uint32_t i;
    uint32_t total;
    uint8_t  buf[2];
    uint8_t  hi = (uint8_t)(color >> 8);
    uint8_t  lo = (uint8_t)(color & 0xFFU);

    if (x >= ST7735_PANEL_W || y >= ST7735_PANEL_H) return;
    if ((uint32_t)x + w > ST7735_PANEL_W)  w = (uint16_t)(ST7735_PANEL_W - x);
    if ((uint32_t)y + h > ST7735_PANEL_H) h = (uint16_t)(ST7735_PANEL_H - y);

    ST7735_SetWindow(x, y, (uint16_t)(x + w - 1U), (uint16_t)(y + h - 1U));

    buf[0] = hi;
    buf[1] = lo;
    total = (uint32_t)w * (uint32_t)h;
    for (i = 0; i < total; i++)
    {
        write_data(buf, 2);
    }
}

void ST7735_FillScreen(uint16_t color)
{
    ST7735_FillRect(0, 0, ST7735_PANEL_W, ST7735_PANEL_H, color);
}

/* ---- 5x7 font (ASCII 0x20..0x7E) ------------------------------------- */

static const uint8_t font5x7[95][5] = {
    {0x00,0x00,0x00,0x00,0x00}, /* ' ' 0x20 */
    {0x00,0x00,0x5F,0x00,0x00}, /* '!' */
    {0x00,0x07,0x00,0x07,0x00}, /* '"' */
    {0x14,0x7F,0x14,0x7F,0x14}, /* '#' */
    {0x24,0x2A,0x7F,0x2A,0x12}, /* '$' */
    {0x23,0x13,0x08,0x64,0x62}, /* '%' */
    {0x36,0x49,0x55,0x22,0x50}, /* '&' */
    {0x00,0x05,0x03,0x00,0x00}, /* '\'' */
    {0x00,0x1C,0x22,0x41,0x00}, /* '(' */
    {0x00,0x41,0x22,0x1C,0x00}, /* ')' */
    {0x08,0x2A,0x1C,0x2A,0x08}, /* '*' */
    {0x08,0x08,0x3E,0x08,0x08}, /* '+' */
    {0x00,0x50,0x30,0x00,0x00}, /* ',' */
    {0x08,0x08,0x08,0x08,0x08}, /* '-' */
    {0x00,0x60,0x60,0x00,0x00}, /* '.' */
    {0x20,0x10,0x08,0x04,0x02}, /* '/' */
    {0x3E,0x51,0x49,0x45,0x3E}, /* '0' */
    {0x00,0x42,0x7F,0x40,0x00}, /* '1' */
    {0x42,0x61,0x51,0x49,0x46}, /* '2' */
    {0x21,0x41,0x45,0x4B,0x31}, /* '3' */
    {0x18,0x14,0x12,0x7F,0x10}, /* '4' */
    {0x27,0x45,0x45,0x45,0x39}, /* '5' */
    {0x3C,0x4A,0x49,0x49,0x30}, /* '6' */
    {0x01,0x71,0x09,0x05,0x03}, /* '7' */
    {0x36,0x49,0x49,0x49,0x36}, /* '8' */
    {0x06,0x49,0x49,0x29,0x1E}, /* '9' */
    {0x00,0x36,0x36,0x00,0x00}, /* ':' */
    {0x00,0x56,0x36,0x00,0x00}, /* ';' */
    {0x00,0x08,0x14,0x22,0x41}, /* '<' */
    {0x14,0x14,0x14,0x14,0x14}, /* '=' */
    {0x41,0x22,0x14,0x08,0x00}, /* '>' */
    {0x02,0x01,0x51,0x09,0x06}, /* '?' */
    {0x32,0x49,0x79,0x41,0x3E}, /* '@' */
    {0x7E,0x11,0x11,0x11,0x7E}, /* 'A' */
    {0x7F,0x49,0x49,0x49,0x36}, /* 'B' */
    {0x3E,0x41,0x41,0x41,0x22}, /* 'C' */
    {0x7F,0x41,0x41,0x22,0x1C}, /* 'D' */
    {0x7F,0x49,0x49,0x49,0x41}, /* 'E' */
    {0x7F,0x09,0x09,0x01,0x01}, /* 'F' */
    {0x3E,0x41,0x41,0x51,0x32}, /* 'G' */
    {0x7F,0x08,0x08,0x08,0x7F}, /* 'H' */
    {0x00,0x41,0x7F,0x41,0x00}, /* 'I' */
    {0x20,0x40,0x41,0x3F,0x01}, /* 'J' */
    {0x7F,0x08,0x14,0x22,0x41}, /* 'K' */
    {0x7F,0x40,0x40,0x40,0x40}, /* 'L' */
    {0x7F,0x02,0x04,0x02,0x7F}, /* 'M' */
    {0x7F,0x04,0x08,0x10,0x7F}, /* 'N' */
    {0x3E,0x41,0x41,0x41,0x3E}, /* 'O' */
    {0x7F,0x09,0x09,0x09,0x06}, /* 'P' */
    {0x3E,0x41,0x51,0x21,0x5E}, /* 'Q' */
    {0x7F,0x09,0x19,0x29,0x46}, /* 'R' */
    {0x46,0x49,0x49,0x49,0x31}, /* 'S' */
    {0x01,0x01,0x7F,0x01,0x01}, /* 'T' */
    {0x3F,0x40,0x40,0x40,0x3F}, /* 'U' */
    {0x1F,0x20,0x40,0x20,0x1F}, /* 'V' */
    {0x7F,0x20,0x18,0x20,0x7F}, /* 'W' */
    {0x63,0x14,0x08,0x14,0x63}, /* 'X' */
    {0x03,0x04,0x78,0x04,0x03}, /* 'Y' */
    {0x61,0x51,0x49,0x45,0x43}, /* 'Z' */
    {0x00,0x00,0x7F,0x41,0x41}, /* '[' */
    {0x02,0x04,0x08,0x10,0x20}, /* '\\' */
    {0x41,0x41,0x7F,0x00,0x00}, /* ']' */
    {0x04,0x02,0x01,0x02,0x04}, /* '^' */
    {0x40,0x40,0x40,0x40,0x40}, /* '_' */
    {0x00,0x01,0x02,0x04,0x00}, /* '`' */
    {0x20,0x54,0x54,0x54,0x78}, /* 'a' */
    {0x7F,0x48,0x44,0x44,0x38}, /* 'b' */
    {0x38,0x44,0x44,0x44,0x20}, /* 'c' */
    {0x38,0x44,0x44,0x48,0x7F}, /* 'd' */
    {0x38,0x54,0x54,0x54,0x18}, /* 'e' */
    {0x08,0x7E,0x09,0x01,0x02}, /* 'f' */
    {0x08,0x14,0x54,0x54,0x3C}, /* 'g' */
    {0x7F,0x08,0x04,0x04,0x78}, /* 'h' */
    {0x00,0x44,0x7D,0x40,0x00}, /* 'i' */
    {0x20,0x40,0x44,0x3D,0x00}, /* 'j' */
    {0x00,0x7F,0x10,0x28,0x44}, /* 'k' */
    {0x00,0x41,0x7F,0x40,0x00}, /* 'l' */
    {0x7C,0x04,0x18,0x04,0x78}, /* 'm' */
    {0x7C,0x08,0x04,0x04,0x78}, /* 'n' */
    {0x38,0x44,0x44,0x44,0x38}, /* 'o' */
    {0x7C,0x14,0x14,0x14,0x08}, /* 'p' */
    {0x08,0x14,0x14,0x18,0x7C}, /* 'q' */
    {0x7C,0x08,0x04,0x04,0x08}, /* 'r' */
    {0x48,0x54,0x54,0x54,0x20}, /* 's' */
    {0x04,0x3F,0x44,0x40,0x20}, /* 't' */
    {0x3C,0x40,0x40,0x20,0x7C}, /* 'u' */
    {0x1C,0x20,0x40,0x20,0x1C}, /* 'v' */
    {0x3C,0x40,0x30,0x40,0x3C}, /* 'w' */
    {0x44,0x28,0x10,0x28,0x44}, /* 'x' */
    {0x0C,0x50,0x50,0x50,0x3C}, /* 'y' */
    {0x44,0x64,0x54,0x4C,0x44}, /* 'z' */
    {0x00,0x08,0x36,0x41,0x00}, /* '{' */
    {0x00,0x00,0x7F,0x00,0x00}, /* '|' */
    {0x00,0x41,0x36,0x08,0x00}, /* '}' */
    {0x08,0x04,0x08,0x10,0x08}, /* '~' 0x7E */
};

void ST7735_DrawChar(uint16_t x, uint16_t y, char c, uint16_t fg, uint16_t bg, uint8_t size)
{
    uint8_t  ch;
    uint16_t i, j, line;
    uint8_t  bits;
    uint16_t color;

    if (c < 0x20 || c > 0x7E) return;
    ch = (uint8_t)(c - 0x20);

    for (i = 0; i < 5; i++)
    {
        line = ((uint16_t)font5x7[ch][i]);
        for (j = 0; j < 8; j++)
        {
            bits = (uint8_t)((line >> j) & 0x01U);
            color = bits ? fg : bg;
            ST7735_FillRect((uint16_t)(x + i * size),
                             (uint16_t)(y + j * size),
                             size, size, color);
        }
    }
    /* 1-pixel column gutter (skip on size 1 to save flash). */
    if (size > 1)
    {
        ST7735_FillRect((uint16_t)(x + 5 * size), y, size, (uint16_t)(8 * size), bg);
    }
}

void ST7735_DrawString(uint16_t x, uint16_t y, const char *s, uint16_t fg, uint16_t bg, uint8_t size)
{
    while (*s)
    {
        ST7735_DrawChar(x, y, *s, fg, bg, size);
        x = (uint16_t)(x + 6 * size);
        s++;
    }
}

/* ---- Bitmap (BMP file bytes dumped to a const array) ----------------- */

void ST7735_DrawBitmap(uint16_t x, uint16_t y, const uint8_t *bmp)
{
    uint32_t idx;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t row;
    const uint8_t *pbmp;

    if (bmp == 0) return;

    /* BMP header offsets (little-endian). */
    idx    = ((uint32_t)bmp[10]      )
           | ((uint32_t)bmp[11] <<  8)
           | ((uint32_t)bmp[12] << 16)
           | ((uint32_t)bmp[13] << 24);
    width  = ((uint32_t)bmp[18]      )
           | ((uint32_t)bmp[19] <<  8)
           | ((uint32_t)bmp[20] << 16)
           | ((uint32_t)bmp[21] << 24);
    height = ((uint32_t)bmp[22]      )
           | ((uint32_t)bmp[23] <<  8)
           | ((uint32_t)bmp[24] << 16)
           | ((uint32_t)bmp[25] << 24);

    pbmp   = bmp + idx;

    /* Clip to panel. */
    if (x >= ST7735_PANEL_W || y >= ST7735_PANEL_H) return;
    if ((uint32_t)x + width  > ST7735_PANEL_W) width  = ST7735_PANEL_W - x;
    if ((uint32_t)y + height > ST7735_PANEL_H) height = ST7735_PANEL_H - y;

    /* BMP row stride: 16 bpp = 2 bytes/pixel, padded to 4 bytes/row. */
    stride = ((width * 2U) + 3U) & ~3U;

    /* BMP stores rows bottom-up, ST7735 wants top-down.
     * Stream one window-row at a time, indexing the BMP row from
     * (height - 1 - row). Also swap each pixel's byte order — BMP
     * is little-endian, ST7735 RAM write is MSB-first. */
    for (row = 0; row < height; row++)
    {
        uint32_t col;
        const uint8_t *prow = pbmp + (height - 1U - row) * stride;

        ST7735_SetWindow(x, (uint16_t)(y + row),
                         (uint16_t)(x + width - 1U),
                         (uint16_t)(y + row));

        for (col = 0; col + 1U < width * 2U; col += 2U)
        {
            uint8_t pair[2];
            pair[0] = prow[col + 1U];
            pair[1] = prow[col];
            write_data(pair, 2);
        }
    }
}

/* ---- Init sequence --------------------------------------------------- */

void ST7735_Init(void)
{
    uint8_t tmp;

    /* Hardware reset pulse (datasheet: RST low > 10us, then high). */
    ST7735_DelayMs(10);

    /* SWRESET */
    write_cmd(ST7735_SWRESET);
    ST7735_DelayMs(120);

    /* Second SWRESET (matches reference init). */
    write_cmd(ST7735_SWRESET);
    ST7735_DelayMs(120);

    /* SLPOUT */
    write_cmd(ST7735_SLPOUT);
    ST7735_DelayMs(120);

    /* FRMCTR1 - normal mode */
    write_cmd(ST7735_FRMCTR1);
    tmp = 0x01U; write_data(&tmp, 1);
    tmp = 0x2CU; write_data(&tmp, 1);
    tmp = 0x2DU; write_data(&tmp, 1);

    /* FRMCTR2 - idle mode */
    write_cmd(ST7735_FRMCTR2);
    tmp = 0x01U; write_data(&tmp, 1);
    tmp = 0x2CU; write_data(&tmp, 1);
    tmp = 0x2DU; write_data(&tmp, 1);

    /* FRMCTR3 - partial mode (full + 8-color pairs) */
    write_cmd(ST7735_FRMCTR3);
    tmp = 0x01U; write_data(&tmp, 1);
    tmp = 0x2CU; write_data(&tmp, 1);
    tmp = 0x2DU; write_data(&tmp, 1);
    tmp = 0x01U; write_data(&tmp, 1);
    tmp = 0x2CU; write_data(&tmp, 1);
    tmp = 0x2DU; write_data(&tmp, 1);

    /* INVCTR - line inversion */
    write_cmd(ST7735_INVCTR);
    tmp = 0x07U; write_data(&tmp, 1);

    /* PWCTR1 */
    write_cmd(ST7735_PWCTR1);
    tmp = 0xA2U; write_data(&tmp, 1);
    tmp = 0x02U; write_data(&tmp, 1);
    tmp = 0x84U; write_data(&tmp, 1);

    /* PWCTR2 */
    write_cmd(ST7735_PWCTR2);
    tmp = 0xC5U; write_data(&tmp, 1);

    /* PWCTR3 */
    write_cmd(ST7735_PWCTR3);
    tmp = 0x0AU; write_data(&tmp, 1);
    tmp = 0x00U; write_data(&tmp, 1);

    /* PWCTR4 */
    write_cmd(ST7735_PWCTR4);
    tmp = 0x8AU; write_data(&tmp, 1);
    tmp = 0x2AU; write_data(&tmp, 1);

    /* PWCTR5 */
    write_cmd(ST7735_PWCTR5);
    tmp = 0x8AU; write_data(&tmp, 1);
    tmp = 0xEEU; write_data(&tmp, 1);

    /* VMCTR1 */
    write_cmd(ST7735_VMCTR1);
    tmp = 0x0EU; write_data(&tmp, 1);

    /* 1.44" panel -> inversion off (matches reference init sequence). */
    write_cmd(ST7735_INVOFF);

    /* COLMOD = RGB565 */
    write_cmd(ST7735_COLMOD);
    tmp = 0x05U; write_data(&tmp, 1);

    /* GMCTRP1 (positive gamma). */
    write_cmd(ST7735_GMCTRP1);
    {
        static const uint8_t p[16] = {
            0x02U, 0x1CU, 0x07U, 0x12U, 0x37U, 0x32U, 0x29U, 0x2DU,
            0x29U, 0x25U, 0x2BU, 0x39U, 0x00U, 0x01U, 0x03U, 0x10U,
        };
        write_data(p, 16);
    }

    /* GMCTRN1 (negative gamma). */
    write_cmd(ST7735_GMCTRN1);
    {
        static const uint8_t n[16] = {
            0x03U, 0x1DU, 0x07U, 0x06U, 0x2EU, 0x2CU, 0x29U, 0x2DU,
            0x2EU, 0x2EU, 0x37U, 0x3FU, 0x00U, 0x00U, 0x02U, 0x10U,
        };
        write_data(n, 16);
    }

    /* NORON (normal display) */
    write_cmd(ST7735_NORON);
    ST7735_DelayMs(10);

    /* MADCTL: portrait + RGB order */
    write_cmd(ST7735_MADCTL);
    write_data(&madctl_value, 1);

    /* Tearing effect off */
    write_cmd(ST7735_TEOFF);

    /* DISPON */
    write_cmd(ST7735_DISPON);
    ST7735_DelayMs(10);
}
