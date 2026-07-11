/**
 * \file   main.c
 *
 * \brief  AM3352 (Antminer L3+ / BBB-class) I2C1 bus scanner.
 *
 *         Scans the I2C1 bus on P9_17 (SCL) / P9_18 (SDA) for devices
 *         in the 7-bit address range 0x03..0x77 and prints each hit
 *         over UART0 at 115200 8N1.
 *
 *         UART bring-up mirrors uartEcho.c setup; I2C1 probing uses
 *         polling only (no ISR-driven I2C).
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

/******************************************************************************
**              INTERNAL MACRO DEFINITIONS
******************************************************************************/
#define BAUD_RATE_115200          (115200)
#define UART_MODULE_INPUT_CLK     (48000000)


/******************************************************************************
**              INTERNAL FUNCTION PROTOTYPES
******************************************************************************/
static void UartFIFOConfigure(void);
static void UartBaudRateSet(void);
static void UartInterruptEnable(void);
static void UART0AINTCConfigure(void);
static void UARTIsr(void);
static void SetupI2C1Master(void);
static int  I2C1ProbeAddress(unsigned char addr);

/******************************************************************************
**              GLOBAL VARIABLE DEFINITIONS
******************************************************************************/
unsigned int foundCount = 0;

/* Bitmap of the 128 7-bit addresses 0x00..0x7F; bit N set = device found. */
static unsigned char foundMap[16];

/******************************************************************************
**              FUNCTION DEFINITIONS
******************************************************************************/

/* A wrapper function performing FIFO configurations. */
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

/* A wrapper function performing Baud Rate settings. */
static void UartBaudRateSet(void)
{
    unsigned int divisorValue = 0;

    divisorValue = UARTDivisorValCompute(UART_MODULE_INPUT_CLK,
                                         BAUD_RATE_115200,
                                         UART16x_OPER_MODE,
                                         UART_MIR_OVERSAMPLING_RATE_42);

    UARTDivisorLatchWrite(SOC_UART_0_REGS, divisorValue);
}

/* A wrapper function performing Interrupt configurations. */
static void UartInterruptEnable(void)
{
    IntMasterIRQEnable();

    UART0AINTCConfigure();

    /* RX and error handling only — TX is polled via ConsoleUtilsPrintf. */
    UARTIntEnable(SOC_UART_0_REGS, UART_INT_LINE_STAT);
}

/* AINTC configuration for UART0 (THR-driven TX; RX is polled from main()). */
static void UART0AINTCConfigure(void)
{
    IntAINTCInit();

    IntRegister(SYS_INT_UART0INT, UARTIsr);

    IntPrioritySet(SYS_INT_UART0INT, 0, AINTC_HOSTINT_ROUTE_IRQ);

    IntSystemEnable(SYS_INT_UART0INT);
}

/* UART ISR — drain RX FIFO on every interrupt (mirrors uartEcho).
** All TX uses polled ConsoleUtilsPrintf, so no THR-driven path here. */
static void UARTIsr(void)
{
    unsigned int intId = 0;

    intId = UARTIntIdentityGet(SOC_UART_0_REGS);

    switch(intId)
    {
        case UART_INTID_RX_THRES_REACH:
            while(TRUE == UARTCharsAvail(SOC_UART_0_REGS))
            {
                (void)UARTCharGetNonBlocking(SOC_UART_0_REGS);
            }
        break;

        case UART_INTID_RX_LINE_STAT_ERROR:
            (void)UARTRxErrorGet(SOC_UART_0_REGS);
            while(TRUE == UARTCharsAvail(SOC_UART_0_REGS))
            {
                (void)UARTCharGetNonBlocking(SOC_UART_0_REGS);
            }
        break;

        case UART_INTID_CHAR_TIMEOUT:
            while(TRUE == UARTCharsAvail(SOC_UART_0_REGS))
            {
                (void)UARTCharGetNonBlocking(SOC_UART_0_REGS);
            }
        break;

        case UART_INTID_TX_THRES_REACH:
        default:
            /* TX path is polled via ConsoleUtilsPrintf; nothing to do. */
        break;
    }
}

