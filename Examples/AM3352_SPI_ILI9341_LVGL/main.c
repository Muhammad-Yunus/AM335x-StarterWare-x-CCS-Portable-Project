/*
 * main.c — LVGL v9 music demo on AM3352 SPI0 + ILI9341.
 *
 *  SPI0 channel 0:
 *      CLK  = P9_22 (spi0_sclk)
 *      MOSI = P9_18 (spi0_d1)       -- driven via MCSPI_DATA_LINE_COMM_MODE_1
 *      CS   = P9_17 (spi0_cs0)      -- hardware CS, active low
 *  GPIO (data/command + reset for the LCD):
 *      DC  = P8_26 = GPIO1[29] (gpmc_csn0)
 *      RST = P8_19 = GPIO0[22] (gpmc_ad8)
 *
 *  Routing (antminer L3+, board ~ BeagleBone):
 *      ILI9341 LED pin is left floating/board-driven. We pull RST high
 *      after the boot sequence and leave DC controlled per-byte by
 *      spi helpers. No TE pin is wired.
 *
 *  Boot sequence:
 *      1) Init ARM IRQ + DMTimer7 (delay.h uses it).
 *      2) Bring up GPIO1[29] (DC) and GPIO0[22] (RST); pin-mux to mode 7.
 *      3) Bring up SPI0 controller @ ~10 MHz, 8-bit, mode 0.
 *      4) Reconfigure DMTimer7 to 1 ms auto-reload and install ISR that
 *         calls lv_tick_inc(1).
 *      5) lv_init() + lv_port_disp_init() (which calls ILI9341_Init()).
 *      6) Run the music demo with auto-play; pump lv_timer_handler().
 *
 *  NOTE: delay.h routes through Sysdelay() which busy-waits on a flag set
 *  by DMTimer7's overflow IRQ. We MUST call IntAINTCInit() and
 *  IntMasterIRQEnable() first, or Sysdelay() hangs forever.
 */

#include "soc_AM335x.h"
#include "beaglebone.h"
#include "gpio_v2.h"
#include "hw_control_AM335x.h"
#include "hw_cm_per.h"
#include "hw_types.h"
#include "mcspi.h"
#include "edma.h"
#include "edma_event.h"
#include "interrupt.h"
#include "delay.h"
#include "dmtimer.h"

#include "lvgl.h"
#include "demos/lv_demos.h"

#include "ili9341.h"
#include "lv_port_disp.h"

/* ---------------------------------------------------------------------- */
/* Pin mapping                                                             */
/* ---------------------------------------------------------------------- */
#define DC_GPIO_BASE     (SOC_GPIO_1_REGS)
#define DC_GPIO_PIN      (29)
#define RST_GPIO_BASE    (SOC_GPIO_0_REGS)
#define RST_GPIO_PIN     (22)

#define SPI_BASE         (SOC_SPI_0_REGS)
#define SPI_CH           (0)
#define MCSPI_IN_CLK     (48000000u)
#define MCSPI_OUT_FREQ   (24000000u)   /* target — McSPIClkConfig ignores this for
                                        * power-of-2 fRatio; actual speed set by
                                         * CLKD=0 override below */

#define DMTIMER_BASE     (SOC_DMTIMER_7_REGS)

/* DEBUG LED: GPIO1[23] = silkscreen D2 on antminer L3+.
 * Toggled from PollLvglTick() to verify tick polling visually. */
#define DEBUG_LED_GPIO_BASE  (SOC_GPIO_1_REGS)
#define DEBUG_LED_PIN        (23)

/* ---------------------------------------------------------------------- */
/* Function prototypes                                                     */
/* ---------------------------------------------------------------------- */
static void Spi0Init(void);
static void Edma3Init(void);
static void EdmaSpiTx(const uint8_t *src, uint32_t byteCount);
static void Spi0Init(void);
void Spi0TxByte(uint8_t b);
void Spi0TxBuffer(const uint8_t *buf, uint32_t len);
void Spi0TxBufferFast(const uint8_t *buf, uint32_t len);

