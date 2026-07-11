# AM3352_GPIO_LED_SEQUENCE

Contoh proyek StarterWare untuk **AM3352** (Cortex-A8) yang menjalankan **animasi sequence** pada **empat LED onboard** (D2, D3, D4, D5) board **Antminer L3+** (PCB-identik BeagleBone Black). Berasal dari template `AM3352_GPIO_LED_TIMER` (single-LED blink via `delay.h`) yang sudah divalidasi; dikembangkan dengan menambah tiga pin GPIO tambahan dan mengubah loop `while` jadi running-light.

## Pola Animasi

Running light urutan **D2 → D3 → D4 → D5 → D2 → ...** dengan satu LED nyala pada satu waktu, step tiap **200 ms**. Loop forever — `STEP_PERIOD_MS` bisa diubah konstan-nya untuk mengatur kecepatan.

```
        ┌───────┐
        │   .   │   . = LED nyala, # = LED padam
   D2 ── .   │   │
   D3 ── #   ──.
   D4 ── #   ──.
   D5 ── #   ──.
        └───────┘
        (frame ke-N)
```

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

## Mapping LED Onboard → Pin AM3352

Antminer L3+ PCB-identik dengan BeagleBone Black, sehingga mapping LED onboard-nya mengikuti SRM BeagleBone:

| LED | Fungsi board | Pin AM3352 | Conf Register | MUXMODE | StarterWare helper |
|---|---|---|---|---|---|
| D2 | USR0 (heartbeat) | GPIO1[21] | `CONTROL_CONF_GPMC_A(5)` | 7 | ❌ (manual) |
| D3 | USR1 (mmc0 activity) | GPIO1[22] | `CONTROL_CONF_GPMC_A(6)` | 7 | ❌ (manual) |
| D4 | USR2 | GPIO1[23] | `CONTROL_CONF_GPMC_A(7)` | 7 | ✅ `GPIO1Pin23PinMuxSetup()` |
| D5 | USR3 | GPIO1[24] | `CONTROL_CONF_GPMC_A(8)` | 7 | ❌ (manual) |

StarterWare hanya menyediakan helper mux untuk D4. Tiga pin lain di-setup manual dengan `HWREG(...) = CONTROL_CONF_MUXMODE(7)` — pola yang **identik** dengan implementasi `GPIO1Pin23PinMuxSetup()` di `platform/beaglebone/gpio.c:62`, jadi perilaku sama terjamin.

> **Verifikasi**: `CONTROL_CONF_GPMC_A(n)` ada di `include/hw/hw_control_AM335x.h:180` dengan formula `0x840 + (n * 4)`. `CONTROL_CONF_MUXMODE(n)` di baris 332 = `(n)`. Untuk `n=5..8`, mux mode 7 artinya sinyal `gpio1_21`..`gpio1_24` sesuai AM335x TRM Table 4-1.

---

## Pola `main.c` untuk LED Sequence 4-LED

Header StarterWare yang dipakai:

```c
#include "soc_AM335x.h"
#include "beaglebone.h"
#include "gpio_v2.h"
#include "interrupt.h"
#include "delay.h"
#include "hw_control_AM335x.h"   /* CONTROL_CONF_GPMC_A(), CONTROL_CONF_MUXMODE() */
#include "hw_cm_wkup.h"          /* aman ditarik; GPIO1ModuleClkConfig() pakai CM_WKUP_* */
```

Catatan: StarterWare **tidak** menarik `hw_*.h` lewat `soc_AM335x.h`. Setiap header hardware harus di-include eksplisit, kalau tidak linker akan error `undefined symbol`. Lihat error build yang sebelumnya muncul di `CONTROL_CONF_GPMC_A` dan `CONTROL_CONF_MUXMODE` — keduanya butuh `hw_control_AM335x.h`.

`delay.h` (dari StarterWare `utils/delay.c` + `platform/beaglebone/sysdelay.c`) memberi `delay(unsigned int milliSec)` yang menunggu overflow DMTimer7 lewat IRQ — **bukan** busy-wait. Dua baris pertama `main()` wajib untuk jalur IRQ:

```c
IntAINTCInit();           /* reset AINTC, seed fnRAMVectors[], priority threshold */
IntMasterIRQEnable();     /* clear I-bit di CPSR supaya CPU bisa terima IRQ */
```

Skema lengkap 4-LED sequence dengan `delay.h`:

