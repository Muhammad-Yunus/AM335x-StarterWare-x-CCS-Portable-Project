/**
 * \file   main.c
 *
 * \brief  AM3352 (Antminer L3+ / BBB-class) demo for SSD1306 OLED 128x32.
 *
 *         Mirrors the AM3352_I2C_SCANNER baseline for UART0 (115200 8N1)
 *         and I2C1 bring-up, then drives an SSD1306 panel addressed at
 *         0x3C on the same I2C1 bus (P9_17 / P9_18).
 *
 *         Demo sequence:
 *           - splash: "AM3352" + "SSD1306 128x32"
 *           - shape tour: rect, filled rect, circle, filled circle, line, triangle
 *           - animated progress bar 0% -> 100%
 *           - counter 0..99 in 7x10 font
 *
 *         The SSD1306 driver in ssd1306/ was ported from the STM32
 *         reference — only the bottom-half I2C transport was swapped to
 *         use the StarterWare I2C1 master here.
 */

#include "consoleUtils.h"
#include "uart_irda_cir.h"
#include "hw_types.h"
#include "hw_control_AM335x.h"
#include "hw_cm_per.h"
#include "soc_AM335x.h"
#include "interrupt.h"
#include "beaglebone.h"
#include "hsi2c.h"
#include "ssd1306.h"
#include "ssd1306_defines.h"
#include "fonts.h"

#include <stdint.h>
#include <stdio.h>

/******************************************************************************
**              INTERNAL MACRO DEFINITIONS
******************************************************************************/
#define BAUD_RATE_115200          (115200)
#define UART_MODULE_INPUT_CLK     (48000000)
#define DEMO_DELAY_LOOPS          (8000000u)

/******************************************************************************
**              INTERNAL FUNCTION PROTOTYPES
******************************************************************************/
static void UartFIFOConfigure(void);
static void UartBaudRateSet(void);
static void UartInterruptEnable(void);
static void UART0AINTCConfigure(void);
static void UARTIsr(void);
static void SetupI2C1Master(void);
static void Delay(volatile uint32_t loops);
static void DrawSplash(void);
static void DrawShapeTour(void);
static void DrawProgress(void);
static void DrawCounter(void);

/* Low-level I2C transport backing the SSD1306 driver. */
void ssd1306_I2C_WriteCommand(uint8_t command);
void ssd1306_I2C_WriteData(uint8_t *data, uint16_t size);

/******************************************************************************
**              FUNCTION DEFINITIONS
******************************************************************************/

static void UartFIFOConfigure(void)
{
    unsigned int fifoConfig = 0;
    fifoConfig = UART_FIFO_CONFIG(UART_TRIG_LVL_GRANULARITY_4,
                                  UART_TRIG_LVL_GRANULARITY_1,
                                  UART_FCR_TX_TRIG_LVL_56,
                                  1, 1, 1,
                                  UART_DMA_EN_PATH_SCR,
                                  UART_DMA_MODE_0_ENABLE);
    UARTFIFOConfig(SOC_UART_0_REGS, fifoConfig);
}

static void UartBaudRateSet(void)
{
    unsigned int divisorValue = 0;
    divisorValue = UARTDivisorValCompute(UART_MODULE_INPUT_CLK,
                                         BAUD_RATE_115200,
                                         UART16x_OPER_MODE,
                                         UART_MIR_OVERSAMPLING_RATE_42);
    UARTDivisorLatchWrite(SOC_UART_0_REGS, divisorValue);
}

static void UartInterruptEnable(void)
{
    IntMasterIRQEnable();
    UART0AINTCConfigure();
    UARTIntEnable(SOC_UART_0_REGS, UART_INT_LINE_STAT);
}

static void UART0AINTCConfigure(void)
{
    IntAINTCInit();
    IntRegister(SYS_INT_UART0INT, UARTIsr);
    IntPrioritySet(SYS_INT_UART0INT, 0, AINTC_HOSTINT_ROUTE_IRQ);
    IntSystemEnable(SYS_INT_UART0INT);
}

static void UARTIsr(void)
{
    unsigned int intId = UARTIntIdentityGet(SOC_UART_0_REGS);
    switch (intId) {
        case UART_INTID_RX_THRES_REACH:
        case UART_INTID_RX_LINE_STAT_ERROR:
        case UART_INTID_CHAR_TIMEOUT:
            while (TRUE == UARTCharsAvail(SOC_UART_0_REGS)) {
                (void)UARTCharGetNonBlocking(SOC_UART_0_REGS);
            }
            if (intId == UART_INTID_RX_LINE_STAT_ERROR) {
                (void)UARTRxErrorGet(SOC_UART_0_REGS);
            }
            break;
        case UART_INTID_TX_THRES_REACH:
        default:
            break;
    }
}

static void SetupI2C1Master(void)
{
    I2C1ModuleClkConfig();
    I2CPinMuxSetup(1);  /* SPI0_D1/SPI0_CS0 -> P9_17/P9_18 */

    I2CMasterDisable(SOC_I2C_1_REGS);
    I2CAutoIdleDisable(SOC_I2C_1_REGS);
    I2CMasterInitExpClk(SOC_I2C_1_REGS, 48000000, 12000000, 100000);
    I2CMasterEnable(SOC_I2C_1_REGS);
}

