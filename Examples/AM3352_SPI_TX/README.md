# AM3352_SPI_TX

Project StarterWare portable yang **mentransmisikan satu byte (`0xAF`) secara terus-menerus melalui SPI0** dari SoC AM3352 ke pin header P9, sebagai **baseline demonstrasi TX SPI**. Dirancang agar setiap sinyal yang bergerak (CS, CLK, MOSI) bisa langsung diamati di osiloskop/logic-analyzer tanpa peripheral tambahan.

Tujuan project ini adalah menjadi titik awal (scaffold) untuk eksperimen SPI lain di board berbasis AM3352 (misalnya **Antminer L3+** — board ini share form-factor dan PCB routing dengan BeagleBone, hanya tanpa PRU subsystem). Setelah baseline ini terbukti hidup, project akan difork ke arah aplikasi tertentu (driver LCD, sensor, flash, dsb.) dengan folder project diganti manual.

Source: `main.c` · Linker: `AM335x.cmd` · Output: ELF di-load langsung ke DDR 0x80000000 via debugger (JTAG/SEGGER J-Link).

---

## Apa yang Dilakukan Program

Setiap iterasi loop forever:

1. **RST** (P8_19) di-pulse HIGH → LOW untuk menandai awal satu transfer (visualisasi di scope, tidak relevan secara protokol).
2. **DC** (P8_26) di-drive LOW (`0xAF` dikirim sebagai *command*, bukan *data* — berguna saat nanti dipakai ke ST7735).
3. **CS** (P9_17) di-assert LOW via `McSPICSAssert` (manual, per-byte).
4. **Byte `0xAF`** ditulis ke `MCSPI_TX0`; controller McSPI0 men-shift 8 bit keluar di CLK.
5. Polling `MCSPI_CH0STAT_TXS` (TX shift register empty) — tunggu sampai byte selesai di-clock-out.
6. Baca & drain `MCSPI_RX0` agar RX FIFO tidak overrun.
7. **CS** di-deassert HIGH.

CLK generator dikonfig pada **100 kHz** (periode 10 µs) supaya 8 pulsa = 80 µs mudah terlihat di scope. Tidak ada receiver yang dituju — MOSI ter-drive ke high-impedance beban; siapapun yang probing cukup melihat pulsa clock & data saja.

---

## Pemetaan Pin

### SPI0 (channel 0) — transmitter

| Sinyal | Pin header | Pad control register | Offset | Fungsi |
|---|---|---|---|---|
| **CLK** | P9_22 | `CONTROL_CONF_SPI0_SCLK` | `0x950` | SPI0 clock (CPOL=0, CPHA=0) |
| **MOSI** | P9_18 | `CONTROL_CONF_SPI0_D1` | `0x958` | SPI0 data line 1 → output enabled oleh `MCSPI_DATA_LINE_COMM_MODE_1` |
| **CS** | P9_17 | `CONTROL_CONF_SPI0_CS0` | `0x95c` | Hardware chip-select 0, active-low |

> **Catatan penting tentang MOSI.** `MCSPI_DATA_LINE_COMM_MODE_1` (`IS=0, DPE1=en, DPE0=dis`) berarti data output di-drive lewat **D1 (P9_18)**, bukan D0 (P9_21). Jika probe MOSI di P9_21, garis akan terlihat flat LOW meskipun CLK jalan — seperti bug controller, padahal wiring-nya benar. Probe MOSI selalu di **P9_18**.

### GPIO

| Sinyal | Pin header | SoC GPIO | Pad control register | Fungsi |
|---|---|---|---|---|
| **DC** | P8_26 | GPIO1[29] | `CONTROL_CONF_GPMC_CSN0` | Data/Command select untuk protokol ST7735-style |
| **RST** | P8_19 | GPIO0[22] | `CONTROL_CONF_GPMC_AD8` | Reset pulse visual (tidak dipakai oleh protokol SPI murni) |

Kedua pin GPIO dipinmux ke **mode 7** (`CONTROL_CONF_MUXMODE(7)`) untuk fungsi GPIO, lalu di-set sebagai output.

### Mode SPI

| Parameter | Nilai |
|---|---|
| Mode | Master, single-channel, transmit-and-receive |
| Word length | 8 bit |
| Clock rate | 100 kHz (configurable lewat `MCSPI_OUT_FREQ`) |
| CPOL / CPHA | Mode 0 (idle LOW, sample on rising edge) |
| CS polarity | Active-low |
| CS control | Manual per-byte via `McSPICSAssert` / `McSPICSDeAssert` |
| Data line comm mode | `MCSPI_DATA_LINE_COMM_MODE_1` (D1 output, D0 input) |
| FIFO | TX & RX enabled |