/* Configure I2C1 in master mode at 100 kHz. */
static void SetupI2C1Master(void)
{
    I2C1ModuleClkConfig();

    /* Routed on beaglebone.h pads via MUXMODE(2): SPI0_D1 -> P9_17 (SCL),
    ** SPI0_CS0 -> P9_18 (SDA). */
    I2CPinMuxSetup(1);

    I2CMasterDisable(SOC_I2C_1_REGS);

    I2CAutoIdleDisable(SOC_I2C_1_REGS);

    /* system clock 48 MHz, I2C functional clock 12 MHz, target 100 kHz. */
    I2CMasterInitExpClk(SOC_I2C_1_REGS, 48000000, 12000000, 100000);

    /* Slave address is set per-probe just before each transaction. */

    I2CMasterEnable(SOC_I2C_1_REGS);
}

/*
** Probe a single 7-bit I2C address. Returns 1 if a slave ACKed (device
** present), 0 if it NACKed or timed out. Polling only — no I2C ISR.
**
** Sequence (bootloader pattern from
** C:\ti\AM335X_StarterWare_02_00_01_01\bootloader\src\armv7a\am335x\bl_platform.c:SetupI2CTransmit,
** with two recovery hooks added for stuck-bus conditions seen when a
** SSD1306 sits next to a probed-but-not-present address):
**   1. if controller still claims bus-busy, send STOP + bounded wait
**   2. clear all status, set slave addr, dcount=1, MST_TX, preload DATA
**   3. START; wait bus-busy (bounded)
**   4. wait ARDY (address byte + ACK/NACK resolved)
**   5. if ARDY && !NACK && !AL -> present=1
**   6. STOP; wait STOP_CONDITION (bounded) — if it times out, soft-reset
**      the controller and try again once before giving up
**   7. drain status
*/
static int I2C1ProbeAddress(unsigned char addr)
{
    volatile unsigned int status = 0;
    unsigned int waited = 0;
    const unsigned int TIMEOUT = 200000u;
    int present = 0;

    /* ---- step 1: drain stale state, bounded ---- */
    if(I2CMasterBusBusy(SOC_I2C_1_REGS))
    {
        I2CMasterStop(SOC_I2C_1_REGS);
        waited = 0;
        while(I2CMasterBusBusy(SOC_I2C_1_REGS) && (waited++ < TIMEOUT));

        /* If still busy, a slave is holding SDA low. Force a controller
        ** reset to recover; this is the only thing that frees the bus
        ** when the slave never releases it. */
        if(I2CMasterBusBusy(SOC_I2C_1_REGS))
        {
            I2CMasterDisable(SOC_I2C_1_REGS);
            I2CSoftReset(SOC_I2C_1_REGS);
            I2CAutoIdleDisable(SOC_I2C_1_REGS);
            I2CMasterInitExpClk(SOC_I2C_1_REGS, 48000000, 12000000, 100000);
            I2CMasterEnable(SOC_I2C_1_REGS);
        }
    }
    I2CMasterIntClearEx(SOC_I2C_1_REGS, 0x7FF);

    /* ---- step 2-3: configure & START ---- */
    I2CMasterSlaveAddrSet(SOC_I2C_1_REGS, addr);
    I2CSetDataCount(SOC_I2C_1_REGS, 1);
    I2CMasterControl(SOC_I2C_1_REGS, I2C_CFG_MST_TX);

    /* Preload the single data byte so the controller has something to
    ** shift after the auto-generated address byte. Without this the
    ** controller stalls mid-transaction and STOP never completes. */
    I2CMasterDataPut(SOC_I2C_1_REGS, 0);

    I2CMasterStart(SOC_I2C_1_REGS);

    /* ---- wait for bus-busy (bounded) ---- */
    waited = 0;
    while((I2CMasterBusBusy(SOC_I2C_1_REGS) == 0) && (waited++ < TIMEOUT));

    /* ---- step 4: wait ARDY ---- */
    waited = 0;
    do {
        status = I2CMasterIntRawStatus(SOC_I2C_1_REGS);
        waited++;
    } while(((status & I2C_INT_ADRR_READY_ACESS) == 0) && (waited < TIMEOUT));

    /* ---- step 5: decide ACK ---- */
    if((status & I2C_INT_ADRR_READY_ACESS) &&
       ((status & I2C_INT_NO_ACK) == 0) &&
       ((status & I2C_INT_ARBITRATION_LOST) == 0))
    {
        present = 1;
    }

    /* ---- step 6: STOP + bounded wait for STOP_CONDITION ---- */
    I2CMasterStop(SOC_I2C_1_REGS);

    waited = 0;
    do {
        status = I2CMasterIntRawStatus(SOC_I2C_1_REGS);
        waited++;
    } while(((status & I2C_INT_STOP_CONDITION) == 0) && (waited < TIMEOUT));

    /* If STOP_CONDITION never came, slave is still holding the bus.
    ** Soft-reset and report this probe as 'no device' so the scan can
    ** continue to the next address. */
    if((status & I2C_INT_STOP_CONDITION) == 0)
    {
        I2CMasterDisable(SOC_I2C_1_REGS);
        I2CSoftReset(SOC_I2C_1_REGS);
        I2CAutoIdleDisable(SOC_I2C_1_REGS);
        I2CMasterInitExpClk(SOC_I2C_1_REGS, 48000000, 12000000, 100000);
        I2CMasterEnable(SOC_I2C_1_REGS);
        present = 0;
    }

    /* ---- step 7: drain status ---- */
    I2CMasterIntClearEx(SOC_I2C_1_REGS, 0x7FF);

    return present;
}

