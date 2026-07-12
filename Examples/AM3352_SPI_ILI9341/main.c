/*
 * main.c — ILI9341 2.8" TFT demo on AM3352 SPI0.
 *
 *  SPI0 channel 0:
 *      CLK  = P9_22 (spi0_sclk)
 *      MOSI = P9_18 (spi0_d1)       -- driven via MCSPI_DATA_LINE_COMM_MODE_1
 *      CS   = P9_17 (spi0_cs0)      -- hardware CS, active low
 *  GPIO (data/command + reset for the LCD):
 *      DC  = P8_26 = GPIO1[29] (gpmc_csn0)
 *      RST = P8_19 = GPIO0[22] (gpmc_ad8)
 *
 *  Routing (antminer L3+, board ~ BeagleBone):
 *      ILI9341 LED pin is left floating/board-driven. We pull RST high
 *      after the boot sequence and leave DC controlled per-byte by
 *      spi helpers. No TE pin is wired.
 *
 *  Boot sequence:
 *      1) Init ARM IRQ + DMTimer7 (delay.h uses it).
 *      2) Bring up GPIO1[29] (DC) and GPIO0[22] (RST); pin-mux to mode 7.
 *      3) Bring up SPI0 controller @ ~10 MHz, 8-bit, mode 0.
 *      4) Initialize ILI9341 (reset pulse + command sequence from the
 *         STM32 reference driver).
 *      5) Run demo: color bands, shapes, text.
 *
 *  NOTE: delay.h routes through Sysdelay() which busy-waits on a flag set
 *  by DMTimer7's overflow IRQ. We MUST call IntAINTCInit() and
 *  IntMasterIRQEnable() first, or Sysdelay() hangs forever.
 */

#include "soc_AM335x.h"
#include "beaglebone.h"
#include "gpio_v2.h"
#include "hw_control_AM335x.h"
#include "hw_cm_per.h"
#include "hw_types.h"
#include "mcspi.h"
#include "interrupt.h"
#include "delay.h"

#include "ili9341.h"

/* ---------------------------------------------------------------------- */
/* Pin mapping                                                             */
/* ---------------------------------------------------------------------- */
#define DC_GPIO_BASE     (SOC_GPIO_1_REGS)
#define DC_GPIO_PIN      (29)
#define RST_GPIO_BASE    (SOC_GPIO_0_REGS)
#define RST_GPIO_PIN     (22)

#define SPI_BASE         (SOC_SPI_0_REGS)
#define SPI_CH           (0)
#define MCSPI_IN_CLK     (48000000u)
#define MCSPI_OUT_FREQ   (10000000u)   /* 10 MHz — fast enough for full-screen fill */

/* ---------------------------------------------------------------------- */
/* Function prototypes                                                     */
/* ---------------------------------------------------------------------- */
static void Spi0Init(void);
/* Spi0TxByte / Spi0TxBuffer are exported (no static) because ili9341.c
 * calls them through its low-level hooks. Spi0Init stays file-local. */
void Spi0TxByte(uint8_t b);
void Spi0TxBuffer(const uint8_t *buf, uint32_t len);

/* GPIO hooks exposed to ili9341.c. */
void LcdDcLow(void)  { GPIOPinWrite(DC_GPIO_BASE,  DC_GPIO_PIN,  GPIO_PIN_LOW);  }
void LcdDcHigh(void) { GPIOPinWrite(DC_GPIO_BASE,  DC_GPIO_PIN,  GPIO_PIN_HIGH); }
void LcdRstLow(void) { GPIOPinWrite(RST_GPIO_BASE, RST_GPIO_PIN, GPIO_PIN_LOW);  }
void LcdRstHigh(void){ GPIOPinWrite(RST_GPIO_BASE, RST_GPIO_PIN, GPIO_PIN_HIGH); }