static void Delay(volatile uint32_t loops)
{
    while (loops--) {
        __asm("    nop");
    }
}

static void DrawSplash(void)
{
    ssd1306_Clear();
    ssd1306_SetCursor(2, 0);
    ssd1306_SetColor(White);
    ssd1306_WriteString("AM3352 + SSD1306", Font_7x10);
    ssd1306_SetCursor(2, 16);
    ssd1306_WriteString("I2C1 @ 0x3C  128x32", Font_7x10);
    ssd1306_UpdateScreen();
}

static void DrawShapeTour(void)
{
    ssd1306_Clear();
    ssd1306_SetColor(White);
    ssd1306_DrawRect(0, 0, 128, 32);
    ssd1306_DrawRect(4, 4, 56, 24);
    ssd1306_DrawCircle(110, 16, 12);
    ssd1306_DrawLine(64, 4, 96, 28);
    ssd1306_DrawLine(96, 4, 64, 28);
    ssd1306_UpdateScreen();
    Delay(DEMO_DELAY_LOOPS);

    ssd1306_Clear();
    ssd1306_SetColor(White);
    ssd1306_FillRect(0, 0, 60, 32);
    ssd1306_FillCircle(110, 16, 14);
    ssd1306_SetColor(Black);
    ssd1306_FillRect(2, 14, 56, 4);
    ssd1306_SetColor(White);
    ssd1306_DrawBitmap(64, 8, 16, 8, (const uint8_t[]){
        0xFF,0x81,0x81,0x81,0x81,0x81,0x81,0xFF,
        0xFF,0x81,0x81,0x81,0x81,0x81,0x81,0xFF
    });
    ssd1306_UpdateScreen();
    Delay(DEMO_DELAY_LOOPS);
}

static void DrawProgress(void)
{
    uint8_t pct;
    char buf[8];
    for (pct = 0; pct <= 100; pct += 5) {
        ssd1306_Clear();
        ssd1306_SetCursor(0, 0);
        ssd1306_SetColor(White);
        ssd1306_WriteString("Loading...", Font_7x10);
        ssd1306_DrawProgressBar(0, 18, 128, 10, pct);
        ssd1306_SetCursor(96, 22);
        snprintf(buf, sizeof(buf), "%u%%", (unsigned)pct);
        ssd1306_WriteString(buf, Font_7x10);
        ssd1306_UpdateScreen();
        Delay(DEMO_DELAY_LOOPS / 8);
    }
    Delay(DEMO_DELAY_LOOPS);
}

static void DrawCounter(void)
{
    int n;
    char buf[16];
    for (n = 0; n < 100; n++) {
        ssd1306_Clear();
        ssd1306_SetCursor(0, 0);
        ssd1306_SetColor(White);
        ssd1306_WriteString("AM3352", Font_7x10);
        ssd1306_SetCursor(0, 16);
        snprintf(buf, sizeof(buf), "n=%d", n);
        ssd1306_WriteString(buf, Font_11x18);
        ssd1306_UpdateScreen();
        Delay(DEMO_DELAY_LOOPS / 4);
    }
}

int main(void)
{
    UART0ModuleClkConfig();
    UARTPinMuxSetup(0);
    UARTModuleReset(SOC_UART_0_REGS);

    /* UART0 bring-up */
    UartFIFOConfigure();
    UartBaudRateSet();
    UARTRegConfigModeEnable(SOC_UART_0_REGS, UART_REG_CONFIG_MODE_B);
    UARTLineCharacConfig(SOC_UART_0_REGS,
                         (UART_FRAME_WORD_LENGTH_8 | UART_FRAME_NUM_STB_1),
                         UART_PARITY_NONE);
    UARTDivisorLatchDisable(SOC_UART_0_REGS);
    UARTBreakCtl(SOC_UART_0_REGS, UART_BREAK_COND_DISABLE);
    UARTOperatingModeSelect(SOC_UART_0_REGS, UART16x_OPER_MODE);

    ConsoleUtilsInit();
    ConsoleUtilsSetType(CONSOLE_UART);
    UartInterruptEnable();

    ConsoleUtilsPrintf("\r\n");
    ConsoleUtilsPrintf("+---------------------------------------------+\r\n");
    ConsoleUtilsPrintf("|  AM3352  SSD1306  128x32  OLED  Demo        |\r\n");
    ConsoleUtilsPrintf("|  I2C1  SCL=P9_17  SDA=P9_18  addr=0x3C      |\r\n");
    ConsoleUtilsPrintf("+---------------------------------------------+\r\n");

    /* I2C1 bring-up */
    SetupI2C1Master();

    ConsoleUtilsPrintf("\r\nI2C1 ready, init SSD1306...\r\n");

    if (ssd1306_Init() == 0) {
        ConsoleUtilsPrintf("ssd1306_Init FAILED — panel not on 0x3C.\r\n");
        while (1) { /* halt */ }
    }
    ConsoleUtilsPrintf("ssd1306_Init OK — splash on.\r\n");

    while (1) {
        DrawSplash();
        Delay(DEMO_DELAY_LOOPS);

        DrawShapeTour();

        DrawProgress();

        DrawCounter();
    }
}

#include <stdio.h>

/******************************* End of file *********************************/
