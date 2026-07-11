/**
 * \file   main.c
 *
 * \brief  AM3352 GPIO input interrupt demo on P9_12 (GPIO1[28], global GPIO60).
 *
 *         Hardware target:
 *           SoC      : TI AM3352 (Cortex-A8, little-endian, ARMv7-A)
 *           Board    : Antminer L3+ (PCB/form-factor identical to BeagleBone)
 *           Input pin: P9_12 -> gpmc_ben1 -> GPIO1[28] (gpio number 60)
 *           Output   : UART0 console at 115200 8N1
 *
 *         Behaviour:
 *           1. UART0 prints a banner and waits for typed characters.
 *              Anything typed is echoed back, line-buffered, with idle flush.
 *           2. P9_12 is configured as a GPIO input with a falling-edge
 *              interrupt. Every press sets a flag; the main loop polls the
 *              flag and prints "GPIO60 interrupt N" on UART0.
 *
 *         Notes on P9_12 for AM3352:
 *           - Pad control register offset: 0x0878 (gpio_pin_mux.h GPIO_1_28).
 *           - Functional mux mode for GPIO: 7.
 *           - Bank 1, pin 28 -> GPIO input number 60 (1 * 32 + 28).
 *           - GPIO bank 1 uses SYS_INT_GPIOINT1A (98) for interrupt line 1.
 */

#include "hw_control_AM335x.h"
#include "soc_AM335x.h"
#include "beaglebone.h"
#include "interrupt.h"
#include "hw_types.h"
#include "pin_mux.h"
#include "gpio_v2.h"
#include "consoleUtils.h"
#include "uart_irda_cir.h"

/******************************************************************************
**              INTERNAL MACRO DEFINITIONS
******************************************************************************/
#define BAUD_RATE_115200          (115200)
#define UART_MODULE_INPUT_CLK     (48000000)
#define NUM_TX_BYTES_PER_TRANS    (56)

/* P9_12 = GPIO1[28] = global GPIO 60, on GPIO bank 1 interrupt line 1.       */
#define GPIO_INPUT_INSTANCE       (SOC_GPIO_1_REGS)
#define GPIO_INPUT_PIN            (28u)
#define GPIO_INPUT_INTR_LINE      (GPIO_INT_LINE_1)
#define GPIO_INPUT_SYS_INT_NUM    (SYS_INT_GPIOINT1A)
#define GPIO_PAD_OFFSET           (GPIO_1_28)

/******************************************************************************
**              INTERNAL FUNCTION PROTOTYPES
******************************************************************************/
static void UartInterruptEnable(void);
static void UART0AINTCConfigure(void);
static void UartFIFOConfigure(void);
static void UartBaudRateSet(void);
static void UARTIsr(void);
static void GPIOIsr(void);
static void GPIOInputInit(void);
static void GPIOINTCConfigure(void);
static void UartPutString(const char *s);
static void DispatchLine(void);

/******************************************************************************
**              GLOBAL VARIABLE DEFINITIONS
******************************************************************************/
unsigned char txArray[] =
    "\r\n=== AM3352 GPIO INTERRUPT (P9_12 = GPIO60) ===\r\n"
    "Type something + Enter to echo. Press button on P9_12 to trigger ISR.\r\n";

unsigned int txEmptyFlag    = TRUE;
unsigned int currNumTxBytes = 0;

#define CMD_BUF_SIZE  64
static char     cmdBuf[CMD_BUF_SIZE];
static unsigned int cmdLen           = 0;
static unsigned int lastCmdLenSnap   = 0;
#define IDLE_LIMIT_HITS  (100000u)
static unsigned int idleHits         = 0;

/* Set by GPIOIsr(); consumed by main loop. */
static volatile unsigned int gGpioIsrFlag       = 0;
static volatile unsigned int gGpioIsrEdgeCount  = 0;

/* Debug heartbeat: poll the pin level periodically so we can confirm the
** input buffer is actually enabled and toggling. */
#define HEARTBEAT_PERIOD  (2000000u)
static unsigned int gHeartbeatHits = 0;

static void UartPutString(const char *s)
{
    while(*s != '\0')
    {
        UARTCharPut(SOC_UART_0_REGS, (unsigned char)*s);
        s++;
    }
}

static void DispatchLine(void)
{
    if(cmdLen >= CMD_BUF_SIZE)
    {
        cmdLen = CMD_BUF_SIZE - 1;
    }
    cmdBuf[cmdLen] = '\0';

    UartPutString("\r\ngpio> ");
    UartPutString(cmdBuf);
    UartPutString("\r\n\r\n");

    cmdLen = 0;
}

/******************************************************************************
**              FUNCTION DEFINITIONS
******************************************************************************/

