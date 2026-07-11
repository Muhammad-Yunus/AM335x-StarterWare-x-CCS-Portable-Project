/**
 * \file   main.c
 *
 * \brief  Blinks the user LED on the BeagleBone using GPIO1[23] with a
 *         500 ms toggle period.
 *
 *         Uses inline busy-wait on DMTimer7 (the same algorithm as
 *         sysdelay.c's #else branch) instead of the IRQ-based delay()
 *         from StarterWare. This isolates the test from DMTimer7 IRQ
 *         setup, so we can confirm that GPIO + DMTimer7 clocking work
 *         independently of interrupt routing.
 */

#include "soc_AM335x.h"
#include "beaglebone.h"
#include "gpio_v2.h"
#include "dmtimer.h"

#define GPIO_INSTANCE_ADDRESS    (SOC_GPIO_1_REGS)
#define GPIO_INSTANCE_PIN_NUMBER (23)
#define TIMER_1MS_COUNT          (24000U)
#define BLINK_PERIOD_MS          (1000U)

static void delay_ms_poll(unsigned int milliSec)
{
    while (milliSec--)
    {
        DMTimerCounterSet(SOC_DMTIMER_7_REGS, 0);
        DMTimerEnable(SOC_DMTIMER_7_REGS);
        while (DMTimerCounterGet(SOC_DMTIMER_7_REGS) < TIMER_1MS_COUNT) ;
        DMTimerDisable(SOC_DMTIMER_7_REGS);
    }
}

int main(void)
{
    GPIO1ModuleClkConfig();
    GPIO1Pin23PinMuxSetup();

    GPIOModuleEnable(GPIO_INSTANCE_ADDRESS);
    GPIOModuleReset(GPIO_INSTANCE_ADDRESS);
    GPIODirModeSet(GPIO_INSTANCE_ADDRESS,
                   GPIO_INSTANCE_PIN_NUMBER,
                   GPIO_DIR_OUTPUT);

    DelayTimerSetup();

    while (1)
    {
        GPIOPinWrite(GPIO_INSTANCE_ADDRESS, GPIO_INSTANCE_PIN_NUMBER, GPIO_PIN_HIGH);
        delay_ms_poll(BLINK_PERIOD_MS);

        GPIOPinWrite(GPIO_INSTANCE_ADDRESS, GPIO_INSTANCE_PIN_NUMBER, GPIO_PIN_LOW);
        delay_ms_poll(BLINK_PERIOD_MS);
    }
}
