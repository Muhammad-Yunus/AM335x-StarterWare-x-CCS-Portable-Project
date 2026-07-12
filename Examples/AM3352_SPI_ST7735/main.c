/*
 * main.c — AM3352 SPI0 to ST7735 1.44" (128x128) LCD demo.
 *
 *  Bus (kept identical to AM3352_SPI_TX scaffold):
 *      DC  = P8_26 = GPIO1[29] = gpmc_csn0    -- data/command select
 *      RST = P8_19 = GPIO0[22] = gpmc_ad8     -- hardware reset
 *      CLK = P9_22 = spi0_sclk
 *      MOSI= P9_18 = spi0_d1  (D1 via DATA_LINE_COMM_MODE_1)
 *      CS  = P9_17 = spi0_cs0 (hardware, active-low, manual per CS pin)
 *
 *  MCK = 48 MHz, SPI output ~ 4 MHz (faster than the 100 kHz baseline
 *  because the LCD's max SCL is up to ~32 MHz; runs cool at 4 MHz).
 *
 *  The demo paints four solid color bands and prints two lines of text,
 *  then loops forever.
 */

#include "soc_AM335x.h"
#include "beaglebone.h"
#include "gpio_v2.h"
#include "hw_control_AM335x.h"
#include "hw_cm_per.h"
#include "hw_types.h"
#include "interrupt.h"
#include "mcspi.h"
#include "delay.h"

#include "st7735.h"

/* Splash bitmap (128x128, RGB565) — generated from logo.bmp, see
 * README "Membuat atau Mengganti Bitmap Splash". */
extern const unsigned char logo_128_128[];

/* --- GPIO: DC / RST ---------------------------------------------------- */
#define DC_GPIO_BASE    (SOC_GPIO_1_REGS)
#define DC_GPIO_PIN     (29)
#define RST_GPIO_BASE   (SOC_GPIO_0_REGS)
#define RST_GPIO_PIN    (22)

/* --- SPI0 -------------------------------------------------------------- */
#define SPI_BASE        (SOC_SPI_0_REGS)
#define SPI_CH          (0)
#define MCSPI_IN_CLK    (48000000u)
#define MCSPI_OUT_FREQ  (4000000u)         /* ~4 MHz, well below LCD max  */

static void     Delay(volatile unsigned int count);
static void     Spi0Init(void);
static void     Spi0TxByte(unsigned char byte);
static void     Spi0TxBuffer(const unsigned char *buf, unsigned int len);
static void     Spi0CsLow(void);
static void     Spi0CsHigh(void);
static void     LcdResetPulse(void);

static void     SpiWriteCommand(uint8_t cmd)
{
    /* DC LOW + assert CS + 1 byte + deassert. */
    GPIOPinWrite(DC_GPIO_BASE, DC_GPIO_PIN, GPIO_PIN_LOW);
    Spi0CsLow();
    Spi0TxByte((unsigned char)cmd);
    Spi0CsHigh();
}

static void     SpiWriteData(const unsigned char *buf, unsigned int len)
{
    /* DC HIGH + assert CS + len bytes + deassert. */
    GPIOPinWrite(DC_GPIO_BASE, DC_GPIO_PIN, GPIO_PIN_HIGH);
    Spi0CsLow();
    Spi0TxBuffer(buf, len);
    Spi0CsHigh();
}

static void     SpiDelayMs(unsigned int ms)
{
    /* Routes through StarterWare's DMTimer7 overflow IRQ. Requires
     * DelayTimerSetup() + IntAINTCInit() + IntMasterIRQEnable() at boot. */
    delay(ms);
}

/* Wires the driver expects. */
void    ST7735_WriteCommand(uint8_t cmd)              { SpiWriteCommand(cmd); }
void    ST7735_WriteData(const uint8_t *p, uint32_t n)
{
    SpiWriteData((const unsigned char *)p, (unsigned int)n);
}
void    ST7735_DelayMs(uint32_t ms)                    { SpiDelayMs((unsigned int)ms); }

