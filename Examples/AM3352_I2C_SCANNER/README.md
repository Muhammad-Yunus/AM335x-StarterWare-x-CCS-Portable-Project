# AM3352_I2C_SCANNER

Project StarterWare portable yang men-scan bus **I2C1** di SoC AM3352 dan menampilkan hasilnya lewat **UART0** dalam format grid `16×16` ala `i2cdetect -y 1` Linux. Board target-nya **Antminer L3+** — sama PCB dan form-factor dengan BeagleBone Black (hanya tanpa PRU), jadi semua pad-mux dan clock-enable di `beaglebone.h` valid.

Source: `main.c` &middot; Linker: `AM335x.cmd` &middot; Output: ELF di-load langsung ke DDR 0x80000000 via debugger (JTAG/SEGGER J-Link).

---

## Konteks Hardware

| Item | Value |
|---|---|
| SoC | TI AM3352 (Cortex-A8, little-endian, ARMv7-A) |
| Board | Antminer L3+ (PCB & form-factor = BeagleBone, tanpa PRU) |
| Bus yang di-scan | I2C1 |
| Pin SCL | P9_17 (pad `SPI0_D1`, MUXMODE=2) |
| Pin SDA | P9_18 (pad `SPI0_CS0`, MUXMODE=2) |
| Kecepatan | 100 kHz (standard mode) |
| UART output | UART0 @ 115200 8N1 (P9_11/P9_13 TX/RX) |
| Pull-up eksternal | **10 kΩ SCL→3V3 dan SDA→3V3** — wajib |
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

## ⚠ Pull-Up Resistor Wajib

SoC AM3352 memang sudah punya internal pull-up ~50 kΩ di pad I2C1, tapi **`I2CPinMuxSetup(1)` di StarterWare tidak mengaktifkan bit `PUDEN`** di pin-mux config — jadi pull-up internal-nya idle. Untuk I2C di 100 kHz, 50 kΩ sudah di borderline (rise-time-nya terlalu lambat), dan kalau dipakai external pull-up tidak ada, **slave tidak akan pernah ter-ACK dengan benar**:

- Probe ke address yang ada slave-nya diam-diam miss (tidak ada "Device found" line).
- Probe setelah address slave hang di `while(I2CMasterBusBusy)` selamanya.

**Solusi**: solder dua resistor **10 kΩ** dari:
- P9_17 (SCL) → header VDD_3V3 (P9_3 atau P9_4)
- P9_18 (SDA) → header VDD_3V3 (P9_3 atau P9_4)

Verifikasi cepat dengan multimeter: saat bus idle, SDA dan SCL harus berada di sekitar 3.3 V (bukan floating 1.x V atau 0 V).

