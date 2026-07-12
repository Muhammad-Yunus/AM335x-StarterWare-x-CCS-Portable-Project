/*
 * main.c — DC/RST toggle + SPI0 TX of 0xAF (sanity check on SPI bus).
 *
 *  DC  = P8_26 = GPIO1[29] = gpmc_csn0
 *  RST = P8_19 = GPIO0[22] = gpmc_ad8
 *
 *  SPI0 (channel 0):
 *      CLK  = P9_22 (spi0_sclk)
 *      D1   = P9_18 (spi0_d1)       -- MOSI (TX-only, see note in MCSPI_DATA_LINE_COMM_MODE_1)
 *      CS   = P9_17 (spi0_cs0)      -- hardware CS, active low
 *
 *  Each iteration:
 *      pulse RST low (LCD reset command pattern -- meaningless for 0xAF
 *      but proves the line moves),
 *      drive DC LOW (0xAF is a command, not data),
 *      toggle CS active via McSPICSAssert, transmit one byte through
 *      channel 0 by polling channel-status flag, then deassert CS.
 *
 *  Loop runs forever so each signal edge is visible on a scope.
 */

#include "soc_AM335x.h"
#include "beaglebone.h"
#include "gpio_v2.h"
#include "hw_control_AM335x.h"
#include "hw_cm_per.h"
#include "hw_types.h"
#include "mcspi.h"

/* --- GPIO: DC / RST ----------------------------------------------------- */
#define DC_GPIO_BASE    (SOC_GPIO_1_REGS)
#define DC_GPIO_PIN     (29)
#define RST_GPIO_BASE   (SOC_GPIO_0_REGS)
#define RST_GPIO_PIN    (22)

/* --- SPI0 --------------------------------------------------------------- */
#define SPI_BASE        (SOC_SPI_0_REGS)
#define SPI_CH          (0)
#define MCSPI_IN_CLK    (48000000u)
#define MCSPI_OUT_FREQ  (100000u)         /* ~100 kHz (easy on a scope) */

static void Delay(volatile unsigned int count);
static void Spi0Init(void);
static void Spi0TxByte(unsigned char byte);

int main(void)
{
    /* GPIO clocks (StarterWare helpers). */
    GPIO0ModuleClkConfig();
    GPIO1ModuleClkConfig();

    /* Pinmux DC and RST into their GPIO mode (mode 7). */
    GpioPinMuxSetup(CONTROL_CONF_GPMC_CSN(0), CONTROL_CONF_MUXMODE(7));
    GpioPinMuxSetup(CONTROL_CONF_GPMC_AD(8),  CONTROL_CONF_MUXMODE(7));

    GPIOModuleEnable(DC_GPIO_BASE);
    GPIOModuleReset(DC_GPIO_BASE);
    GPIODirModeSet(DC_GPIO_BASE, DC_GPIO_PIN, GPIO_DIR_OUTPUT);

    GPIOModuleEnable(RST_GPIO_BASE);
    GPIOModuleReset(RST_GPIO_BASE);
    GPIODirModeSet(RST_GPIO_BASE, RST_GPIO_PIN, GPIO_DIR_OUTPUT);

    /* Initial state: both idle LOW. */
    GPIOPinWrite(DC_GPIO_BASE,  DC_GPIO_PIN,  GPIO_PIN_LOW);
    GPIOPinWrite(RST_GPIO_BASE, RST_GPIO_PIN, GPIO_PIN_LOW);

    /* Bring up SPI0. */
    Spi0Init();

    Delay(0x3FFFF);

    while (1)
    {
        /* Toggle DC + RST as a heartbeat on the scope. */
        GPIOPinWrite(DC_GPIO_BASE,  DC_GPIO_PIN,  GPIO_PIN_HIGH);
        GPIOPinWrite(RST_GPIO_BASE, RST_GPIO_PIN, GPIO_PIN_LOW);
        Delay(0x3FFFF);

        /* DC back LOW -> next transfer is a command (ST7735 logic). */
        GPIOPinWrite(DC_GPIO_BASE, DC_GPIO_PIN, GPIO_PIN_LOW);

        /* Send 0xAF. */
        Spi0TxByte(0xAF);

        /* RST pulse HIGH then LOW. */
        GPIOPinWrite(RST_GPIO_BASE, RST_GPIO_PIN, GPIO_PIN_HIGH);
        Delay(0x3FFFF);
        GPIOPinWrite(RST_GPIO_BASE, RST_GPIO_PIN, GPIO_PIN_LOW);
        Delay(0x3FFFF);
    }
}

/* --------------------------------------------------------------------- */
/* SPI helpers                                                            */
/* --------------------------------------------------------------------- */

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

    /* Configure channel 0: single channel, transmit-and-receive on D0/D1. */
    McSPIMasterModeConfig(SPI_BASE,
                          MCSPI_SINGLE_CH,           /* channelMode */
                          MCSPI_TX_RX_MODE,          /* trMode      */
                          MCSPI_DATA_LINE_COMM_MODE_1, /* pinMode   */
                          SPI_CH);

    /* 8-bit word length. */
    McSPIWordLengthSet(SPI_BASE, MCSPI_WORD_LENGTH(8), SPI_CH);

    /* SPI clock: ~1 MHz out. */
    McSPIClkConfig(SPI_BASE,
                   MCSPI_IN_CLK,
                   MCSPI_OUT_FREQ,
                   SPI_CH,
                   MCSPI_CLK_MODE_0);

    /* Active-low chip select. */
    McSPICSPolarityConfig(SPI_BASE, MCSPI_CS_POL_LOW, SPI_CH);

    /* Enable FIFOs. */
    McSPITxFIFOConfig(SPI_BASE, MCSPI_TX_FIFO_ENABLE, SPI_CH);
    McSPIRxFIFOConfig(SPI_BASE, MCSPI_RX_FIFO_ENABLE, SPI_CH);

    /* Enable channel 0. */
    McSPIChannelEnable(SPI_BASE, SPI_CH);
}

static void Spi0TxByte(unsigned char byte)
{
    /* Manual CS control (per byte). */
    McSPICSAssert(SPI_BASE, SPI_CH);
    McSPITransmitData(SPI_BASE, byte, SPI_CH);

    /* Wait for the shift register to drain.  MCSPI_CH0STAT_TXS is the
     * "Tx Register Empty" flag and stays cleared while the controller
     * is clocking bits out.  Only when it goes high may we safely pull
     * CS high again.
     */
    while (!((McSPIChannelStatusGet(SPI_BASE, SPI_CH)) &
             MCSPI_CH0STAT_TXS)) { /* spin */ }

    /* Drain any received byte so RX FIFO never overruns. */
    (void)McSPIReceiveData(SPI_BASE, SPI_CH);

    McSPICSDeAssert(SPI_BASE, SPI_CH);
}

/* --------------------------------------------------------------------- */

static void Delay(volatile unsigned int count)
{
    while (count--) ;
}
