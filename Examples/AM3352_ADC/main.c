/**
 * \file    main.c
 *
 * \brief   AM3352 ADC demo: read AIN0 (P9_39) and print raw + voltage on UART0.
 *
 *          Target: AM3352 (Antminer L3+ control board, BBB-like header).
 *          ADC channel: AIN0 wired to header pin P9_39 via conf_rgmii1_txc.
 *
 *          Application behaviour:
 *            1) UART0 @ 115200 8N1 with FIFO configured.
 *            2) On startup, print a one-shot banner via UART.
 *            3) In the main loop, re-trigger ADC step 1 on AIN0, read FIFO0,
 *               and print "[AIN0] raw=NNNN mV=MMMM". Sample interval is
 *               ADC_PERIOD_MS milliseconds, using StarterWare delay.h which
 *               routes through DMTimer7's overflow IRQ.
 *
 *          ADC configuration follows
 *          C:/ti/AM335X_StarterWare_02_00_01_01/examples/evmAM335x/adc/adcVoltMeasure.c
 *          but writes its own PRCM clock gating + pinmux so we don't have to
 *          pull in evmAM335x.h (which conflicts with beaglebone.h).
 */

/*
 * Copyright (C) 2010 Texas Instruments Incorporated - http://www.ti.com/
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *    Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *    Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 */

#include "uart_irda_cir.h"
#include "soc_AM335x.h"
#include "hw_types.h"
#include "hw_control_AM335x.h"
#include "hw_cm_wkup.h"
#include "interrupt.h"
#include "beaglebone.h"
#include "tsc_adc.h"
#include "delay.h"

/******************************************************************************
**              INTERNAL MACRO DEFINITIONS
******************************************************************************/
#define BAUD_RATE_115200          (115200)
#define UART_MODULE_INPUT_CLK     (48000000)

/* AM335x control module base (not exported by StarterWare headers). */
#define SOC_CONTROL_MODULE_REGS   (0x44E10000u)

/* P9_39 (AIN0) is wired to the rgmii1_txc ball, controlled by the
 * conf_rgmii1_txc pad-config register (TRM spruh73p, sec 9.3).
 * Offset 0x8A0 in the control module.
 *
 * Pad-config bit layout (all 32 bits, only a few are meaningful for analog):
 *   [2:0]   MUXMODE       : 0 = analog (AIN0) mode  [default after reset]
 *   [4]     PULLTYPESEL   : 0 = pulldown selected
 *   [5]     PULLUDEN      : 0 = pull enabled
 *   [14]    RXACTIVE      : 1 = receiver enabled (TRM recommends this)
 *
 * For pure analog input we want MUXMODE=0, no pull, RXACTIVE=1 -> 0x4000.
 */
#define CONF_RGMII1_TXC_OFFSET    (0x8A0u)
#define PADCONF_AIN0_VALUE        (0x00004000u)

/* WKUP clock controller: ADC_TSC. Register offset and macros already defined
 * in hw_cm_wkup.h (CM_WKUP_ADC_TSC_CLKCTRL, ..._MODULEMODE_ENABLE,
 * ..._IDLEST_FUNC). We use those directly.
 *
 * ADC sampling math: VDDA = 1.8 V, 12-bit -> 1.8 / 4096 * 1e6 uV = 439.45.
 * Reference uses (sample * 439) / 1000 -> millivolts.
 */
#define RESOL_X_MILLION           (439u)

/* Delay between ADC samples, in milliseconds. Uses StarterWare delay.h
 * which routes through DMTimer7's overflow IRQ. DelayTimerSetup() must be
 * called once during init.
 */
#define ADC_PERIOD_MS             (100u)

/******************************************************************************
**              INTERNAL FUNCTION PROTOTYPES
******************************************************************************/
static void UartFIFOConfigure(void);
static void UartBaudRateSet(void);
static void UartPutString(const char *s);
static char *Uitoa(unsigned int value, char *buf);

static void ADCConfigure(void);
static unsigned int ADCReadAIN0(void);

/******************************************************************************
**              GLOBAL VARIABLE DEFINITIONS
******************************************************************************/
static const char banner[] =
    "\r\n=== AM3352 ADC DEMO ===\r\n"
    "Reading AIN0 (P9_39) every 500 ms.\r\n";

/******************************************************************************
**              UART helpers
******************************************************************************/
static void UartPutString(const char *s)
{
    while(*s != '\0')
    {
        UARTCharPut(SOC_UART_0_REGS, (unsigned char)*s);
        s++;
    }
}

