/*
 * Local override of StarterWare's platform/beaglebone/dmtimer.c.
 *
 * Antminer L3+ clocks the AM3352 differently than BeagleBone Black.
 * The original DMTimer1msModuleClkConfig() spin-loops on PRCM status
 * bits that never settle, hanging the board. Re-implement without
 * the blocking waits; registers writes still take effect.
 *
 * Exposed as DMTimer1msModuleClkConfig_Local. The local demoTimer.c
 * is patched to call this variant, sidestepping the duplicate-symbol
 * clash with platform.lib's DMTimer1msModuleClkConfig.
 */

#include "hw_cm_dpll.h"
#include "hw_cm_wkup.h"
#include "soc_AM335x.h"
#include "hw_types.h"

void DMTimer1msModuleClkConfig_Local(unsigned int clkselect)
{
    unsigned int reg;

    reg = HWREG(SOC_CM_DPLL_REGS + CM_DPLL_CLKSEL_TIMER1MS_CLK);
    reg &= ~CM_DPLL_CLKSEL_TIMER1MS_CLK_CLKSEL;
    reg |= clkselect & CM_DPLL_CLKSEL_TIMER1MS_CLK_CLKSEL;
    HWREG(SOC_CM_DPLL_REGS + CM_DPLL_CLKSEL_TIMER1MS_CLK) = reg;

    reg = HWREG(SOC_CM_WKUP_REGS + CM_WKUP_TIMER1_CLKCTRL);
    reg |= CM_WKUP_TIMER1_CLKCTRL_MODULEMODE_ENABLE;
    HWREG(SOC_CM_WKUP_REGS + CM_WKUP_TIMER1_CLKCTRL) = reg;
}