/* ---------------------------------------------------------------------- */
/* Demo payloads                                                           */
/* ---------------------------------------------------------------------- */
static const uint16_t COLOR_BAND[] = {
    ILI9341_COLOR_RED,    ILI9341_COLOR_ORANGE, ILI9341_COLOR_YELLOW,
    ILI9341_COLOR_GREEN,  ILI9341_COLOR_CYAN,   ILI9341_COLOR_BLUE,
    ILI9341_COLOR_MAGENTA,ILI9341_COLOR_WHITE
};
#define COLOR_BAND_COUNT (sizeof(COLOR_BAND) / sizeof(COLOR_BAND[0]))

/* ---------------------------------------------------------------------- */
/* Demo steps                                                              */
/* ---------------------------------------------------------------------- */
static void Demo_ClearAndHeader(const char *title, uint16_t bg)
{
    ILI9341_FillScreen(bg);
    ILI9341_DrawString(10, 10, title, ILI9341_COLOR_WHITE, bg);
    ILI9341_DrawString(10, 30, "AM3352 SPI0 + ILI9341",
                       ILI9341_COLOR_YELLOW, bg);
}

static void Demo_ColorBands(void)
{
    Demo_ClearAndHeader("Color bands (8x40 px)", ILI9341_COLOR_BLACK);
    uint16_t band_h = (ILI9341_HEIGHT - 50) / COLOR_BAND_COUNT;
    uint32_t i;       /* C89: declare at top of block */
    uint16_t y;

    for (i = 0; i < COLOR_BAND_COUNT; i++)
    {
        y = (uint16_t)(50 + i * band_h);
        ILI9341_FillRect(0, y, ILI9341_WIDTH, band_h, COLOR_BAND[i]);
    }
    delay(1500);
}

static void Demo_Shapes(void)
{
    Demo_ClearAndHeader("Shapes", ILI9341_COLOR_BLACK);

    /* Rectangles, hollow + filled. */
    ILI9341_DrawRect(20, 60,  90, 60, ILI9341_COLOR_WHITE);
    ILI9341_FillRect(130, 60, 90, 60, ILI9341_COLOR_RED);

    /* Circles, hollow + filled. */
    ILI9341_DrawCircle(80,  180, 35, ILI9341_COLOR_CYAN);
    ILI9341_FillCircle(190, 180, 35, ILI9341_COLOR_GREEN);

    /* Diagonal line. */
    ILI9341_DrawLine(230, 60,  310, 200, ILI9341_COLOR_YELLOW);
    ILI9341_DrawLine(230, 200, 310, 60,  ILI9341_COLOR_MAGENTA);

    delay(1500);
}

static void Demo_Text(void)
{
    Demo_ClearAndHeader("Text", ILI9341_COLOR_BLUE);

    ILI9341_DrawString(10,  60,  "Hello AM3352!",  ILI9341_COLOR_WHITE,
                       ILI9341_COLOR_BLUE);
    ILI9341_DrawString(10,  80,  "ILI9341 240x320", ILI9341_COLOR_YELLOW,
                       ILI9341_COLOR_BLUE);
    ILI9341_DrawString(10, 100,  "SPI0 @ 10 MHz",   ILI9341_COLOR_GREEN,
                       ILI9341_COLOR_BLUE);

    ILI9341_DrawString(10, 140, "ABCDEFGHIJKLMN",
                       ILI9341_COLOR_WHITE, ILI9341_COLOR_BLUE);
    ILI9341_DrawString(10, 160, "OPQRSTUVWXYZ",
                       ILI9341_COLOR_WHITE, ILI9341_COLOR_BLUE);
    ILI9341_DrawString(10, 180, "abcdefghijklmn",
                       ILI9341_COLOR_CYAN, ILI9341_COLOR_BLUE);
    ILI9341_DrawString(10, 200, "opqrstuvwxyz",
                       ILI9341_COLOR_CYAN, ILI9341_COLOR_BLUE);
    ILI9341_DrawString(10, 220, "0123456789 !?.,:",
                       ILI9341_COLOR_WHITE, ILI9341_COLOR_BLUE);

    delay(1500);
}

