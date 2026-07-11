# AM3352_GPIO_LED

Project StarterWare portable yang mengedipkan LED pada pin **GPIO1[23]** dari SoC AM3352. Project ini adalah *template* minimal yang bisa dipakai ulang untuk eksperimen GPIO / peripheral lain di board yang berbasis AM3352 (misalnya **Antminer L3+** — board ini share form-factor dan PCB routing dengan BeagleBone, hanya tanpa PRU subsystem).

Source: `main.c` &middot; Linker: `AM335x.cmd` &middot; Output: ELF di-load langsung ke DDR 0x80000000 via debugger (JTAG/SEGGER J-Link).

---

## Konteks Hardware

| Item | Value |
|---|---|
| SoC | TI AM3352 (Cortex-A8, little-endian, ARMv7-A) |
| Board | Antminer L3+ (PCB & form-factor = BeagleBone, tanpa PRU) |
| Pin LED | GPIO1[23] |
| Debug probe | SEGGER J-Link |

Library StarterWare yang dipakai sudah prebuilt di:
```
C:\ti\AM335X_StarterWare_02_00_01_01\binary\armv7a\cgt_ccs\am335x\drivers\Release\drivers.lib
C:\ti\AM335X_StarterWare_02_00_01_01\binary\armv7a\cgt_ccs\am335x\system_config\Release\system.lib
C:\ti\AM335X_StarterWare_02_00_01_01\binary\armv7a\cgt_ccs\am335x\beaglebone\platform\Release\platform.lib
C:\ti\AM335X_StarterWare_02_00_01_01\binary\armv7a\cgt_ccs\utils\Release\utils.lib
```
Folder StarterWare ini dipakai **read-only** — tidak ada file StarterWare yang diubah atau dicopy ke project.

---

## Cara Membuat Project Seperti Ini Dari Nol (Manual di CCS)

### 1. New CCS Project

Buka **File &rarr; New &rarr; CCS Project**, lalu isi dalam satu halaman dialog:

1. **Project name**: `AM3352_GPIO_LED` (atau apa pun — nama ini jadi nama ELF output).
2. **Location**: biarkan default (akan dibuat di dalam workspace).
3. **Connection**: pilih **SEGGER J-Link Emulator** (sesuai probe yang dipakai). Asumsinya, sebuah shared global target config **sudah ada** — biasanya `AM3352_bb_shared_target_configuration.ccxml` yang tersimpan di user-level config (bukan di dalam folder project), sudah dikonfigurasi untuk probe ini + Cortex-A8 core + device AM3352. Field target configuration cukup dikosongkan atau memilihnya dari dropdown referensi ke config global tersebut. **JANGAN** centang *&ldquo;Create new target configuration&rdquo;* — kita pakai yang global supaya tidak duplikasi config.
4. **Project type**: `Cortex A.AM3352` (atau `AM335x` generic).
5. Di panel **Project templates and examples**, pilih:
   ```
   ☑ Empty Project (with main.c)
   ```
   &nbsp;&nbsp;→ otomatis generate `main.c` kosong.
6. **Toolchain**: **TI v20.2.7.LTS** (atau versi LTS CGT ARM yang ter-install — bukan GNU ARM).
7. Klik **Finish**.

### 3. Konfigurasi Include Path StarterWare

**Properties &rarr; Build &rarr; ARM Compiler &rarr; Include Options**, tambahkan 5 path ini (via `-I`):

```
C:/ti/AM335X_StarterWare_02_00_01_01/include
C:/ti/AM335X_StarterWare_02_00_01_01/include/hw
C:/ti/AM335X_StarterWare_02_00_01_01/include/armv7a
C:/ti/AM335X_StarterWare_02_00_01_01/include/armv7a/am335x
${CG_TOOL_ROOT}/include
```

Compiler flags tambahan yang disarankan (sama dengan example StarterWare):
- `--silicon_version=7A8`
- `--code_state=32`
- `--abi=eabi`
- `--little_endian`
- `--gcc` (untuk kompatibilitas konstruk GCC)
- `--diag_warning=225`
- `--display_error_number`