---

## Context Hardware

| Item | Value |
|---|---|
| SoC | TI AM3352 (Cortex-A8, little-endian, ARMv7-A) |
| Board | Antminer L3+ (PCB & form-factor = BeagleBone, tanpa PRU) |
| McSPI0 functional clock | 48 MHz dari L4 interconnect |
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

## Konfigurasi CCS

### New CCS Project

Buka **File → New → CCS Project**, lalu isi dalam satu halaman dialog:

1. **Project name**: `AM3352_SPI_TX` (atau apa pun — nama ini jadi nama ELF output).
2. **Location**: biarkan default (akan dibuat di dalam workspace).
3. **Connection**: pilih **SEGGER J-Link Emulator** (sesuai probe yang dipakai). Asumsinya, sebuah shared global target configuration **sudah ada** — biasanya `AM3352_bb_shared_target_configuration.ccxml` yang tersimpan di user-level config (bukan di dalam folder project), sudah dikonfigurasi untuk probe ini + Cortex-A8 core + device AM3352. Field target configuration cukup dikosongkan atau memilihnya dari dropdown referensi ke config global tersebut. **JANGAN** centang *"Create new target configuration"* — kita pakai yang global supaya tidak duplikasi config.
4. **Project type**: `Cortex A.AM3352` (atau `AM335x` generic).
5. Di panel **Project templates and examples**, pilih:
   ```
   ☑ Empty Project (with main.c)
   ```
   → otomatis generate `main.c` kosong.
6. **Toolchain**: **TI v20.2.7.LTS** (atau versi LTS CGT ARM yang ter-install — bukan GNU ARM).
7. Klik **Finish**.

### Konfigurasi Include Path StarterWare

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

### Konfigurasi Linker Command File

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

### Link Library StarterWare

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

---

## Struktur File Final Project

```
AM3352_SPI_TX/
├── .ccsproject         ← toolchain, device, linker cmd, origin path
├── .cproject           ← full build config (Debug + Release)
├── .project            ← Eclipse project metadata + linked build artifacts
├── .launches/
│   └── AM3352_SPI_TX.launch   ← debug launch config
├── .settings/
│   ├── org.eclipse.cdt.codan.core.prefs
│   ├── org.eclipse.cdt.debug.core.prefs
│   └── org.eclipse.core.resources.prefs
├── AM335x.cmd          ← TI CGT linker command file
└── main.c              ← application source
```

Tidak perlu ada `targetConfigs/` kalau pakai predefined ccxml global — CCS akan pakai yang di-set di **Window → Preferences → Code Composer Studio → Debug → Target Configurations**.

---

## Build & Flash

### Build

1. Pilih **Release** (atau Debug) di toolbar.
2. **Project → Build All** (`Ctrl+B`).
3. Output `.out` muncul di `${workspace_loc:/AM3352_SPI_TX/${ConfigName}/AM3352_SPI_TX.out}` (yaitu `Debug/` atau `Release/` di dalam folder project).

### Debug / Flash via J-Link

1. Hubungkan J-Link ke header JTAG board.
2. Pilih target configuration yang sesuai (predefined, mis. `am3352_bb_shared_target_configuration.ccxml`).
3. **Run → Debug** (`F11`) — CCS load `.out` ke 0x80000000 (DDR) dan PC mulai dari `Entry`.

---

## Yang Bisa Diamati di Osiloskop / Logic Analyzer

Probe channel dengan urutan yang disarankan:

| CH | Pin | Sinyal | Expected pattern |
|---|---|---|---|
| CH1 | P9_17 | CS | LOW pulse ~80–100 µs per iterasi, lalu HIGH |
| CH2 | P9_22 | CLK | 8 pulsa 100 kHz (periode 10 µs) tepat di tengah CS LOW |
| CH3 | P9_18 | MOSI | Bit pattern LSB-first dari `0xAF` = `1 1 1 1 0 1 0 1` (MSB-first dilihat di scope: `1010_1111`) |

**Sanity check urutan byte MOSI**: bit pertama yang muncul di CLK pertama harus sama dengan LSB `0xAF` = **1**. Jika bit pertama adalah 0, maka `0xAF` di-shift MSB-first (periksa `McSPIDataModeConfig` atau word-length setting).

**Sanity check CS**: lebar LOW harus ≥ 8 × periode CLK = 80 µs @ 100 kHz. Jika lebih pendek, controller `TXS` trigger sebelum byte selesai di-clock-out (kembali pakai `TXFFE` yang salah).