int main(void)
{
    unsigned int numByteChunks = 0;
    unsigned int remainBytes   = 0;
    unsigned int bIndex        = 0;

    /* --- UART0 bring-up (echo baseline) ----------------------------------- */
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

    ConsoleUtilsSetType(CONSOLE_UART);
    UartInterruptEnable();

    /* Project banner — printed once via blocking UartPutString from main.    */
    UartPutString("\r\n");
    UartPutString("============================================================\r\n");
    UartPutString("  AM3352 GPIO INTERRUPT DEMO                              \r\n");
    UartPutString("  Target  : Antminer L3+ (BeagleBone form-factor)          \r\n");
    UartPutString("  Pin     : P9_12 = GPIO1[28] = global GPIO 60            \r\n");
    UartPutString("  Trigger : Rising edge on GPIO_INT_LINE_1                \r\n");
    UartPutString("  Console : UART0 @ 115200 8N1                            \r\n");
    UartPutString("============================================================\r\n");
    UartPutString("\r\n");
    UartPutString("Type characters to echo. Press button/signal on P9_12 to\r\n");
    UartPutString("fire ISR — main loop will print: [ISR] GPIO60 edge #N.\r\n");
    UartPutString("\r\n");

    /* --- GPIO60 (P9_12) bring-up with falling-edge interrupt -------------- */
    GPIOInputInit();
    GPIOINTCConfigure();

    numByteChunks = (sizeof(txArray) - 1) / NUM_TX_BYTES_PER_TRANS;
    remainBytes   = (sizeof(txArray) - 1) % NUM_TX_BYTES_PER_TRANS;

    while(1)
    {
        /* UART RX echo path. */
        while(TRUE == UARTCharsAvail(SOC_UART_0_REGS))
        {
            unsigned char rxByte = UARTCharGetNonBlocking(SOC_UART_0_REGS);
            UARTCharPutNonBlocking(SOC_UART_0_REGS, rxByte);
            idleHits = 0;

            if(rxByte == '\r')
            {
                /* CR ignored; wait for LF. */
            }
            else if(rxByte == '\n')
            {
                DispatchLine();
            }
            else if(cmdLen < (CMD_BUF_SIZE - 1))
            {
                cmdBuf[cmdLen++] = (char)rxByte;
            }
        }

        /* Idle-timeout flush for hosts that send without a trailing newline. */
        if((cmdLen > 0) && (cmdLen == lastCmdLenSnap))
        {
            if(++idleHits >= IDLE_LIMIT_HITS)
            {
                DispatchLine();
                idleHits = 0;
            }
        }
        lastCmdLenSnap = cmdLen;

        /* Heartbeat: print the current pin level every HEARTBEAT_PERIOD spins.
        ** Confirms input buffer (RXACTIVE) is enabled and reads the line.
        ** If the line reads back the right value but ISR still doesn't fire,
        ** the trigger type or INTC wiring is the next suspect. */
        if(++gHeartbeatHits >= HEARTBEAT_PERIOD)
        {
            unsigned int level = GPIOPinRead(GPIO_INPUT_INSTANCE,
                                             GPIO_INPUT_PIN);
            gHeartbeatHits = 0;
            UartPutString("[hb] P9_12 level = ");
            UartPutString(level ? "1\r\n" : "0\r\n");
        }

        /* ISR -> main loop signal: report every edge in banner-friendly form. */
        if(gGpioIsrFlag != 0)
        {
            gGpioIsrFlag = 0;

            /* Tiny printf replacement via the existing console helper. */
            UartPutString("\r\n[ISR] GPIO60 (P9_12) edge #");
            {
                /* Decimal print of gGpioIsrEdgeCount. Sufficient for demo. */
                unsigned int n = gGpioIsrEdgeCount;
                char buf[12];
                char *p = &buf[11];
                *p = '\0';
                if(n == 0) { *(--p) = '0'; }
                else
                {
                    while(n != 0 && p > buf)
                    {
                        *(--p) = (char)('0' + (n % 10u));
                        n /= 10u;
                    }
                }
                UartPutString(p);
            }
            UartPutString("\r\n");
        }

        /* Original TX kickoff (banner chunks). */
        if(TRUE == txEmptyFlag)
        {
            if(bIndex < numByteChunks)
            {
                currNumTxBytes += UARTFIFOWrite(SOC_UART_0_REGS,
                                                &txArray[currNumTxBytes],
                                                NUM_TX_BYTES_PER_TRANS);
                bIndex++;
            }
            else
            {
                currNumTxBytes += UARTFIFOWrite(SOC_UART_0_REGS,
                                                &txArray[currNumTxBytes],
                                                remainBytes);
            }
            txEmptyFlag = FALSE;
            UARTIntEnable(SOC_UART_0_REGS, UART_INT_THR);
        }
    }
}

/* -------------------------------------------------------------------------- */
/* UART plumbing (same model as baseline uartEcho).                           */
/* -------------------------------------------------------------------------- */

static void UartFIFOConfigure(void)
{
    unsigned int fifoConfig =
        UART_FIFO_CONFIG(UART_TRIG_LVL_GRANULARITY_4,
                         UART_TRIG_LVL_GRANULARITY_1,
                         UART_FCR_TX_TRIG_LVL_56,
                         1, 1, 1,
                         UART_DMA_EN_PATH_SCR,
                         UART_DMA_MODE_0_ENABLE);
    UARTFIFOConfig(SOC_UART_0_REGS, fifoConfig);
}