/******************************************************************************
**              ADC helpers
******************************************************************************/

/*
** Enable the ADC_TSC module clock in the WKUP domain and wait for it to be
** functional. Replaces TSCADCModuleClkConfig() from evmAM335x.h (which we
** don't include because it would clash with beaglebone.h).
*/
static void AdcModuleClockEnable(void)
{
    volatile unsigned int reg;

    HWREG(SOC_CM_WKUP_REGS + CM_WKUP_ADC_TSC_CLKCTRL) |=
        CM_WKUP_ADC_TSC_CLKCTRL_MODULEMODE_ENABLE;

    /* Poll IDLEST until the module reports functional clock. */
    do
    {
        reg = HWREG(SOC_CM_WKUP_REGS + CM_WKUP_ADC_TSC_CLKCTRL);
    } while((reg & CM_WKUP_ADC_TSC_CLKCTRL_IDLEST) !=
            (CM_WKUP_ADC_TSC_CLKCTRL_IDLEST_FUNC <<
             CM_WKUP_ADC_TSC_CLKCTRL_IDLEST_SHIFT));
}

/*
** Configure P9_39 (AIN0) as an analog input via conf_rgmii1_txc.
** StarterWare doesn't ship a pin_mux macro for this register, so we write it
** directly. Mode 0 = analog, RX active, no pull.
*/
static void AIN0PinMuxSetup(void)
{
    HWREG(SOC_CONTROL_MODULE_REGS + CONF_RGMII1_TXC_OFFSET) = PADCONF_AIN0_VALUE;
}

/*
** Configure the TSC_ADC_SS controller for one-shot, single-ended AIN0 reads
** on step 1 routed to FIFO 0.
*/
static void ADCConfigure(void)
{
    AdcModuleClockEnable();
    AIN0PinMuxSetup();

    /* AFE clock: 24 MHz functional / 8 = 3 MHz ADC clock. */
    TSCADCConfigureAFEClock(SOC_ADC_TSC_0_REGS, 24000000, 3000000);

    /* Pure ADC mode: turn off all touchscreen drive transistors. */
    TSCADCTSTransistorConfig(SOC_ADC_TSC_0_REGS, TSCADC_TRANSISTOR_DISABLE);

    /* Tag each FIFO entry with its step ID (useful when debugging). */
    TSCADCStepIDTagConfig(SOC_ADC_TSC_0_REGS, 1);

    /* Allow writes to STEPCONF registers. */
    TSCADCStepConfigProtectionDisable(SOC_ADC_TSC_0_REGS);

    /* Step 1: single-ended, channel 1 (AIN0) positive, VSSA negative,
     * VDDA positive reference, FIFO 0.
     */
    TSCADCTSStepOperationModeControl(SOC_ADC_TSC_0_REGS,
                                     TSCADC_SINGLE_ENDED_OPER_MODE, 1);

    TSCADCTSStepConfig(SOC_ADC_TSC_0_REGS, 1,
                       TSCADC_NEGATIVE_REF_VSSA,
                       TSCADC_POSITIVE_INP_CHANNEL1,
                       TSCADC_NEGATIVE_INP_CHANNEL1,
                       TSCADC_POSITIVE_REF_VDDA);

    /* All analog switches off (general-purpose mode). */
    TSCADCTSStepAnalogSupplyConfig(SOC_ADC_TSC_0_REGS,
                                  TSCADC_XPPSW_PIN_OFF,
                                  TSCADC_XNPSW_PIN_OFF,
                                  TSCADC_YPPSW_PIN_OFF,
                                  1);

    TSCADCTSStepAnalogGroundConfig(SOC_ADC_TSC_0_REGS,
                                  TSCADC_XNNSW_PIN_OFF,
                                  TSCADC_YPNSW_PIN_OFF,
                                  TSCADC_YNNSW_PIN_OFF,
                                  TSCADC_WPNSW_PIN_OFF,
                                  1);

    TSCADCTSStepFIFOSelConfig(SOC_ADC_TSC_0_REGS, 1, TSCADC_FIFO_0);

    /* Software one-shot: each conversion needs a fresh step-enable toggle. */
    TSCADCTSStepModeConfig(SOC_ADC_TSC_0_REGS, 1,
                           TSCADC_ONE_SHOT_SOFTWARE_ENABLED);

    /* Mode = general-purpose (no touchscreen logic). */
    TSCADCTSModeConfig(SOC_ADC_TSC_0_REGS, TSCADC_GENERAL_PURPOSE_MODE);

    /* Enable step 1 in the sequencer. */
    TSCADCConfigureStepEnable(SOC_ADC_TSC_0_REGS, 1, 1);

    /* Clear any stale interrupts before we start. */
    TSCADCIntStatusClear(SOC_ADC_TSC_0_REGS, 0x7FF);
    TSCADCIntStatusClear(SOC_ADC_TSC_0_REGS, 0x7FF);
    TSCADCIntStatusClear(SOC_ADC_TSC_0_REGS, 0x7FF);

    /* Fire up the controller. */
    TSCADCModuleStateSet(SOC_ADC_TSC_0_REGS, TSCADC_MODULE_ENABLE);
}