/* GPIO hooks exposed to ili9341.c. */
void LcdDcLow(void)  { GPIOPinWrite(DC_GPIO_BASE,  DC_GPIO_PIN,  GPIO_PIN_LOW);  }
void LcdDcHigh(void) { GPIOPinWrite(DC_GPIO_BASE,  DC_GPIO_PIN,  GPIO_PIN_HIGH); }
void LcdRstLow(void) { GPIOPinWrite(RST_GPIO_BASE, RST_GPIO_PIN, GPIO_PIN_LOW);  }
void LcdRstHigh(void){ GPIOPinWrite(RST_GPIO_BASE, RST_GPIO_PIN, GPIO_PIN_HIGH); }

/* ---------------------------------------------------------------------- */
/* Background color cycle timer callback                                    */
/* ---------------------------------------------------------------------- */
static const uint32_t bg_colors[] = {
    0xFF0000, 0x00FF00, 0x0000FF, 0xFFFF00, 0xFF00FF, 0x00FFFF,
    0x800000, 0x008000, 0x000080, 0x808000, 0x800080, 0x008080,
};
static uint8_t bg_idx = 0;

#if 0
static void bg_cycle_timer_cb(lv_timer_t *t)
{
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(bg_colors[bg_idx]), LV_PART_MAIN);
    bg_idx = (bg_idx + 1) % 12;
    (void)t;
}
#endif

/* ---------------------------------------------------------------------- */
/* 1 ms LVGL tick (DMTimer7 polled, no IRQ)                                */
/* ---------------------------------------------------------------------- */
/* LVGL tick: software-driven from the main loop. We poll the DMTimer7
 * counter register (TCRR) and increment lv_tick_inc() whenever it
 * crosses a 1 ms boundary. No IRQ, no AINTC setup. */
static volatile uint32_t s_lastCounter = 0;
static volatile uint32_t s_tickCount = 0;

#if 0
static void PollLvglTick(void)
{
    uint32_t now = DMTimerCounterGet(DMTIMER_BASE);
    uint32_t elapsed = now - s_lastCounter;
    if (elapsed >= 24000u)
    {
        s_lastCounter = now;
        s_tickCount++;

        lv_tick_inc(elapsed / 24000u);
    }
}

static void LvglTickInit(void)
{
    DMTimer7ModuleClkConfig();

    DMTimerDisable(DMTIMER_BASE);

    /* Free-running 32-bit up counter at 24 MHz. No compare, no reload —
     * we poll TCRR directly. Each 24000 ticks ≈ 1 ms. */
    DMTimerModeConfigure(DMTIMER_BASE, DMTIMER_ONESHOT_NOCMP_ENABLE);

    DMTimerCounterSet(DMTIMER_BASE, 0);
    s_lastCounter = 0;

    DMTimerEnable(DMTIMER_BASE);
}
#endif