static void Demo_PixelGrid(void)
{
    Demo_ClearAndHeader("Pixel art border", ILI9341_COLOR_BLACK);
    uint16_t y;       /* C89: declare at top of block */
    uint16_t x;

    /* Checker stripe on the left half. */
    for (y = 50; y < ILI9341_HEIGHT; y += 10)
        for (x = 0; x < ILI9341_WIDTH / 2; x += 10)
            ILI9341_DrawPixel(x, y,
                              (((x / 10) + (y / 10)) & 1)
                                ? ILI9341_COLOR_YELLOW
                                : ILI9341_COLOR_BLUE);

    /* Solid green panel right half. */
    ILI9341_FillRect(ILI9341_WIDTH / 2, 50,
                     ILI9341_WIDTH / 2, ILI9341_HEIGHT - 50,
                     ILI9341_COLOR_GREEN);

    ILI9341_DrawString(10, ILI9341_HEIGHT - 20,
                       "left: checker  | right: solid",
                       ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);

    delay(2000);
}

/* ---------------------------------------------------------------------- */
/* main                                                                    */
/* ---------------------------------------------------------------------- */
int main(void)
{
    /* IRQ path required by delay.h (Sysdelay uses DMTimer7 overflow IRQ). */
    IntAINTCInit();
    IntMasterIRQEnable();

    /* GPIO clocks + pin-mux for DC and RST to GPIO mode 7. */
    GPIO0ModuleClkConfig();
    GPIO1ModuleClkConfig();

    GpioPinMuxSetup(CONTROL_CONF_GPMC_CSN(0), CONTROL_CONF_MUXMODE(7));
    GpioPinMuxSetup(CONTROL_CONF_GPMC_AD(8),  CONTROL_CONF_MUXMODE(7));

    GPIOModuleEnable(DC_GPIO_BASE);
    GPIOModuleReset(DC_GPIO_BASE);
    GPIODirModeSet(DC_GPIO_BASE, DC_GPIO_PIN, GPIO_DIR_OUTPUT);

    GPIOModuleEnable(RST_GPIO_BASE);
    GPIOModuleReset(RST_GPIO_BASE);
    GPIODirModeSet(RST_GPIO_BASE, RST_GPIO_PIN, GPIO_DIR_OUTPUT);

    /* Both idle LOW until init; ILI9341_Init() drives RST HIGH after
     * a short delay. DC stays LOW (command interpretation) until
     * ILI9341_Draw* flips it before each data byte. */
    GPIOPinWrite(DC_GPIO_BASE,  DC_GPIO_PIN,  GPIO_PIN_LOW);
    GPIOPinWrite(RST_GPIO_BASE, RST_GPIO_PIN, GPIO_PIN_LOW);

    /* Bring up SPI0. */
    Spi0Init();

    /* Register DMTimer7 IRQ so delay() / Sysdelay() can return. */
    DelayTimerSetup();

    /* Initialise the LCD (RST pulse + boot register sequence). */
    ILI9341_Init();

    /* Demo loop. */
    while (1)
    {
        Demo_ColorBands();
        Demo_Shapes();
        Demo_Text();
        Demo_PixelGrid();
    }
}