/* -------------------------------------------------------------------- */

int main(void)
{
    /* AINTC must be up before delay()'s DMTimer7 IRQ can fire. */
    IntAINTCInit();
    IntMasterIRQEnable();
    DelayTimerSetup();

    /* GPIO clocks. */
    GPIO0ModuleClkConfig();
    GPIO1ModuleClkConfig();

    /* Pinmux DC, RST to GPIO mode (mode 7). */
    GpioPinMuxSetup(CONTROL_CONF_GPMC_CSN(0), CONTROL_CONF_MUXMODE(7));
    GpioPinMuxSetup(CONTROL_CONF_GPMC_AD(8),  CONTROL_CONF_MUXMODE(7));

    GPIOModuleEnable(DC_GPIO_BASE);
    GPIOModuleReset(DC_GPIO_BASE);
    GPIODirModeSet(DC_GPIO_BASE, DC_GPIO_PIN, GPIO_DIR_OUTPUT);

    GPIOModuleEnable(RST_GPIO_BASE);
    GPIOModuleReset(RST_GPIO_BASE);
    GPIODirModeSet(RST_GPIO_BASE, RST_GPIO_PIN, GPIO_DIR_OUTPUT);

    /* Idle LOW. */
    GPIOPinWrite(DC_GPIO_BASE,  DC_GPIO_PIN,  GPIO_PIN_LOW);
    GPIOPinWrite(RST_GPIO_BASE, RST_GPIO_PIN, GPIO_PIN_LOW);

    /* Bring up SPI0. */
    Spi0Init();

    /* Hardware-reset the LCD panel before driving it. */
    LcdResetPulse();

    /* Initialize the ST7735 1.44" controller. */
    ST7735_Init();

    /* ----- Splash: Texas Instruments logo, full 128x128 panel --------- */
    ST7735_DrawBitmap(0, 0, logo_128_128);

    /* Hold the logo for ~2 seconds. (Reference: brightness ramp 0..1000 ms;
     * we skip backlight PWM and just dwell.) */
    delay(2000);

    /* ----- 2 s up-counter with red progress bar ----------------------- *
     * Reproduces lcd.c::LCD_Test() pattern: every 10 ms, advance a
     * counter and grow a red bar across the bottom 3 rows.            */
    {
        unsigned step;
        for (step = 0; step < 200U; step++)
        {
            unsigned percent  = step / 2U;            /* 0..99 % */
            unsigned bar_pixels = (step * 128U) / 200U; /* 0..128 */

            char text[8];
            text[0] = (char)('0' + (percent / 100U) % 10U);
            text[1] = (char)('0' + (percent /  10U) % 10U);
            text[2] = (char)('0' + (percent /   1U) % 10U);
            text[3] = '\0';

            /* Counter overlay on top of logo, right-aligned at top-right. */
            ST7735_DrawString(94, 4, text, ST7735_WHITE, ST7735_BLACK, 1);

            /* White progress bar across bottom 3 rows. */
            ST7735_FillRect(0, 125, (uint16_t)bar_pixels, 3, ST7735_WHITE);

            delay(10);
        }
    }

    /* ----- Final info screen on a black background ------------------- */
    ST7735_FillScreen(ST7735_BLACK);

    ST7735_DrawString(4,  4, "Antminer L3+",     ST7735_RED,  ST7735_BLACK, 1);
    ST7735_DrawString(4, 20, "AM3352 / SPI0",    ST7735_BLUE,  ST7735_BLACK, 1);
    ST7735_DrawString(4, 36, "ST7735 1.44 LCD TFT", ST7735_CYAN, ST7735_BLACK, 1);
    ST7735_DrawString(4, 52, "RGB565 OK",        ST7735_GREEN,  ST7735_BLACK, 1);

    /* Loop: keep panel on. */
    while (1)
    {
        Delay(0x3FFFF);
    }
}

/* --------------------------------------------------------------------- */
/* SPI helpers                                                            */
/* --------------------------------------------------------------------- */