/* ---------------------------------------------------------------------- */
/* main                                                                    */
/* ---------------------------------------------------------------------- */
int main(void)
{
    /* IRQ path required by delay.h. We do not install any IRQ handler
     * of our own — software tick is polled in main loop. */
    IntAINTCInit();
    IntMasterIRQEnable();

    /* GPIO clocks + pin-mux for DC and RST to GPIO mode 7. */
    GPIO0ModuleClkConfig();
    GPIO1ModuleClkConfig();

    GpioPinMuxSetup(CONTROL_CONF_GPMC_CSN(0), CONTROL_CONF_MUXMODE(7));
    GpioPinMuxSetup(CONTROL_CONF_GPMC_AD(8),  CONTROL_CONF_MUXMODE(7));

    /* DEBUG LED (D2 on antminer L3+): GPIO1[23]. Toggled from PollLvglTick. */
    GPIO1Pin23PinMuxSetup();
    GPIOModuleEnable(DEBUG_LED_GPIO_BASE);
    GPIOModuleReset(DEBUG_LED_GPIO_BASE);
    GPIODirModeSet(DEBUG_LED_GPIO_BASE, DEBUG_LED_PIN, GPIO_DIR_OUTPUT);
    GPIOPinWrite(DEBUG_LED_GPIO_BASE, DEBUG_LED_PIN, GPIO_PIN_LOW);

    GPIOModuleEnable(DC_GPIO_BASE);
    GPIOModuleReset(DC_GPIO_BASE);
    GPIODirModeSet(DC_GPIO_BASE, DC_GPIO_PIN, GPIO_DIR_OUTPUT);

    GPIOModuleEnable(RST_GPIO_BASE);
    GPIOModuleReset(RST_GPIO_BASE);
    GPIODirModeSet(RST_GPIO_BASE, RST_GPIO_PIN, GPIO_DIR_OUTPUT);

    /* Both idle LOW until init; ILI9341_Init() drives RST HIGH after
     * a short delay. DC stays LOW (command interpretation) until
     * LVGL flush flips it before each data byte. */
    GPIOPinWrite(DC_GPIO_BASE,  DC_GPIO_PIN,  GPIO_PIN_LOW);
    GPIOPinWrite(RST_GPIO_BASE, RST_GPIO_PIN, GPIO_PIN_LOW);

    /* DIAG: GPIO bus speed BEFORE L4LS wakeup — probe DC (P8_26) now.
     * A tight GPIO toggle loop shows the bus clock rate: */
    { uint32_t _di; for (_di = 0; _di < 20000; _di++) {
        GPIOPinWrite(DC_GPIO_BASE, DC_GPIO_PIN, GPIO_PIN_HIGH);
        GPIOPinWrite(DC_GPIO_BASE, DC_GPIO_PIN, GPIO_PIN_LOW);
    }}

    /* Wake up the L4LS clock domain before SPI init (DMTimer7ModuleClkConfig
     * inside DelayTimerSetup sets CM_PER_L4LS_CLKSTCTRL to SW_WKUP).
     * Without this, the McSPI module clock may run at a critically low
     * frequency (~1 MHz), resulting in ~500 kHz SPI_CLK even with CLKD=0. */
    DelayTimerSetup();

    /* DIAG: GPIO bus speed AFTER L4LS wakeup — probe DC (P8_26) again.
     * If toggle rate is the same as before, L4LS is NOT the bottleneck. */
    { uint32_t _di; for (_di = 0; _di < 20000; _di++) {
        GPIOPinWrite(DC_GPIO_BASE, DC_GPIO_PIN, GPIO_PIN_HIGH);
        GPIOPinWrite(DC_GPIO_BASE, DC_GPIO_PIN, GPIO_PIN_LOW);
    }}

    /* Bring up SPI0. */
    Spi0Init();

    /* ================================================================
     * MINIMAL SPI CLOCK TEST
     * ================================================================
     * Commented out: LVGL, display init, timer, GPIO diagnostics.
     *
     * SPI master configured, CLKD=0 → SPI_CLK = FUNC_CLK / 2.
     * Probe P9_22 (SPI0_SCLK) — this loop keeps the TX FIFO non-empty
     * so the SPI bit-rate engine generates a continuous clock.
     *
     * Expected clock period (CLKD=0 → divide by 2):
     *   FUNC_CLK = 48 MHz → SPI_CLK = 24 MHz → period = 42 ns
     *   FUNC_CLK = 24 MHz → SPI_CLK = 12 MHz → period =  83 ns
     *   FUNC_CLK =  1 MHz → SPI_CLK = 500 kHz → period =   2 µs  ← issue
     * ================================================================ */
    McSPICSAssert(SPI_BASE, SPI_CH);
    GPIOPinWrite(DEBUG_LED_GPIO_BASE, DEBUG_LED_PIN, GPIO_PIN_HIGH);
    while (1)
    {
        uint32_t _cnt = 256;
        while (_cnt--)
            HWREG(SPI_BASE + MCSPI_TX(SPI_CH)) = 0xAA;
        while ((HWREG(SPI_BASE + MCSPI_CHSTAT(SPI_CH)) &
                MCSPI_CH0STAT_TXFFF)) { }
    }
}