> 💡 **Device nggak muncul di scan?** Jika pin I2C-nya sudah benar (SCL = P9_17, SDA = P9_18, sudah di-pinmux via `I2CPinMuxSetup(1)`, firmware bisa cetak SCL 100 kHz di scope), **coba dulu pull-up 10 kΩ ke VDD_3V3 di kedua line** seperti di section [Pull-Up Resistor Wajib](#-pull-up-resistor-wajib). Itu 95% solusinya — bukan bug firmware.

---

## Contoh Output UART

Probe loop hanya-baca — tidak ada interaksi host. Berikut contoh dengan SSD1306 128×32 OLED di address 0x3C:

```
+---------------------------------------------+
|   AM3352  I2C1  Bus  Scanner                |
|   SCL = P9_17   SDA = P9_18   100 kHz       |
+---------------------------------------------+

       0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
00:
10:    -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
20:    -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
30:    -- -- -- -- -- -- -- -- -- -- -- -- 3c -- -- --
40:    -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
50:    -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
60:    -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
70:    -- -- -- -- -- -- -- --

1 device(s) detected on I2C1.
```

Format persis `i2cdetect -y 1`:
- `--` = tidak ada ACK (no device).
- `XX` = slave ACK di address ini.
- kosong (3 spasi) = reserved range (0x00..0x02 general call, 0x78..0x7F 10-bit addrs) — tidak di-probe.

---

## Cara Membuat Project Seperti Ini Dari Nol (Manual di CCS)

### 1. New CCS Project

Buka **File → New → CCS Project**, lalu isi dalam satu halaman dialog:

1. **Project name**: `AM3352_I2C_SCANNER` (atau apa pun — nama ini jadi nama ELF output).
2. **Location**: biarkan default (akan dibuat di dalam workspace).
3. **Connection**: pilih **SEGGER J-Link Emulator** (sesuai probe yang dipakai). Asumsinya, sebuah shared global target config **sudah ada** — biasanya `AM3352_bb_shared_target_configuration.ccxml` yang tersimpan di user-level config (bukan di dalam folder project), sudah dikonfigurasi untuk probe ini + Cortex-A8 core + device AM3352. Field target configuration cukup dikosongkan atau memilihnya dari dropdown referensi ke config global tersebut. **JANGAN** centang *"Create new target configuration"* — kita pakai yang global supaya tidak duplikasi config.
4. **Project type**: `Cortex A.AM3352` (atau `AM335x` generic).
5. Di panel **Project templates and examples**, pilih:
   ```
   ☑ Empty Project (with main.c)
   ```
   &nbsp;&nbsp;→ otomatis generate `main.c` kosong.
6. **Toolchain**: **TI v20.2.7.LTS** (atau versi LTS CGT ARM yang ter-install — bukan GNU ARM).
7. Klik **Finish**.

### 2. Konfigurasi Include Path StarterWare

**Properties → Build → ARM Compiler → Include Options**, tambahkan 5 path ini (via `-I`):

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

### 3. Konfigurasi Linker Command File

**Properties → General → Linker command file**:
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

### 4. Link Library StarterWare

**Properties → Build → ARM Linker → File Search Path**, tambahkan ke **Libraries** (urutan penting):

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

### 5. Tulis `main.c`

Replace isi `main.c` yang kosong dengan kode I2C scanner di repo ini. Skeleton singkat:

```c
#include "consoleUtils.h"
#include "uart_irda_cir.h"
#include "hw_types.h"
#include "hw_control_AM335x.h"
#include "hw_cm_per.h"
#include "soc_AM335x.h"
#include "interrupt.h"
#include "beaglebone.h"
#include "hsi2c.h"

/* UART0 115200 8N1 + I2C1 100 kHz master — sama dengan SetupI2C1Master
** di main.c. Probe loop pakai I2C1ProbeAddress(addr) yang polling-only
** (no ISR): bus-busy → ARDY → STOP → STOP_CONDITION, dengan soft-reset
** recovery kalau slave hold SDA low. */

int main(void)
{
    /* UART bring-up ... */
    /* Banner ASCII box ... */

    SetupI2C1Master();

    for(addr = 0x03; addr <= 0x77; addr++) {
        if(I2C1ProbeAddress(addr)) {
            foundMap[addr >> 3] |= (1u << (addr & 0x7));
            foundCount++;
        }
    }

    /* Print 16x16 grid ala i2cdetect -y 1 ... */

    while(1);
}
```

Versi lengkap ada di `main.c` (~360 baris).

### 6. Build

1. Pilih **Release** (atau Debug) di toolbar.
2. **Project → Build All** (`Ctrl+B`).
3. Output `.out` muncul di `${workspace_loc:/AM3352_I2C_SCANNER/${ConfigName}/AM3352_I2C_SCANNER.out}` (yaitu `Debug/` atau `Release/` di dalam folder project).

### 7. Debug / Flash via J-Link

1. Hubungkan J-Link ke header JTAG board.
2. Pilih target configuration yang sesuai (predefined, mis. `am3352_bb_shared_target_configuration.ccxml`).
3. **Run → Debug** (`F11`) — CCS load `.out` ke 0x80000000 (DDR) dan PC mulai dari `Entry`.
4. Buka terminal serial di 115200 8N1 ke UART0 (P9_11/P9_13 TX/RX), atau ke header debug USB-to-serial kalau board menyediakan.

---

## Struktur File Final Project

```
AM3352_I2C_SCANNER/
├── .ccsproject         ← toolchain, device, linker cmd, origin path
├── .cproject           ← full build config (Debug + Release)
├── .project            ← Eclipse project metadata + linked build artifacts
├── .launches/
│   └── AM3352_I2C_SCANNER.launch   ← debug launch config
├── .settings/
│   ├── org.eclipse.cdt.codan.core.prefs
│   ├── org.eclipse.cdt.debug.core.prefs
│   └── org.eclipse.core.resources.prefs
├── targetConfigs/
│   ├── AM3352.ccxml            ← target configuration (opsional, kalau tidak pakai global)
│   └── readme.txt
├── AM335x.cmd          ← TI CGT linker command file
├── README.md           ← you are here
└── main.c              ← application source — UART bring-up + I2C probe loop
```

Tidak perlu ada `targetConfigs/` sendiri kalau pakai predefined ccxml global — CCS akan pakai yang di-set di **Window → Preferences → Code Composer Studio → Debug → Target Configurations**.

---

## Catatan Teknis: Kenapa Probe-nya Polling, Bukan ISR

AM335x HSI2C punya ISR-driven reference di `hsi2cEeprom.c` (StarterWare), tapi itu untuk transfer multi-byte full-duplex. Untuk 1-byte address probe, polling lebih sederhana dan deterministik:

1. **Tunggu bus-busy** setelah `I2CMasterStart()` — controller owns SDA/SCL.
2. **Tunggu ARDY** (`I2C_INT_ADRR_READY_ACESS`) — address byte + ACK/NACK sudah resolved.
3. **Putusan ACK**: ARDY set, `I2C_INT_NO_ACK` clear, `I2C_INT_ARBITRATION_LOST` clear → present=1.
4. **STOP + tunggu STOP_CONDITION** — supaya bus idle bersih sebelum probe berikutnya.
5. **Bounded wait + soft-reset** di setiap loop — kalau slave hold SDA low (SDA tidak naik karena pull-up lemah, atau slave tidak release), `I2CSoftReset` adalah satu-satunya cara pulih tanpa GPIO bit-bang.

Polling `I2C_INT_TRANSMIT_READY` (XRDY) **akan** ngasih false ACKs contiguous di address rendah (~0x05..0x21) karena XRDY flip saat address byte masih di-clock out. Selalu tunggu ARDY untuk probe.