```c
#define GPIO_INSTANCE_ADDRESS    (SOC_GPIO_1_REGS)

#define LED_D2_PIN               (21U)   /* GPMC_A5 */
#define LED_D3_PIN               (22U)   /* GPMC_A6 */
#define LED_D4_PIN               (23U)   /* GPMC_A7 */
#define LED_D5_PIN               (24U)   /* GPMC_A8 */

#define STEP_PERIOD_MS           (200U)

static const unsigned int gLedPins[] = {
    LED_D2_PIN, LED_D3_PIN, LED_D4_PIN, LED_D5_PIN,
};

#define LED_COUNT (sizeof(gLedPins) / sizeof(gLedPins[0]))

int main(void)
{
    unsigned int step = 0;

    IntAINTCInit();
    IntMasterIRQEnable();

    /* Clock GPIO1 di PRCM (sama untuk semua GPIO1 pin). */
    GPIO1ModuleClkConfig();

    /* Pin-mux keempat LED ke mode GPIO (muxmode 7). D4 pakai helper bawaan,
       sisanya di-tulis manual dengan HWREG persis seperti helper. */
    GPIO1Pin23PinMuxSetup();
    HWREG(SOC_CONTROL_REGS + CONTROL_CONF_GPMC_A(5)) = CONTROL_CONF_MUXMODE(7);
    HWREG(SOC_CONTROL_REGS + CONTROL_CONF_GPMC_A(6)) = CONTROL_CONF_MUXMODE(7);
    HWREG(SOC_CONTROL_REGS + CONTROL_CONF_GPMC_A(8)) = CONTROL_CONF_MUXMODE(7);

    GPIOModuleEnable(GPIO_INSTANCE_ADDRESS);
    GPIOModuleReset(GPIO_INSTANCE_ADDRESS);

    for (step = 0; step < LED_COUNT; step++) {
        GPIODirModeSet(GPIO_INSTANCE_ADDRESS, gLedPins[step], GPIO_DIR_OUTPUT);
        GPIOPinWrite(GPIO_INSTANCE_ADDRESS, gLedPins[step], GPIO_PIN_LOW);
    }

    DelayTimerSetup();

    /* Running light: padamkan semua, nyalakan satu per satu. */
    while (1) {
        for (step = 0; step < LED_COUNT; step++) {
            unsigned int other;
            for (other = 0; other < LED_COUNT; other++) {
                GPIOPinWrite(GPIO_INSTANCE_ADDRESS, gLedPins[other], GPIO_PIN_LOW);
            }
            GPIOPinWrite(GPIO_INSTANCE_ADDRESS, gLedPins[step], GPIO_PIN_HIGH);
            delay(STEP_PERIOD_MS);
        }
    }
}
```

> **Catatan IRQ path**: `Sysdelay()` di `sysdelay.c:131` busy-wait di `while(FALSE == flagIsr)` sampai `DMTimerIsr()` set flag. Tanpa `IntAINTCInit()` + `IntMasterIRQEnable()`, `flagIsr` tidak pernah di-set dan CPU stuck di loop itu. Pola di atas mengikuti `examples/evmAM335x/dmtimer/dmtimerCounter.c` (StarterWare reference).
>
> **Catatan Antminer L3+**: mapping pin di atas mengasumsikan board PCB-identik BeagleBone Black. Jika ternyata routing-nya berbeda, ganti konstanta `LED_D*_PIN` dan offset `CONTROL_CONF_GPMC_A(n)` sesuai datasheet board. Semua prototype ada di `include/armv7a/am335x/beaglebone.h`.

---

## Variasi yang mudah

- **Knight Rider / bouncing**: tambah variabel `dir` (`+1` / `-1`) di outer loop, balik arah saat `step == 0` atau `step == LED_COUNT-1`.
- **Breathing (PWM)**: ganti ke DMTimer PWM mode (lihat StarterWare `examples/evmAM335x/dmtimer/dmtimerPwm.c`) — perlu pin yang support PWM, GPIO1[21..24] perlu remux ke mode ehrpwm.
- **Step lebih cepat/lambat**: ubah `STEP_PERIOD_MS` (mis. `100U` untuk 10 fps, `500U` untuk 2 fps).

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
4. **Target tab**: klik **Add...** di "Target Configuration" dan buat target config baru (SEGGER J-Link + AM3352). Simpan `.ccxml` di lokasi yang Anda inginkan.
5. **Apply → Debug**. CCS load `.out` ke `0x80000000` (DDR) dan PC mulai dari `Entry`.
