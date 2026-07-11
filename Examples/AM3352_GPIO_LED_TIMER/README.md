# AM3352_GPIO_LED_TIMER

Template proyek StarterWare untuk **AM3352** (Cortex-A8) yang mengedipkan LED pada **GPIO1[23]**. Dipakai sebagai basis untuk eksperimen GPIO / peripheral lain di board berbasis AM3352 (misalnya **Antminer L3+** — PCB & form-factor identik dengan BeagleBone, tanpa PRU).

Output ELF di-load langsung ke DDR `0x80000000` via debugger (JTAG / SEGGER J-Link), lalu dieksekusi oleh entry `Entry` di `AM335x.cmd`.

---

## Konteks Hardware

| Item | Value |
|---|---|
| SoC | TI AM3352 (Cortex-A8, little-endian, ARMv7-A) |
| Board | Antminer L3+ (PCB & form-factor = BeagleBone, tanpa PRU) |
| Pin LED | GPIO1[23] |
| Debug probe | SEGGER J-Link |
| Toolchain | TI v20.2.7.LTS (CGT ARM) |

Library StarterWare yang dipakai (sudah prebuilt, **read-only**):

```
C:\ti\AM335X_StarterWare_02_00_01_01\include
C:\ti\AM335X_StarterWare_02_00_01_01\include\hw
C:\ti\AM335X_StarterWare_02_00_01_01\include\armv7a
C:\ti\AM335X_StarterWare_02_00_01_01\include\armv7a\am335x
C:\ti\AM335X_StarterWare_02_00_01_01\binary\armv7a\cgt_ccs\am335x\drivers\Release\drivers.lib
C:\ti\AM335X_StarterWare_02_00_01_01\binary\armv7a\cgt_ccs\am335x\system_config\Release\system.lib
C:\ti\AM335X_StarterWare_02_00_01_01\binary\armv7a\cgt_ccs\am335x\beaglebone\platform\Release\platform.lib
C:\ti\AM335X_StarterWare_02_00_01_01\binary\armv7a\cgt_ccs\utils\Release\utils.lib
```

Folder StarterWare ini **tidak** diubah / dicopy ke proyek — header di-include via path di `.cproject`, library di-link langsung dari binary prebuilt.

---

## Struktur File Proyek

```
AM3352_GPIO_LED_TIMER/
├── .ccsproject             ← toolchain, device, linker cmd, origin path
├── .cproject               ← full build config (Debug + Release, TMS470_20.2)
├── .project                ← Eclipse project metadata
├── .settings/
│   ├── org.eclipse.cdt.codan.core.prefs
│   └── org.eclipse.cdt.debug.core.prefs
├── AM335x.cmd              ← TI CGT linker command file
├── main.c                  ← application source
├── README.md               ← file ini
└── Debug/                  ← build output (auto-generated oleh CCS)
```

Proyek ini **tidak** memerlukan folder `.launches/` — launch config debug dibuat via **Run → Debug Configurations** di CCS.

---

## Konfigurasi `.cproject` (sudah built-in)

Proyek ini dibuat via **New CCS Project → Empty Project (with main.c)** di CCS 12.8, lalu `.cproject` di-custom ke:

- **Device**: `Cortex A.AM3352` (toolchain `TMS470_20.2`, CGT ARM 20.2.7.LTS)
- **Compiler flags**: `--silicon_version=7A8`, `--code_state=32`, `--abi=eabi`, `--little_endian`, `--diag_warning=225`, `--display_error_number`
- **Predefine**: `am3352`
- **Linker command file**: `AM335x.cmd` (lihat di bawah)
- **Stack/heap**: `0x800` / `0x800`
- **RTS**: `libc.a`

`.cproject` **tidak** menambahkan StarterWare ke include path secara default. Agar `main.c` bisa `#include "soc_AM335x.h"`, `beaglebone.h`, `gpio_v2.h`, dan agar `GPIO1ModuleClkConfig()` / `GPIO1Pin23PinMuxSetup()` / `GPIOModuleEnable()` dll ter-resolve saat link, tambahkan manual via UI CCS:

**Properties → Build → ARM Compiler → Include Options** (path include StarterWare):

```
C:/ti/AM335X_StarterWare_02_00_01_01/include
C:/ti/AM335X_StarterWare_02_00_01_01/include/hw
C:/ti/AM335X_StarterWare_02_00_01_01/include/armv7a
C:/ti/AM335X_StarterWare_02_00_01_01/include/armv7a/am335x
```

**Properties → Build → ARM Linker → File Search Path → Libraries** (urutan penting):

```
drivers.lib
system.lib
platform.lib
utils.lib
libc.a
```

dan **Library search path**:

```
C:/ti/AM335X_StarterWare_02_00_01_01/binary/armv7a/cgt_ccs/am335x/drivers/Release
C:/ti/AM335X_StarterWare_02_00_01_01/binary/armv7a/cgt_ccs/am335x/system_config/Release
C:/ti/AM335X_StarterWare_02_00_01_01/binary/armv7a/cgt_ccs/am335x/beaglebone/platform/Release
C:/ti/AM335X_StarterWare_02_00_01_01/binary/armv7a/cgt_ccs/utils/Release
${CG_TOOL_ROOT}/lib
${CG_TOOL_ROOT}/include
```