/* ---------------------------------------------------------------------- */
/* SPI helpers                                                             */
/* ---------------------------------------------------------------------- */
static void Spi0Init(void)
{
    /* McSPI0 functional clock from CM_PER. */
    HWREG(SOC_CM_PER_REGS + CM_PER_SPI0_CLKCTRL) |=
        CM_PER_SPI0_CLKCTRL_MODULEMODE_ENABLE;
    while ((HWREG(SOC_CM_PER_REGS + CM_PER_SPI0_CLKCTRL) &
            CM_PER_SPI0_CLKCTRL_MODULEMODE) !=
           CM_PER_SPI0_CLKCTRL_MODULEMODE_ENABLE) { /* spin */ }

    /* Pin-mux P9_22/P9_18/P9_17 to SPI0 (mode 0). */
    GpioPinMuxSetup(CONTROL_CONF_SPI0_SCLK,
                    CONTROL_CONF_MUXMODE(0) | CONTROL_CONF_RXACTIVE);
    GpioPinMuxSetup(CONTROL_CONF_SPI0_D1,
                    CONTROL_CONF_MUXMODE(0) | CONTROL_CONF_RXACTIVE);
    GpioPinMuxSetup(CONTROL_CONF_SPI0_CS0,
                    CONTROL_CONF_MUXMODE(0) | CONTROL_CONF_RXACTIVE);

    /* Reset controller, then enable master mode. */
    McSPIReset(SPI_BASE);
    while ((HWREG(SPI_BASE + MCSPI_SYSSTATUS) & MCSPI_SYSSTATUS_RESETDONE)
           == 0) { /* spin */ }

    McSPIMasterModeEnable(SPI_BASE);
    McSPICSEnable(SPI_BASE);    /* route SPI0_CS to pin (PIN34=0) */

    /* SYSCONFIG: smart-idle, both clock domains active, no autoidle. */
    HWREG(SPI_BASE + MCSPI_SYSCONFIG) =
        (1u << MCSPI_SYSCONFIG_SIDLEMODE_SHIFT) |
        (1u << MCSPI_SYSCONFIG_CLOCKACTIVITY_SHIFT) |
        (0u << MCSPI_SYSCONFIG_AUTOIDLE_SHIFT);

    /* Channel 0 — single channel, TX-ONLY, D1 output.
     * TRM = 2 = TX-only. McSPI only shifts out TX data. */
    McSPIMasterModeConfig(SPI_BASE,
                          MCSPI_SINGLE_CH,
                          MCSPI_TX_RX_MODE,
                          MCSPI_DATA_LINE_COMM_MODE_1,
                          SPI_CH);
    /* Override TRM field (bits 19:18) to TX-only mode. */
    uint32_t conf = HWREG(SPI_BASE + MCSPI_CHCONF(SPI_CH));
    conf &= ~(0x3u << 12);  /* clear TRM field (bits 13:12) */
    conf |= (2u << 12);     /* TRM = TX-only */
    HWREG(SPI_BASE + MCSPI_CHCONF(SPI_CH)) = conf;

    McSPIWordLengthSet(SPI_BASE, MCSPI_WORD_LENGTH(8), SPI_CH);

    McSPIClkConfig(SPI_BASE,
                   MCSPI_IN_CLK,
                   MCSPI_OUT_FREQ,
                   SPI_CH,
                   MCSPI_CLK_MODE_0);

    McSPICSPolarityConfig(SPI_BASE, MCSPI_CS_POL_LOW, SPI_CH);

    McSPITxFIFOConfig(SPI_BASE, MCSPI_TX_FIFO_ENABLE, SPI_CH);
    McSPIRxFIFOConfig(SPI_BASE, MCSPI_RX_FIFO_DISABLE, SPI_CH);

    /* Enable TX finished interrupt (bit 27) — generates EOT when McSPI is done. */
    HWREG(SPI_BASE + MCSPI_CHCONF(SPI_CH)) |= (1u << 27);

    /* Override CLKD to 0 — McSPIClkConfig always over-divides.
     * For fRatio = 48 MHz / 24 MHz = 2, the function loops: fRatio→2→1,
     * clkD→1, giving SPI_CLK = FUNC_CLK/4 (12 MHz).  CLKD=0 removes
     * the extra ÷2, delivering FUNC_CLK/2 (24 MHz if FUNC_CLK=48 MHz). */
    HWREG(SPI_BASE + MCSPI_CHCONF(SPI_CH)) &= ~MCSPI_CH0CONF_CLKD;

    McSPIChannelEnable(SPI_BASE, SPI_CH);

    /* TURBO mode: remove the inter-word gap within a single CS assertion.
     * With TURBO=1 the SPI master starts shifting the next word as soon as
     * the shift register is empty, maximizing back-to-back throughput. */
    HWREG(SPI_BASE + MCSPI_CHCONF(SPI_CH)) |= MCSPI_CH0CONF_TURBO;

    /* Set TX FIFO Almost-Full Level to 64 bytes (25 % of 256-byte FIFO).
     * TXFFF = 1 when FIFO > 64 bytes; TXFFF = 0 when FIFO ≤ 64 bytes.
     * This gives a 192-byte refill window per burst in Spi0TxBufferFast
     * — the FIFO stays well-fed and the SPI shift engine never starves. */
    {
        uint32_t xfer = HWREG(SPI_BASE + MCSPI_XFERLEVEL);
        xfer &= ~MCSPI_XFERLEVEL_AFL;
        xfer |= (64u << MCSPI_XFERLEVEL_AFL_SHIFT);
        HWREG(SPI_BASE + MCSPI_XFERLEVEL) = xfer;
    }

    /* Diagnostic: read back CLKD and blink on LED.
     * Blinks = CLKD+1 (so 1 blink = CLKD=0 = correct). */
    {
        uint32_t chc = HWREG(SPI_BASE + MCSPI_CHCONF(SPI_CH));
        uint32_t clkd = (chc & MCSPI_CH0CONF_CLKD) >> MCSPI_CH0CONF_CLKD_SHIFT;
        uint32_t i;
        for (i = 0; i < 3; i++) {
            GPIOPinWrite(DEBUG_LED_GPIO_BASE, DEBUG_LED_PIN, GPIO_PIN_HIGH);
            delay(100);
            GPIOPinWrite(DEBUG_LED_GPIO_BASE, DEBUG_LED_PIN, GPIO_PIN_LOW);
            delay(100);
        }
        delay(500);
        for (i = 0; i <= clkd; i++) {
            GPIOPinWrite(DEBUG_LED_GPIO_BASE, DEBUG_LED_PIN, GPIO_PIN_HIGH);
            delay(200);
            GPIOPinWrite(DEBUG_LED_GPIO_BASE, DEBUG_LED_PIN, GPIO_PIN_LOW);
            delay(200);
        }
    }
}

