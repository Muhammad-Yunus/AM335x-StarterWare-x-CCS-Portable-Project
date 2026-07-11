/**
 * \file   main.c
 *
 * \brief  Onboard LED sequence animation for the AM3352 Antminer L3+ board
 *         (PCB-identik BeagleBone Black).
 *
 *         BeagleBone Black onboard user LED mapping (also valid for L3+):
 *             D2 (USR0) -> GPIO1[21] -> GPMC_A5
 *             D3 (USR1) -> GPIO1[22] -> GPMC_A6
 *             D4 (USR2) -> GPIO1[23] -> GPMC_A7
 *             D5 (USR3) -> GPIO1[24] -> GPMC_A8
 *
 *         StarterWare's beaglebone.h hanya menyediakan helper
 *         GPIO1Pin23PinMuxSetup() untuk D4. Pin D2/D3/D5 di-setup manual
 *         lewat CONTROL_CONF_GPMC_A(n) dengan MUXMODE 7 (sama dengan yang
 *         dipakai helper bawaan StarterWare untuk D4 - konfirmasi pola).
 *
 *         Pola animasi: running light D2 -> D3 -> D4 -> D5 -> D2 -> ...
 *         Kecepatan perpindahan step dikontrol STEP_PERIOD_MS.
 */

#include "soc_AM335x.h"
#include "beaglebone.h"
#include "gpio_v2.h"
#include "interrupt.h"
#include "delay.h"
#include "hw_control_AM335x.h"
#include "hw_cm_wkup.h"

#define GPIO_INSTANCE_ADDRESS    (SOC_GPIO_1_REGS)

#define LED_D2_PIN               (21U)   /* GPMC_A5 */
#define LED_D3_PIN               (22U)   /* GPMC_A6 */
#define LED_D4_PIN               (23U)   /* GPMC_A7 */
#define LED_D5_PIN               (24U)   /* GPMC_A8 */

#define STEP_PERIOD_MS           (200U)

static const unsigned int gLedPins[] = {
    LED_D2_PIN,
    LED_D3_PIN,
    LED_D4_PIN,
    LED_D5_PIN,
};

#define LED_COUNT (sizeof(gLedPins) / sizeof(gLedPins[0]))

int main(void)
{
    unsigned int step = 0;

    IntAINTCInit();
    IntMasterIRQEnable();

    GPIO1ModuleClkConfig();

    GPIO1Pin23PinMuxSetup();
    HWREG(SOC_CONTROL_REGS + CONTROL_CONF_GPMC_A(5)) = CONTROL_CONF_MUXMODE(7);
    HWREG(SOC_CONTROL_REGS + CONTROL_CONF_GPMC_A(6)) = CONTROL_CONF_MUXMODE(7);
    HWREG(SOC_CONTROL_REGS + CONTROL_CONF_GPMC_A(8)) = CONTROL_CONF_MUXMODE(7);

    GPIOModuleEnable(GPIO_INSTANCE_ADDRESS);
    GPIOModuleReset(GPIO_INSTANCE_ADDRESS);

    for (step = 0; step < LED_COUNT; step++)
    {
        GPIODirModeSet(GPIO_INSTANCE_ADDRESS,
                       gLedPins[step],
                       GPIO_DIR_OUTPUT);
        GPIOPinWrite(GPIO_INSTANCE_ADDRESS, gLedPins[step], GPIO_PIN_LOW);
    }

    DelayTimerSetup();

    while (1)
    {
        for (step = 0; step < LED_COUNT; step++)
        {
            unsigned int other;

            for (other = 0; other < LED_COUNT; other++)
            {
                GPIOPinWrite(GPIO_INSTANCE_ADDRESS,
                             gLedPins[other],
                             GPIO_PIN_LOW);
            }

            GPIOPinWrite(GPIO_INSTANCE_ADDRESS,
                         gLedPins[step],
                         GPIO_PIN_HIGH);
            delay(STEP_PERIOD_MS);
        }
    }
}