> **Catatan**: proyek StarterWare asli (mis. `AM3352_GPIO_LED` di workspace yang sama) me-mount path StarterWare langsung di `.cproject` lewat `postbuildStep` dan `-l` hardcoded untuk build `.bin`/`.ti.bin`. Proyek ini lebih minimal — konfigurasi ditambah manual lewat UI Properties CCS, sehingga `.cproject` tidak bocor path absolut StarterWare ke VCS.

---

## Linker Command File (`AM335x.cmd`)

```
-stack  0x0008
-heap   0x2000
-e Entry
--diag_suppress=10063

MEMORY
{
        DDR_MEM        : org = 0x80000000  len = 0x7FFFFFF
}

SECTIONS
{
    .text:Entry : load > 0x80000000
    .text    : load > DDR_MEM
    .data    : load > DDR_MEM
    .bss     : load > DDR_MEM
                    RUN_START(bss_start)
                    RUN_END(bss_end)
    .const   : load > DDR_MEM
    .stack   : load > 0x87FFFFF0
}
```

Stack/heap di `.cproject` di-override ke `0x800` / `0x800`, sehingga flag `-stack` / `-heap` di `.cmd` sebenarnya tidak berlaku — linker pakai yang dari `.cproject`. Tetap dipertahankan agar `.cmd` tetap portable ke proyek StarterWare lain.

---

## Pola `main.c` untuk GPIO Blink

Header StarterWare yang dipakai:

```c
#include "soc_AM335x.h"
#include "beaglebone.h"
#include "gpio_v2.h"
#include "dmtimer.h"
```

Skema GPIO output + blink dengan **inline busy-wait on DMTimer7** (24 MHz, prescaler disabled → counter naik 24.000 kali per milidetik):

```c
#define GPIO_INSTANCE_ADDRESS    (SOC_GPIO_1_REGS)
#define GPIO_INSTANCE_PIN_NUMBER (23)
#define TIMER_1MS_COUNT          (24000U)
#define BLINK_PERIOD_MS          (500U)

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
    /* GPIO1[23] setup */
    GPIO1ModuleClkConfig();         /* nyalakan clock GPIO1 di PRCM */
    GPIO1Pin23PinMuxSetup();        /* set pin-mux GPIO1[23] ke mode GPIO */

    GPIOModuleEnable(GPIO_INSTANCE_ADDRESS);
    GPIOModuleReset(GPIO_INSTANCE_ADDRESS);
    GPIODirModeSet(GPIO_INSTANCE_ADDRESS,
                   GPIO_INSTANCE_PIN_NUMBER,
                   GPIO_DIR_OUTPUT);

    /* DMTimer7 clock enable (24 MHz, no prescaler) */
    DMTimer7ModuleClkConfig();
    DMTimerModeConfigure(SOC_DMTIMER_7_REGS, DMTIMER_ONESHOT_NOCMP_ENABLE);

    while (1) {
        GPIOPinWrite(GPIO_INSTANCE_ADDRESS, GPIO_INSTANCE_PIN_NUMBER, GPIO_PIN_HIGH);
        delay_ms_poll(BLINK_PERIOD_MS);

        GPIOPinWrite(GPIO_INSTANCE_ADDRESS, GPIO_INSTANCE_PIN_NUMBER, GPIO_PIN_LOW);
        delay_ms_poll(BLINK_PERIOD_MS);
    }
}
```

`DMTimer7ModuleClkConfig()` + `DMTimerModeConfigure(..., DMTIMER_ONESHOT_NOCMP_ENABLE)` memberi counter DMTimer7 clock 24 MHz tanpa prescaler dan tanpa auto-reload, sehingga setiap loop `DMTimerCounterGet() < 24000` menunggu tepat 1 ms.

> **Catatan**: `DMTimer7ModuleClkConfig()` di sini dipanggil langsung (bukan via `DelayTimerSetup()` di `platform.lib`) supaya blink-loop tidak tergantung ke jalur IRQ dari StarterWare — semua timing terjadi lewat counter register read. `delay()`/`DelayTimerSetup()` dari StarterWare tersedia di `utils.lib`/`platform.lib` dan tetap ter-link, tapi tidak dipanggil oleh `main.c`.

> **Catatan Antminer L3+**: `beaglebone.h` dipakai karena board ini PCB-identik dengan BeagleBone — pin-mux `GPIO1Pin23PinMuxSetup()` dan clock-enable `GPIO1ModuleClkConfig()` keduanya valid untuk L3+. Kalau routing GPIO ternyata beda, ganti ke `GPIO1PinXxxPinMuxSetup()` yang sesuai — semua prototype ada di `include/armv7a/am335x/beaglebone.h`.

---

## Build

1. Pilih konfigurasi **Debug** atau **Release** di toolbar CCS.
2. **Project → Build All** (`Ctrl+B`).
3. Output `.out` muncul di `Debug/AM3352_GPIO_LED_TIMER.out` (atau `Release/...`).

---

## Debug via SEGGER J-Link

1. Hubungkan J-Link ke header JTAG board.
2. **Run → Debug Configurations → Code Composer Studio → New launch**.
3. **Main tab**:
   - Project: `AM3352_GPIO_LED_TIMER`
   - C/C++ application: `${workspace_loc:/AM3352_GPIO_LED_TIMER/Debug/AM3352_GPIO_LED_TIMER.out}`
4. **Apply → Debug**. CCS load `.out` ke `0x80000000` (DDR) dan PC mulai dari `Entry`.