void Spi0TxByte(uint8_t b)
{
    McSPICSAssert(SPI_BASE, SPI_CH);
    McSPITransmitData(SPI_BASE, b, SPI_CH);

    /* Wait until the shift register has drained (TXS = Tx Register Empty). */
    while (!((McSPIChannelStatusGet(SPI_BASE, SPI_CH)) &
             MCSPI_CH0STAT_TXS)) { /* spin */ }

    /* Drain RX FIFO so it never overruns. */
    (void)McSPIReceiveData(SPI_BASE, SPI_CH);

    McSPICSDeAssert(SPI_BASE, SPI_CH);
}

void Spi0TxBuffer(const uint8_t *buf, uint32_t len)
{
    /* ili9341.c pushes small blocks (rows / commands); re-assert CS per
     * byte to match Spi0TxByte semantics. */
    uint32_t i;

    for (i = 0; i < len; i++)
        Spi0TxByte(buf[i]);
}

/* ====================================================================== */
/*  DMA-driven SPI TX (EDMA3 channel 16 = MCSPI0_CH0_TX)                  */
/* ====================================================================== */
#define EDMA3_CC_BASE          (SOC_EDMA30CC_0_REGS)
#define EDMA3_TC_BASE          (SOC_EDMA30TC_0_REGS)
#define EDMA3_DMA_CH_SPI_TX    (16u)
#define EDMA3_TCC_SPI_TX       (16u)
#define EDMA_CHUNK             (65535u)          /* Max A-count per shot */