### 4. Konfigurasi Linker Command File

**Properties &rarr; General &rarr; Linker command file**:
- `AM335x.cmd` (file ini isinya lihat di bawah).

Karena starter project kasih `*.lds` (format GCC), **hapus** file `.lds` itu dan buat `AM335x.cmd` dengan isi:

```cmd
-stack  0x0008
-heap   0x2000
-e Entry
--diag_suppress=10063

MEMORY
{
    DDR_MEM : org = 0x80000000  len = 0x7FFFFFF
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

### 5. Link Library StarterWare

**Properties &rarr; Build &rarr; ARM Linker &rarr; File Search Path**, tambahkan ke **Libraries** (urutan penting):

```
libc.a
drivers.lib
system.lib
platform.lib
utils.lib
```

**Library search path** (`-i`):

```
${CG_TOOL_ROOT}/lib
${CG_TOOL_ROOT}/include
```

### 6. Tulis `main.c`

Replace isi `main.c` yang kosong dengan kode GPIO blink:

```c
#include "soc_AM335x.h"
#include "beaglebone.h"
#include "gpio_v2.h"

#define GPIO_INSTANCE_ADDRESS    (SOC_GPIO_1_REGS)
#define GPIO_INSTANCE_PIN_NUMBER (23)

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

    while(1) {
        GPIOPinWrite(GPIO_INSTANCE_ADDRESS, GPIO_INSTANCE_PIN_NUMBER, GPIO_PIN_HIGH);
        Delay(0x3FFFF);
        GPIOPinWrite(GPIO_INSTANCE_ADDRESS, GPIO_INSTANCE_PIN_NUMBER, GPIO_PIN_LOW);
        Delay(0x3FFFF);
    }
}

static void Delay(volatile unsigned int count) { while(count--); }
```

**Catatan Antminer L3+**: header `beaglebone.h` dipakai karena board ini PCB-identik dengan BeagleBone — pin-mux `GPIO1Pin23PinMuxSetup()` dan clock-enable `GPIO1ModuleClkConfig()` keduanya **valid** untuk L3+. Kalau ternyata board L3+ kamu *routing* GPIO-nya beda, ganti ke `GPIO1PinXxxPinMuxSetup()` yang sesuai — semua prototype ada di `include/armv7a/am335x/beaglebone.h`.

### 7. Build

1. Pilih **Release** (atau Debug) di toolbar.
2. **Project &rarr; Build All** (`Ctrl+B`).
3. Output `.out` muncul di `${workspace_loc:/AM3352_GPIO_LED/${ConfigName}/AM3352_GPIO_LED.out}` (yaitu `Debug/` atau `Release/` di dalam folder project).

### 8. Debug / Flash via J-Link

1. Hubungkan J-Link ke header JTAG board.
2. Pilih target configuration yang sesuai (predefined, mis. `am3352_bb_shared_target_configuration.ccxml`).
3. **Run &rarr; Debug** (`F11`) &mdash; CCS load `.out` ke 0x80000000 (DDR) dan PC mulai dari `Entry`.

---

## Struktur File Final Project

```
AM3352_GPIO_LED/
├── .ccsproject         ← toolchain, device, linker cmd, origin path
├── .cproject           ← full build config (Debug + Release)
├── .project            ← Eclipse project metadata + linked build artifacts
├── .launches/
│   └── AM3352_GPIO_LED.launch   ← debug launch config
├── .settings/
│   ├── org.eclipse.cdt.codan.core.prefs
│   ├── org.eclipse.cdt.debug.core.prefs
│   └── org.eclipse.core.resources.prefs
├── AM335x.cmd          ← TI CGT linker command file
└── main.c              ← application source
```

Tidak perlu ada `targetConfigs/` kalau pakai predefined ccxml global — CCS akan pakai yang di-set di **Window &rarr; Preferences &rarr; Code Composer Studio &rarr; Debug &rarr; Target Configurations**.