static void UartBaudRateSet(void)
{
    unsigned int divisorValue =
        UARTDivisorValCompute(UART_MODULE_INPUT_CLK,
                              BAUD_RATE_115200,
                              UART16x_OPER_MODE,
                              UART_MIR_OVERSAMPLING_RATE_42);
    UARTDivisorLatchWrite(SOC_UART_0_REGS, divisorValue);
}

static void UartInterruptEnable(void)
{
    IntMasterIRQEnable();
    UART0AINTCConfigure();
    UARTIntEnable(SOC_UART_0_REGS, UART_INT_LINE_STAT | UART_INT_THR);
}

static void UARTIsr(void)
{
    unsigned int intId = UARTIntIdentityGet(SOC_UART_0_REGS);

    switch(intId)
    {
        case UART_INTID_TX_THRES_REACH:
            if(currNumTxBytes < (sizeof(txArray) - 1))
            {
                txEmptyFlag = TRUE;
            }
            UARTIntDisable(SOC_UART_0_REGS, UART_INT_THR);
            break;

        case UART_INTID_RX_THRES_REACH:
        case UART_INTID_CHAR_TIMEOUT:
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

        default:
            break;
    }
}

static void UART0AINTCConfigure(void)
{
    IntAINTCInit();
    IntRegister(SYS_INT_UART0INT, UARTIsr);
    IntPrioritySet(SYS_INT_UART0INT, 0, AINTC_HOSTINT_ROUTE_IRQ);
    IntSystemEnable(SYS_INT_UART0INT);
}

/* -------------------------------------------------------------------------- */
/* GPIO60 (P9_12) input interrupt.                                            */
/* -------------------------------------------------------------------------- */

/*
** Configure GPIO1[28] (P9_12, gpmc_ben1) as a debounced, rising-edge input.
** Mux mode 7 selects the GPIO function on the pad; RX enabled (mandatory for
** GPIO input — RXACTIVE=0 disables the input buffer and the interrupt never
** fires); internal pull-down so the line idles at 0V when the button is open.
*/
static void GPIOInputInit(void)
{
    /* Functional clock for GPIO1. */
    GPIO1ModuleClkConfig();

    /* Pin mux: gpmc_ben1 -> GPIO1[28], mode 7, RX enabled, pulldown enabled. */
    GpioPinMuxSetup(GPIO_PAD_OFFSET, PAD_FS_RXE_PD_PUPDE(7));

    /* Bring GPIO1 module out of reset. */
    GPIOModuleEnable(GPIO_INPUT_INSTANCE);
    GPIOModuleReset(GPIO_INPUT_INSTANCE);

    /* Direction = input. */
    GPIODirModeSet(GPIO_INPUT_INSTANCE, GPIO_INPUT_PIN, GPIO_DIR_INPUT);

    /* Debounce: enable per-pin, ~1.5 ms debounce time on all bank 1 inputs. */
    GPIODebounceFuncControl(GPIO_INPUT_INSTANCE,
                            GPIO_INPUT_PIN,
                            GPIO_DEBOUNCE_FUNC_ENABLE);
    GPIODebounceTimeConfig(GPIO_INPUT_INSTANCE, 48);
}

static void GPIOINTCConfigure(void)
{
    /* Interrupt controller already initialized by UART0 path; just register. */
    IntRegister(GPIO_INPUT_SYS_INT_NUM, GPIOIsr);
    IntPrioritySet(GPIO_INPUT_SYS_INT_NUM, 0, AINTC_HOSTINT_ROUTE_IRQ);
    IntSystemEnable(GPIO_INPUT_SYS_INT_NUM);

    /* Rising-edge trigger: fires when active-high button is pressed (low->high).
    ** Switch to GPIO_INT_TYPE_BOTH_EDGE if you also want ISR on release. */
    GPIOIntTypeSet(GPIO_INPUT_INSTANCE,
                   GPIO_INPUT_PIN,
                   GPIO_INT_TYPE_RISE_EDGE);
    GPIOPinIntEnable(GPIO_INPUT_INSTANCE,
                     GPIO_INPUT_INTR_LINE,
                     GPIO_INPUT_PIN);
}

/*
** GPIO interrupt handler. Must clear the latched status bit, otherwise the
** INTC will re-trigger indefinitely. Keep it short: no UART I/O here —
** the heavy lifting (UartPutString) runs in the main loop context where
** UARTCharPut is safe to block on.
*/
static void GPIOIsr(void)
{
    if(GPIOPinIntStatus(GPIO_INPUT_INSTANCE,
                        GPIO_INPUT_INTR_LINE,
                        GPIO_INPUT_PIN) & (1u << GPIO_INPUT_PIN))
    {
        GPIOPinIntClear(GPIO_INPUT_INSTANCE,
                        GPIO_INPUT_INTR_LINE,
                        GPIO_INPUT_PIN);
    }
    gGpioIsrEdgeCount++;
    gGpioIsrFlag = 1;
}

/******************************* End of file *********************************/