/* EDMA3 CC register offsets (relative to EDMA3_CC_BASE). */
#define EDMA3_CC_DCHMAP        (0x100)           /* Channel map (PaRAM set mapping) */
#define EDMA3_CC_SECR          (0x1040)          /* Software Event Clear Reg */
#define EDMA3_CC_IER           (0x1050)          /* Interrupt Enable Set */
#define EDMA3CC_IPR            (0x1068)          /* Interrupt Pending Reg */
#define EDMA3CC_ICR            (0x1070)          /* Interrupt Clear Reg */
#define EDMA3_CC_DRAE          (0x340)           /* Region Attribute Enable */
#define EDMA3_CC_EMCR          (0x308)           /* Event Clear Register */

/* Shadow Region 0 (ARM Cortex-A8) — S_* registers for AM335x. */
#define EDMA3_SHD0_S_EESR      (0x2030)          /* Enable Event Set Reg (per-DMA enable) */
#define EDMA3_SHD0_S_EECR      (0x2028)          /* Event Enable Clear Reg (per-DMA disable) */
#define EDMA3_SHD0_S_SECR      (0x2040)          /* Secondary Event Clear Reg */
#define EDMA3_SHD0_S_ESR       (0x2020)          /* Software Event Set (manual trigger) */

/* Completion flag — set in main loop after polling. */
static volatile uint32_t s_dmaDone = 0;

/* ---------------------------------------------------------------------- */
/* EDMA3 helper: write 32-bit to arbitrary EDMA register offset.          */
/* ---------------------------------------------------------------------- */
static void EdmaRegWrite(uint32_t reg, uint32_t val)
{
    HWREG(reg) = val;
}

static uint32_t EdmaRegRead(uint32_t reg)
{
    return HWREG(reg);
}

/* ---------------------------------------------------------------------- */
/* Reset EDMA3 CC — puts controller in a known state.                     */
/* Required before EDMA3Init on cold boot.                                */
/* ---------------------------------------------------------------------- */
static void EdmaReset(void)
{
    volatile uint32_t timeout = 0xFFFFFFu;
    uint32_t stat;

    /* Global SW reset: writing 0x0 to SOFT register triggers it */
    EdmaRegWrite(SOC_EDMA30CC_0_REGS + 0x0FC, 0x0);

    /* Wait for RSTSTAT to clear (0 = no outstanding resets) */
    do {
        stat = EdmaRegRead(SOC_EDMA30CC_0_REGS + 0x0F0); /* RSTSTAT */
        if (stat == 0) break;
    } while (--timeout);
}

/* ---------------------------------------------------------------------- */
/* Initialize EDMA3 controller (platform clock + CC init + AINTC mapping). */
/* ---------------------------------------------------------------------- */
static void Edma3Init(void)
{
    /* Enable EDMA3 clocks (TPCC + TPTCs). */
    EDMAModuleClkConfig();

    /* Hard-reset EDMA3 CC to ensure clean state. */
    EdmaReset();

    /* Initialize the EDMA3 Control Module — default region 0. */
    EDMA3Init(EDMA3_CC_BASE, 0);

    /* Request DMA channel 16 for McSPI0 TX with TCC 16.
     * TRIG_MODE_EVENT (2) — event-triggered (hardware-synced). */
    EDMA3RequestChannel(EDMA3_CC_BASE, 0,  /* DMA channel type */
                        EDMA3_DMA_CH_SPI_TX,   /* channel 16 */
                        EDMA3_TCC_SPI_TX,      /* TCC 16 */
                        0);                    /* event queue 0 */

    /* Map DMA channel 16 to IRQ in AINTC (for completion callback).
     * INT_ID_KY would normally be defined here. */
    /* IntAINTCMapInterrupt(16, INT_ID_KY); */

    /* Enable DMA event 16 in EMR (Event Mask Register).
     * This tells EDMA to accept hardware-synced event triggers. */
    EdmaRegWrite(EDMA3_CC_BASE + 0x300, (1u << 16));
}

