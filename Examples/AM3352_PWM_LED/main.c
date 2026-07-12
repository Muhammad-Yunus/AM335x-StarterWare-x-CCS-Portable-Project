/**
 * \file    main.c
 *
 * \brief   WIP: eHRPWM0A on P9_22 (GPMC_AD2, MUXMODE 6).
 *          Bare-metal AM3352, StarterWare 02.00.01.01.
 *          Status: WIP — see ../memory/project_am3352_pwm_led.md
 */

#include "soc_AM335x.h"
#include "hw_control_AM335x.h"
#include "hw_cm_per.h"
#include "hw_types.h"
#include "ehrpwm.h"

#define EPWM0_BASE   SOC_EPWM_0_REGS
#define PWMSS0_BASE  SOC_PWMSS0_REGS
#define TBCLK_HZ     (10000000U)   /* 100 MHz / 10 */
#define PERIOD_TBPRD (0xFFU)       /* ~39 kHz */

int main(void)
{
    /* PRCM: ungate PWMSS0 functional + interface clocks */
    HWREG(SOC_CM_PER_REGS + CM_PER_EPWMSS0_CLKCTRL) |=
        CM_PER_EPWMSS0_CLKCTRL_MODULEMODE_ENABLE;
    while ((HWREG(SOC_CM_PER_REGS + CM_PER_EPWMSS0_CLKCTRL) &
            CM_PER_EPWMSS0_CLKCTRL_MODULEMODE) !=
           CM_PER_EPWMSS0_CLKCTRL_MODULEMODE_ENABLE);

    /* Control module: gate TBCLK + route P9_22 to ehrpwm0A */
    HWREG(SOC_CONTROL_REGS + CONTROL_PWMSS_CTRL) |=
        CONTROL_PWMSS_CTRL_PWMSS0_TBCLKEN;
    HWREG(SOC_CONTROL_REGS + CONTROL_CONF_GPMC_AD(2)) =
        CONTROL_CONF_MUXMODE(6);

    /* Inside PWMSS0: ungate ePWM logic clock */
    EHRPWMClockEnable(PWMSS0_BASE);

    /* Time base */
    EHRPWMTimebaseClkConfig(EPWM0_BASE, TBCLK_HZ, TBCLK_HZ * 10U);
    EHRPWMPWMOpFreqSet(EPWM0_BASE, TBCLK_HZ, TBCLK_HZ,
                       EHRPWM_COUNT_UP, EHRPWM_SHADOW_WRITE_DISABLE);

    /* 50% duty: HIGH at ZRO, LOW at CAU */
    EHRPWMLoadCMPA(EPWM0_BASE, 50U,
                   EHRPWM_SHADOW_WRITE_DISABLE,
                   EHRPWM_COMPA_NO_LOAD,
                   EHRPWM_CMPCTL_OVERWR_SH_FL);
    EHRPWMConfigureAQActionOnA(EPWM0_BASE,
        EHRPWM_AQCTLA_ZRO_EPWMXAHIGH,
        EHRPWM_AQCTLA_PPRD_DONOTHING,
        EHRPWM_AQCTLA_CAU_EPWMXALOW,
        EHRPWM_AQCTLA_CAD_DONOTHING,
        EHRPWM_AQCTLA_CBU_DONOTHING,
        EHRPWM_AQCTLA_CBD_DONOTHING,
        EHRPWM_AQSFRC_ACTSFA_DONOTHING);

    /* Disable optional sub-modules not needed for a simple toggle */
    EHRPWMHRDisable(EPWM0_BASE);

    while (1) { /* PWM hardware drives P9_22 */ }
}