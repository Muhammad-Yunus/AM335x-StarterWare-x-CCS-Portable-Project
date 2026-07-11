/**
 * \file   main.c
 *
 *  \brief  Blinks the user LED on the BeagleBone using GPIO1[23].
 *
 *          Application Use Case:
 *              1) GPIO1[23] is used as an output pin.
 *              2) The pin is alternately driven HIGH and LOW with a
 *                 delay between transitions, producing a visible blink.
 */

#include "soc_AM335x.h"
#include "beaglebone.h"
#include "gpio_v2.h"


#define GPIO_INSTANCE_ADDRESS           (SOC_GPIO_1_REGS)
#define GPIO_INSTANCE_PIN_NUMBER        (23)


static void Delay(unsigned int count);

int main(void)
{
    GPIO1ModuleClkConfig();

    GPIO1Pin23PinMuxSetup();

    GPIOModuleEnable(GPIO_INSTANCE_ADDRESS);
    GPIOModuleReset(GPIO_INSTANCE_ADDRESS);

    GPIODirModeSet(GPIO_INSTANCE_ADDRESS,
                   GPIO_INSTANCE_PIN_NUMBER,
                   GPIO_DIR_OUTPUT);

    while(1)
    {
        GPIOPinWrite(GPIO_INSTANCE_ADDRESS,
                     GPIO_INSTANCE_PIN_NUMBER,
                     GPIO_PIN_HIGH);

        Delay(0x3FFFF);

        GPIOPinWrite(GPIO_INSTANCE_ADDRESS,
                     GPIO_INSTANCE_PIN_NUMBER,
                     GPIO_PIN_LOW);

        Delay(0x3FFFF);
    }
}

static void Delay(volatile unsigned int count)
{
    while(count--);
}