/* ---------------------------------------------------------------------- */
/* Fire a single-shot DMA transfer from SRAM buffer -> SPI TX register.   */
/* Polls until transfer completes (blocking).                              */
/* ---------------------------------------------------------------------- */
static void EdmaSpiTx(const uint8_t *src, uint32_t byteCount)
{
    uint32_t ch = EDMA3_DMA_CH_SPI_TX;

    /* Clear any lingering EDMA3 interrupt flags and shadow events. */
    EdmaRegWrite(EDMA3_CC_BASE + EDMA3CC_ICR, 1u << ch);          /* Clear IPR */
    EdmaRegWrite(EDMA3_CC_BASE + EDMA3_SHD0_S_SECR, 1u << ch);    /* Clear S_SECR */
    EdmaRegWrite(EDMA3_CC_BASE + EDMA3_CC_SECR, 1u << ch);        /* Clear SECR */

    /* Build PaRAM entry manually.
     * AM35x OPT layout: bits[31:24]=EVENTNO, [17:12]=TCC, [11:8]=FWID,
     *   [3]=STANDBY_PD, [2]=SYNCDIM, [1]=DAM(const), [0]=SAM(increment). */
    uint32_t opt = (EDMA3_TCC_SPI_TX << 24) |      /* EVENTNO = channel 16 */
                   (EDMA3_TCC_SPI_TX << 12) |      /* TCC = 16 (matches EVENTNO) */
                   (0u << 8) |                      /* FWID = 0 (8-bit) */
                   (0u << 3) |                      /* STANDBY_PD = 0 */
                   (0u << 2) |                      /* SYNCDIM = 0 (A-sync) */
                   (0u << 1) |                      /* DAM = 0 (dst = constant) */
                   (1u << 0);                       /* SAM = 1 (src = increment) */

    uint32_t dst = SPI_BASE + MCSPI_TX(SPI_CH);     /* fixed dst = McSPI TX register */
    uint32_t abCnt = ((byteCount & 0xFFFFu) << 16) | (1u & 0xFFFFu);  /* ACNT | BCNT=1 */
    uint32_t srcDstBIdx = (0 << 16) | 1;            /* srcBIdx=1, dstBIdx=0 */
    uint32_t linkBcntrld = (0u << 16) | (byteCount & 0xFFFFu);        /* no link | BCNTRELOAD */
    uint32_t srcDstCIdx_ccnt = (0u << 16) | (0u << 0);                /* dstCIdx=0, srcCIdx=0 */

    /* Write PaRAM registers (macros return offset from PaRAM base, add EDMA3_CC_BASE). */
    EdmaRegWrite(EDMA3_CC_BASE + EDMA3CC_OPT(ch), opt);
    EdmaRegWrite(EDMA3_CC_BASE + EDMA3CC_SRC(ch), (uint32_t)src);
    EdmaRegWrite(EDMA3_CC_BASE + EDMA3CC_A_B_CNT(ch), abCnt);
    EdmaRegWrite(EDMA3_CC_BASE + EDMA3CC_DST(ch), dst);
    EdmaRegWrite(EDMA3_CC_BASE + EDMA3CC_SRC_DST_BIDX(ch), srcDstBIdx);
    EdmaRegWrite(EDMA3_CC_BASE + EDMA3CC_LINK_BCNTRLD(ch), linkBcntrld);
    EdmaRegWrite(EDMA3_CC_BASE + EDMA3CC_SRC_DST_CIDX(ch), srcDstCIdx_ccnt);

    /* --- Trigger the transfer --- */
    /* 1. Assert CS */
    McSPICSAssert(SPI_BASE, SPI_CH);

    /* 2. Enable DMA mode on McSPI (TX event) */
    McSPIDMAEnable(SPI_BASE, MCSPI_DMA_TX_EVENT, SPI_CH);

    /* 3. Wait for McSPI to be idle (serial shift not in progress). */
    int tTimeout = 100000;
    while ((HWREG(SPI_BASE + MCSPI_CHSTAT(SPI_CH)) & MCSPI_CH0STAT_TXS) && --tTimeout) {
        /* Wait for TX serial shift to finish before starting DMA transfer. */
    }

    /* 4. Enable DMA event via S_EESR (per-DMA-channel enable for AM335x).
     * Use |= to set only bit 16, leave other bits untouched. */
    HWREG(EDMA3_CC_BASE + EDMA3_SHD0_S_EESR) |= (1u << ch);

    /* 5. Wait for DMA to finish (poll IPR for TCC match). */
    int timeout = 2000000;
    while (!((EdmaRegRead(EDMA3_CC_BASE + EDMA3CC_IPR) >> ch) & 1) && --timeout) {
    }

    /* 6. Disable DMA channel via S_EECR (use |= to clear only bit 16). */
    HWREG(EDMA3_CC_BASE + EDMA3_SHD0_S_EECR) |= (1u << ch);

    /* 7. Clear IPR/ICR flag */
    EdmaRegWrite(EDMA3_CC_BASE + EDMA3CC_ICR, 1u << ch);

    /* 8. Wait for McSPI to finish transmitting all data (EOT = End of Transfer).
     * Without EOFTX=1, TXS only means TX shift register drained.
     * With EOFTX=1 and FIFO mode, EOT = FIFO completely empty. */
    int eotTimeout = 200000;
    while (!((McSPIChannelStatusGet(SPI_BASE, SPI_CH)) & MCSPI_CH0STAT_EOT) && --eotTimeout) {
    }

    /* 9. Disable McSPI DMA */
    McSPIDMADisable(SPI_BASE, MCSPI_DMA_TX_EVENT, SPI_CH);

    /* 10. Deassert CS */
    McSPICSDeAssert(SPI_BASE, SPI_CH);

    s_dmaDone = 1;
}

