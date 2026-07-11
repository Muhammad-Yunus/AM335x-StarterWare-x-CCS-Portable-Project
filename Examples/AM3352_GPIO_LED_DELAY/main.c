/**
 * \file   main.c
 *
 * \brief  Blinks the user LED on the BeagleBone using GPIO1[23] with a
 *         500 ms toggle period.
 *
 *         Uses delay() from StarterWare's delay.h, which routes through
 *         Sysdelay() -> DMTimer7 overflow IRQ. DelayTimerSetup() configures
 *         the timer and registers DMTimerIsr at startup.
 */

#include "soc_AM335x.h"
#include "beaglebone.h"
#include "gpio_v2.h"
#include "interrupt.h"
#include "delay.h"

#define GPIO_INSTANCE_ADDRESS    (SOC_GPIO_1_REGS)
#define GPIO_INSTANCE_PIN_NUMBER (23)
#define BLINK_PERIOD_MS          (500U)

int main(void)
{
    /* Initialize the ARM interrupt controller. Required before any IRQ-based
     * code (including delay.h's Sysdelay) can fire: resets AINTC, sets the
     * priority threshold, and seeds fnRAMVectors[] with IntDefaultHandler.
     * Matches the pattern in examples/evmAM335x/dmtimer/dmtimerCounter.c. */
    IntAINTCInit();

    /* Enable IRQs at the CPU (clear I-bit in CPSR). Without this, even a
     * perfectly wired AINTC/Timer7 never enters the IRQ vector. */
    IntMasterIRQEnable();

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
        delay(BLINK_PERIOD_MS);

        GPIOPinWrite(GPIO_INSTANCE_ADDRESS, GPIO_INSTANCE_PIN_NUMBER, GPIO_PIN_LOW);
        delay(BLINK_PERIOD_MS);
    }
}