/*
** Trigger one AIN0 conversion and return the 12-bit sample (0..4095).
** The controller was set up for software one-shot, so each call:
**   1. Disables then re-enables step 1 to re-arm the one-shot.
**   2. Spins until FIFO0 has data.
**   3. Reads the FIFO.
*/
static unsigned int ADCReadAIN0(void)
{
    unsigned int count;
    unsigned int raw;

    /* Re-arm: toggle the step-enable bit. Disable then enable. */
    TSCADCConfigureStepEnable(SOC_ADC_TSC_0_REGS, 1, 0);
    TSCADCConfigureStepEnable(SOC_ADC_TSC_0_REGS, 1, 1);

    /* Poll FIFO0 count. */
    do
    {
        count = TSCADCFIFOWordCountRead(SOC_ADC_TSC_0_REGS, TSCADC_FIFO_0);
    } while(count == 0u);

    raw = TSCADCFIFOADCDataRead(SOC_ADC_TSC_0_REGS, TSCADC_FIFO_0);
    return (raw & 0xFFFu);
}

/*
** Format a small unsigned int into the supplied buffer as decimal text.
** Buffer must hold at least 7 chars. Returns pointer to the NUL terminator.
*/
static char *Uitoa(unsigned int value, char *buf)
{
    char tmp[7];
    int  i = 0;
    int  j;

    if(value == 0u)
    {
        buf[0] = '0';
        buf[1] = '\0';
        return &buf[1];
    }

    while(value > 0u)
    {
        tmp[i++] = (char)('0' + (value % 10u));
        value /= 10u;
    }

    for(j = 0; j < i; j++)
    {
        buf[j] = tmp[i - 1 - j];
    }
    buf[i] = '\0';
    return &buf[i];
}

/******************************************************************************
**              MAIN
******************************************************************************/
int main(void)
{
    /* ---- UART bring-up ---- */
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


    /* ---- ADC bring-up ---- */
    ADCConfigure();

    /* delay() routes through DMTimer7 overflow IRQ (sysdelay.c:131 busy-waits
     * on flagIsr until DMTimerIsr sets it). Three things must be in place,
     * and the order matters for the debugger:
     *   1) IntAINTCInit()        : reset AINTC, seed fnRAMVectors[], priority
     *                              threshold, IntDefaultHandler. Required
     *                              before any IRQ-based code can fire.
     *   2) IntMasterIRQEnable()  : clear I-bit in CPSR so CPU accepts IRQs.
     *                              Must come BEFORE DelayTimerSetup(): that
     *                              function calls IntSystemEnable(TINT7),
     *                              and if any spurious edge fires between
     *                              here and there the debugger can get
     *                              stuck in the IRQ handler when stepping.
     *   3) DelayTimerSetup()     : enable DMTimer7 clock, register DMTimerIsr
     *                              at SYS_INT_TINT7, enable that system IRQ.
     *                              Timer itself isn't started here — it
     *                              starts the first time delay() is called.
     */
    IntAINTCInit();
    IntMasterIRQEnable();
    DelayTimerSetup();

    UartPutString(banner);

    while(1)
    {
        unsigned int raw = ADCReadAIN0();
        unsigned int mV  = (raw * RESOL_X_MILLION) / 1000u;
        char         rawStr[8];
        char         mvStr[8];

        Uitoa(raw, rawStr);
        Uitoa(mV,  mvStr);

        UartPutString("[AIN0] raw=");
        UartPutString(rawStr);
        UartPutString(" mV=");
        UartPutString(mvStr);
        UartPutString("\r\n");

        delay(ADC_PERIOD_MS);
    }
}

/******************************************************************************
**              UART config + ISR (mirrors uartEcho)
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

/******************************* End of file *********************************/