void Spi0TxBufferFast(const uint8_t *buf, uint32_t len)
{
    if (len == 0) return;

    McSPICSAssert(SPI_BASE, SPI_CH);
    GPIOPinWrite(DEBUG_LED_GPIO_BASE, DEBUG_LED_PIN, GPIO_PIN_HIGH);  /* PROBE: SPI TX active */

    /* Fill the 256-byte TX FIFO aggressively, then top up whenever
     * TXFFF de-asserts (FIFO ≤ 64 bytes = AFL threshold).  With the
     * 192-byte refill window the SPI shift engine never starves:
     * the CPU fills ahead of the drain rate. */

    uint32_t i = 0;
    while (i < len)
    {
        /* Fill the FIFO completely in one shot (up to 192 bytes). */
        uint32_t burst = (len - i > 192u) ? 192u : (len - i);
        {
            uint32_t j;
            for (j = 0; j < burst; j++)
            {
                HWREG(SPI_BASE + MCSPI_TX(SPI_CH)) = buf[i + j];
            }
        }
        i += burst;

        /* If more data remains, wait for the FIFO to drain below the
         * AFL threshold (64 bytes).  TXFFF = 0 → ≥ 192 slots free. */
        if (i < len)
        {
            int timeout = 100000;
            while ((McSPIChannelStatusGet(SPI_BASE, SPI_CH)) &
                   MCSPI_CH0STAT_TXFFF && --timeout) { /* spin */ }
        }
    }

    /* Wait for EOT — shift register + FIFO completely empty. */
    {
        int timeout = 500000;
        while (!((McSPIChannelStatusGet(SPI_BASE, SPI_CH)) &
                 MCSPI_CH0STAT_EOT) && --timeout) { /* spin */ }
    }

    /* Drain RX register to clear any stray EOT/TXS latches. */
    (void)McSPIReceiveData(SPI_BASE, SPI_CH);

    McSPICSDeAssert(SPI_BASE, SPI_CH);
    GPIOPinWrite(DEBUG_LED_GPIO_BASE, DEBUG_LED_PIN, GPIO_PIN_LOW);   /* PROBE: SPI TX done */
}