/* ---------------------------------------------------------------------- */
/* SPI helpers — 8-bit master, manual CS per byte.                         */
/* ---------------------------------------------------------------------- */
static void Spi0Init(void)
{
    /* McSPI0 functional clock from CM_PER. */
    HWREG(SOC_CM_PER_REGS + CM_PER_SPI0_CLKCTRL) |=
        CM_PER_SPI0_CLKCTRL_MODULEMODE_ENABLE;
    while ((HWREG(SOC_CM_PER_REGS + CM_PER_SPI0_CLKCTRL) &
            CM_PER_SPI0_CLKCTRL_MODULEMODE) !=
           CM_PER_SPI0_CLKCTRL_MODULEMODE_ENABLE) { /* spin */ }

    /* Pin-mux P9_22/P9_18/P9_17 to SPI0 (mode 0). */
    GpioPinMuxSetup(CONTROL_CONF_SPI0_SCLK,
                    CONTROL_CONF_MUXMODE(0) | CONTROL_CONF_RXACTIVE);
    GpioPinMuxSetup(CONTROL_CONF_SPI0_D1,
                    CONTROL_CONF_MUXMODE(0) | CONTROL_CONF_RXACTIVE);
    GpioPinMuxSetup(CONTROL_CONF_SPI0_CS0,
                    CONTROL_CONF_MUXMODE(0) | CONTROL_CONF_RXACTIVE);

    /* Reset controller, then enable master mode. */
    McSPIReset(SPI_BASE);
    while ((HWREG(SPI_BASE + MCSPI_SYSSTATUS) & MCSPI_SYSSTATUS_RESETDONE)
           == 0) { /* spin */ }

    McSPIMasterModeEnable(SPI_BASE);
    McSPICSEnable(SPI_BASE);    /* route SPI0_CS to pin (PIN34=0) */

    /* SYSCONFIG: smart-idle, both clock domains active, no autoidle. */
    HWREG(SPI_BASE + MCSPI_SYSCONFIG) =
        (1u << MCSPI_SYSCONFIG_SIDLEMODE_SHIFT) |
        (1u << MCSPI_SYSCONFIG_CLOCKACTIVITY_SHIFT) |
        (0u << MCSPI_SYSCONFIG_AUTOIDLE_SHIFT);

    /* Channel 0 — single channel, transmit-and-receive, D1 output. */
    McSPIMasterModeConfig(SPI_BASE,
                          MCSPI_SINGLE_CH,
                          MCSPI_TX_RX_MODE,
                          MCSPI_DATA_LINE_COMM_MODE_1,
                          SPI_CH);

    McSPIWordLengthSet(SPI_BASE, MCSPI_WORD_LENGTH(8), SPI_CH);

    McSPIClkConfig(SPI_BASE,
                   MCSPI_IN_CLK,
                   MCSPI_OUT_FREQ,
                   SPI_CH,
                   MCSPI_CLK_MODE_0);

    McSPICSPolarityConfig(SPI_BASE, MCSPI_CS_POL_LOW, SPI_CH);

    McSPITxFIFOConfig(SPI_BASE, MCSPI_TX_FIFO_ENABLE, SPI_CH);
    McSPIRxFIFOConfig(SPI_BASE, MCSPI_RX_FIFO_ENABLE, SPI_CH);

    McSPIChannelEnable(SPI_BASE, SPI_CH);
}

void Spi0TxByte(uint8_t b)
{
    McSPICSAssert(SPI_BASE, SPI_CH);
    McSPITransmitData(SPI_BASE, b, SPI_CH);

    /* Wait until the shift register has drained (TXS = Tx Register Empty). */
    while (!((McSPIChannelStatusGet(SPI_BASE, SPI_CH)) &
             MCSPI_CH0STAT_TXS)) { /* spin */ }

    /* Drain RX FIFO so it never overruns. */
    (void)McSPIReceiveData(SPI_BASE, SPI_CH);

    McSPICSDeAssert(SPI_BASE, SPI_CH);
}

void Spi0TxBuffer(const uint8_t *buf, uint32_t len)
{
    /* The ILI9341 driver pushes bytes in blocks (rows of pixels). Most
     * blocks are 4-128 bytes; treat each byte with manual CS for
     * correctness against the LCD protocol (CS must go high between
     * tokens if a DC change is required, but for DC=high bulk pixel
     * streams we can keep CS asserted). For simplicity (and to mirror
     * the Spi0TxByte behaviour), we re-assert CS once per byte. */
    uint32_t i;       /* C89: declare at top of block */

    for (i = 0; i < len; i++)
        Spi0TxByte(buf[i]);
}