static void LcdResetPulse(void)
{
    /* Assert reset, hold, deassert, settle. */
    GPIOPinWrite(RST_GPIO_BASE, RST_GPIO_PIN, GPIO_PIN_LOW);
    SpiDelayMs(50);
    GPIOPinWrite(RST_GPIO_BASE, RST_GPIO_PIN, GPIO_PIN_HIGH);
    SpiDelayMs(50);
}

static void Spi0CsLow(void)
{
    McSPICSAssert(SPI_BASE, SPI_CH);
}

static void Spi0CsHigh(void)
{
    McSPICSDeAssert(SPI_BASE, SPI_CH);
}

static void Spi0Init(void)
{
    /* Enable McSPI0 functional clock from CM_PER. */
    HWREG(SOC_CM_PER_REGS + CM_PER_SPI0_CLKCTRL) |=
        CM_PER_SPI0_CLKCTRL_MODULEMODE_ENABLE;
    while ((HWREG(SOC_CM_PER_REGS + CM_PER_SPI0_CLKCTRL) &
            CM_PER_SPI0_CLKCTRL_MODULEMODE) !=
           CM_PER_SPI0_CLKCTRL_MODULEMODE_ENABLE) { /* spin */ }

    /* Pinmux P9_22/18/17 (spi0_* pads) -> mode 0 = SPI0. */
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
    McSPICSEnable(SPI_BASE);           /* route SPI0_CS to pin (PIN34=0) */

    /* SYSCONFIG: free-running clock, no smart-idle, no autoidle. */
    HWREG(SPI_BASE + MCSPI_SYSCONFIG) =
        (1u << MCSPI_SYSCONFIG_SIDLEMODE_SHIFT) |     /* smart-idle */
        (1u << MCSPI_SYSCONFIG_CLOCKACTIVITY_SHIFT) | /* both clocks */
        (0u << MCSPI_SYSCONFIG_AUTOIDLE_SHIFT);

    /* Single-channel master, transmit-and-receive on D0/D1. */
    McSPIMasterModeConfig(SPI_BASE,
                          MCSPI_SINGLE_CH,           /* channelMode */
                          MCSPI_TX_RX_MODE,          /* trMode      */
                          MCSPI_DATA_LINE_COMM_MODE_1, /* pinMode   */
                          SPI_CH);

    McSPIWordLengthSet(SPI_BASE, MCSPI_WORD_LENGTH(8), SPI_CH);
    McSPIClkConfig(SPI_BASE, MCSPI_IN_CLK, MCSPI_OUT_FREQ, SPI_CH, MCSPI_CLK_MODE_0);
    McSPICSPolarityConfig(SPI_BASE, MCSPI_CS_POL_LOW, SPI_CH);

    McSPITxFIFOConfig(SPI_BASE, MCSPI_TX_FIFO_ENABLE, SPI_CH);
    McSPIRxFIFOConfig(SPI_BASE, MCSPI_RX_FIFO_ENABLE, SPI_CH);
    McSPIChannelEnable(SPI_BASE, SPI_CH);
}

/*
 * Polling "TX register empty" is the right primitive for byte-accurate
 * timing with manual CS: see comment block on Spi0TxByte below.
 */
static void Spi0TxByte(unsigned char byte)
{
    McSPITransmitData(SPI_BASE, byte, SPI_CH);
    while (!((McSPIChannelStatusGet(SPI_BASE, SPI_CH)) &
             MCSPI_CH0STAT_TXS)) { /* spin */ }
    (void)McSPIReceiveData(SPI_BASE, SPI_CH);
}

/* Burst transmit: keep CS low, pump bytes through FIFO. */
static void Spi0TxBuffer(const unsigned char *buf, unsigned int len)
{
    while (len--)
    {
        Spi0TxByte(*buf++);
    }
}

/* --------------------------------------------------------------------- */

static void Delay(volatile unsigned int count)
{
    while (count--) ;
}