int main(void)
{
    unsigned char addr = 0;

    /* ---------------- UART0 bring-up ---------------- */
    UART0ModuleClkConfig();
    UARTPinMuxSetup(0);
    UARTModuleReset(SOC_UART_0_REGS);
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
    ConsoleUtilsPrintf("|   AM3352  I2C1  Bus  Scanner                |\r\n");
    ConsoleUtilsPrintf("|   SCL = P9_17   SDA = P9_18   100 kHz       |\r\n");
    ConsoleUtilsPrintf("+---------------------------------------------+\r\n");

    /* ---------------- I2C1 bring-up ---------------- */
    SetupI2C1Master();

    ConsoleUtilsPrintf("\r\n");
    for(addr = 0x00; addr < 0x80; addr++)
    {
        /* Skip the 7-bit reserved addresses that cannot legally appear on
        ** the bus: 0x00 (general call) and 0x78..0x7F (10-bit addrs). */
        if(addr < 0x03 || addr > 0x77)
        {
            continue;
        }
        if(I2C1ProbeAddress(addr))
        {
            foundMap[addr >> 3] |= (unsigned char)(1u << (addr & 0x7));
            foundCount++;
        }
    }

    /* Print an 8-row grid (0x00..0x70) in i2cdetect -y 1 format.
    ** `--` = no ACK, `XX` = slave present. The last row (0x70..) stops
    ** at 0x77 since 7-bit addresses live in 0x00..0x77. */
    ConsoleUtilsPrintf("       0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\r\n");
    {
        unsigned int row;
        for(row = 0; row < 8; row++)
        {
            unsigned int col;
            ConsoleUtilsPrintf("%02x: ", row << 4);
            for(col = 0; col < 16; col++)
            {
                unsigned char a = (unsigned char)((row << 4) | col);
                if(a > 0x77)
                {
                    /* No cells past 0x77 in 7-bit I2C; row 0x70 stops
                    ** at col 0x7. Skip to next row. */
                    break;
                }
                if(a < 0x03)
                {
                    ConsoleUtilsPrintf("   ");
                }
                else if(foundMap[a >> 3] & (unsigned char)(1u << (a & 0x7)))
                {
                    ConsoleUtilsPrintf(" %02x", a);
                }
                else
                {
                    ConsoleUtilsPrintf(" --");
                }
                if(col < 15) ConsoleUtilsPrintf(" ");
            }
            ConsoleUtilsPrintf("\r\n");
        }
    }

    ConsoleUtilsPrintf("\r\n%u device(s) detected on I2C1.\r\n", foundCount);

    while(1)
    {
        /* Idle — keep JTAG connection alive, leave UART RX interrupt
        ** wired so any host input is silently drained. */
    }
}

/******************************* End of file *********************************/
