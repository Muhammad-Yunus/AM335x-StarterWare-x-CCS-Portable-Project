# AM3352_GPIO_INTERRUPT

Project StarterWare portable yang mendemonstrasikan **GPIO input interrupt** pada pin **P9_12** SoC AM3352, ditambah **UART0 echo baseline** sebagai bukti bring-up SoC sudah benar. Setiap kali tombol ditekan (P9_12 transisi LOW → HIGH), GPIO ISR set flag, lalu main-loop mengirim pesan ke UART0 untuk konfirmasi.

Project ini adalah _template_ kedua yang bisa dipakai ulang untuk eksperimen interrupt / peripheral lain di board berbasis AM3352 (misalnya **Antminer L3+** — board ini share form-factor dan PCB routing dengan BeagleBone, hanya tanpa PRU subsystem). Berbagi struktur project yang sama dengan `AM3352_GPIO_LED`.

Source: `main.c` · Linker: `AM335x.cmd` · Output: ELF di-load langsung ke DDR 0x80000000 via debugger (JTAG/SEGGER J-Link).

---

## Konteks Hardware

| Item | Value |
|---|---|
| SoC | TI AM3352 (Cortex-A8, little-endian, ARMv7-A) |
| Board | Antminer L3+ (PCB & form-factor = BeagleBone, tanpa PRU) |
| Pin input interrupt | **P9_12** (pad `gpmc_ben1`, GPIO1[28], gpio number 60) |
| Pin mux | Mode 7 (fungsi GPIO), **RX enabled**, internal pulldown |
| Sys interrupt | `SYS_INT_GPIOINT1A` (98), line `GPIO_INT_LINE_1` |
| Console | UART0 @ 115200 8N1 |
| Debounce | enable per-pin, ~1.5 ms (debounce time 48) |
| Trigger | **Rising edge** (active-high button: LOW idle, HIGH on press) |
| Debug probe | SEGGER J-Link |

Mapping pin BeagleBone P9_12 ke AM335x:

| BBB header | Ball AM335x | Pad register | Mux mode 7 | GPIO bank | GPIO input # |
|------------|-------------|--------------|------------|-----------|--------------|
| P9_12      | `gpmc_ben1` | `0x0878`     | GPIO1[28]  | 1         | 60           |

Makro pad-register offset `0x0878` di StarterWare adalah `GPIO_1_28` (lihat `include/armv7a/am335x/pin_mux.h`).

Library StarterWare yang dipakai sudah prebuilt di:
```
C:\ti\AM335X_StarterWare_02_00_01_01\binary\armv7a\cgt_ccs\am335x\drivers\Release\drivers.lib
C:\ti\AM335X_StarterWare_02_00_01_01\binary\armv7a\cgt_ccs\am335x\system_config\Release\system.lib
C:\ti\AM335X_StarterWare_02_00_01_01\binary\armv7a\cgt_ccs\am335x\beaglebone\platform\Release\platform.lib
C:\ti\AM335X_StarterWare_02_00_01_01\binary\armv7a\cgt_ccs\utils\Release\utils.lib
```
Folder StarterWare ini dipakai **read-only** — tidak ada file StarterWare yang diubah atau dicopy ke project.

---

## Skema Rangkaian Pushbutton Active-High

Tombol pushbutton dipasang antara pin **P9_12** dan **VDD_ADC (3V3)** di header P9. Saat tombol **ditekan** → P9_12 tertarik ke **HIGH (~3V3)**. Saat tombol **dilepas** → P9_12 ditarik ke **LOW (GND)** oleh resistor pulldown.

```
                       3V3 (HEADER P9 PIN 3 / P9 PIN 4 = VDD_ADC)
                        │
                        │
                        ├───────────┐
                        │           │
                        │         ┌─────┐
                        │         │ BTN │  ← pushbutton normally-open
                        │         └─────┘
                        │           │
                        │         (contact closes on press)
                        │           │
                        │           ├──────────── P9_12  (HEADER P9, pin 12)
                        │           │
                        │         ┌─────┐
                        │         │ 10k │  ← R_pulldown
                        │         └─────┘
                        │           │
                       GND ─────────┴──────────── (HEADER P9 GND, e.g. P9 pin 1/2)
```

---

## Cara Membuat Project Seperti Ini Dari Nol (Manual di CCS)

### 1. New CCS Project

Buka **File → New → CCS Project**, lalu isi dalam satu halaman dialog:

1. **Project name**: `AM3352_GPIO_INTERRUPT` (atau apa pun — nama ini jadi nama ELF output).
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

### 3. Konfigurasi Include Path StarterWare

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

### 4. Konfigurasi Linker Command File

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

### 5. Link Library StarterWare

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

### 6. Tulis `main.c`

Replace isi `main.c` yang kosong dengan kode di bawah. Program menggabungkan dua hal:

- **UART0 echo baseline** — apa pun yang diketik di terminal di-echo balik (line-buffered, flushed on Enter atau setelah idle). Memvalidasi bahwa SoC boot, clock, UART, dan INTC sudah benar.
- **GPIO60 (P9_12) rising-edge interrupt** — setiap tekan pushbutton aktif = satu entry ISR. Main loop membaca flag + counter dari ISR dan print ke UART. ISR sendiri **tidak** mengirim apa-apa (tidak ada blocking call dari context ISR).

```c
#include "soc_AM335x.h"
#include "beaglebone.h"
#include "gpio_v2.h"
#include "uart_irda_cir.h"
#include "hw_uart_irda_cir.h"
#include "hw_types.h"

/* === Pin / interrupt mapping === */
#define GPIO_INPUT_INSTANCE     SOC_GPIO_1_REGS
#define GPIO_INPUT_PIN          (28)
#define GPIO_INPUT_INTR_LINE    GPIO_INT_LINE_1
#define GPIO_INPUT_SYS_INT_NUM  SYS_INT_GPIOINT1A   /* 98 */
#define GPIO_PAD_OFFSET         (0x0878u)

/* === UART line-buffer state === */
#define CMD_BUF_SIZE            (128)
static char     cmdBuf[CMD_BUF_SIZE];
static unsigned int cmdLen           = 0;
static unsigned int lastCmdLenSnap   = 0;
#define IDLE_LIMIT_HITS  (100000u)
static unsigned int idleHits         = 0;

/* === ISR -> main-loop hand-off === */
static volatile unsigned int gGpioIsrFlag      = 0;
static volatile unsigned int gGpioIsrEdgeCount = 0;

/* === Optional heartbeat === */
#define HEARTBEAT_PERIOD  (2000000u)
static unsigned int gHeartbeatHits = 0;

/* (implementasi UartPutString, UART0 setup, GPIOInputInit, GPIOINTCConfigure,
**  GPIOIsr, UARTIsr, main loop ... — lihat file main.c) */
```

Pola inti dari kode:

**Pin-mux (RX ENABLED untuk GPIO input):**
```c
GPIO1ModuleClkConfig();
GpioPinMuxSetup(GPIO_1_28, PAD_FS_RXE_PD_PUPDE(7));   /* RXACTIVE = 1 */
```

**Module init + dir = input + debounce:**
```c
GPIOModuleEnable(SOC_GPIO_1_REGS);
GPIOModuleReset(SOC_GPIO_1_REGS);
GPIODirModeSet(SOC_GPIO_1_REGS, 28, GPIO_DIR_INPUT);
GPIODebounceFuncControl(SOC_GPIO_1_REGS, 28, GPIO_DEBOUNCE_FUNC_ENABLE);
GPIODebounceTimeConfig(SOC_GPIO_1_REGS, 48);
```

**Trigger + enable pin interrupt + register ISR:**
```c
GPIOIntTypeSet(SOC_GPIO_1_REGS, 28, GPIO_INT_TYPE_RISE_EDGE);
GPIOPinIntEnable(SOC_GPIO_1_REGS, GPIO_INT_LINE_1, 28);

IntRegister(SYS_INT_GPIOINT1A, GPIOIsr);
IntPrioritySet(SYS_INT_GPIOINT1A, 0, AINTC_HOSTINT_ROUTE_IRQ);
IntSystemEnable(SYS_INT_GPIOINT1A);
IntMasterIRQEnable();   /* sudah enabled oleh UART path */
```

**ISR aman (wajib clear status):**
```c
static void GPIOIsr(void) {
    if (GPIOPinIntStatus(SOC_GPIO_1_REGS, GPIO_INT_LINE_1, 28) & (1u << 28)) {
        GPIOPinIntClear(SOC_GPIO_1_REGS, GPIO_INT_LINE_1, 28);
    }
    gGpioIsrEdgeCount++;
    gGpioIsrFlag = 1;
}
```

**Catatan Antminer L3+**: header `beaglebone.h` dipakai karena board ini PCB-identik dengan BeagleBone — pin-mux `GPIO1ModuleClkConfig()` + `GpioPinMuxSetup(GPIO_1_28, …)` keduanya **valid** untuk L3+. Kalau ternyata board L3+ kamu *routing* GPIO-nya beda, ganti ke konstanta yang sesuai (`GPIO_1_28` didefinisikan di `include/armv7a/am335x/pin_mux.h`).

### 7. Build

1. Pilih **Release** (atau Debug) di toolbar.
2. **Project → Build All** (`Ctrl+B`).
3. Output `.out` muncul di `${workspace_loc:/AM3352_GPIO_INTERRUPT/${ConfigName}/AM3352_GPIO_INTERRUPT.out}` (yaitu `Debug/` atau `Release/` di dalam folder project).

### 8. Debug / Flash via J-Link

1. Hubungkan J-Link ke header JTAG board.
2. Pilih target configuration yang sesuai (predefined, mis. `am3352_bb_shared_target_configuration.ccxml`).
3. **Run → Debug** (`F11`) — CCS load `.out` ke 0x80000000 (DDR) dan PC mulai dari `Entry`.

---

## Cara Menjalankan / Menguji

1. Pastikan pushbutton **active-high** terpasang antara **P9_12** dan **3V3** (lihat Skema Rangkaian di atas).
2. Buka terminal serial di UART0 (115200 8N1).
3. Banner project tercetak di awal. Karakter yang diketik di terminal di-echo balik dengan prefix `gpio>` di akhir baris.
4. Setiap tekan tombol = satu baris `[ISR] GPIO60 (P9_12) edge #N` di UART, counter naik.
5. Sekitar tiap detik main loop print `[hb] P9_12 level = 0|1` (heartbeat debug) — konfirmasi pin benar-benar dibaca.

---

## Output yang Diharapkan di Serial

```
============================================================
  AM3352 GPIO INTERRUPT DEMO
  Target  : Antminer L3+ (BeagleBone form-factor)
  Pin     : P9_12 = GPIO1[28] = global GPIO 60
  Trigger : Rising edge on GPIO_INT_LINE_1
  Console : UART0 @ 115200 8N1
============================================================

Type characters to echo. Press button/signal on P9_12 to
fire ISR — main loop will print: [ISR] GPIO60 edge #N.


=== AM3352 GPIO INTERRUPT (P9_12 = GPIO60) ===
Type something + Enter to echo. Press button on P9_12 to trigger ISR.

[hb] P9_12 level = 0
[ISR] GPIO60 (P9_12) edge #1
[ISR] GPIO60 (P9_12) edge #2
[ISR] GPIO60 (P9_12) edge #3
```

`[hb] P9_12 level = 0` muncul tiap ~detik saat tombol dilepas; `= 1` saat ditekan. Setiap tekan naik counter `edge #N`.

---

## Catatan Implementasi

- **Trigger rising edge**: active-high button = ditekan = HIGH. Falling edge dipakai kalau tombolnya active-low (satu sisi ke GND). Untuk switch ke `GPIO_INT_TYPE_BOTH_EDGE` kalau mau fire dua arah.
- **Debounce**: di hardware (~1.5 ms) lewat `GPIODebounceFuncControl` + `GPIODebounceTimeConfig(48)`. Cukup untuk pushbutton mekanik standar. Untuk kontak sangat jelek, tambahkan debounce software di ISR (timestamp via DMTimer).
- **ISR aman**: `GPIOPinIntClear` **wajib** dipanggil di dalam ISR. Tanpa clear, INTC re-trigger tanpa henti. ISR ini cuma clear status + increment counter + set flag — semua output UART dilakukan di main-loop (tidak ada blocking call dari context ISR).
- **RX UART dipoll** dari main (pola `uartEcho`), TX pakai ISR FIFO; keduanya tidak konflik dengan GPIO ISR karena `IntPrioritySet` untuk ketiganya pakai priority 0 dengan IRQ channel berbeda (eksekusi serial di A8 INTC).
- **Heartbeat `[hb]`**: polling `GPIOPinRead` tiap ~2 M loop spin. Bisa dibuang dengan menghapus blok `if(++gHeartbeatHits >= HEARTBEAT_PERIOD)` di main loop kalau sudah tidak diperlukan.
- **Pin alternatif**: kalau P9_12 di layout L3+ ternyata dipakai peripheral lain, ganti tiga konstanta di `main.c` (`GPIO_INPUT_PIN`, `GPIO_PAD_OFFSET`, `GPIO_INPUT_SYS_INT_NUM`):
  - Pin GPIO1_6 (SYS_INT_GPIOINT1A) — bank 1, line 1.
  - Pin GPIO0_6 (SYS_INT_GPIOINT0A) — pindah ke bank 0, line 1.
  - Cek offset `GPIO_X_N` di `pin_mux.h` untuk nilai `GPIO_PAD_OFFSET`.

---

## Struktur File Final Project

```
AM3352_GPIO_INTERRUPT/
├── .ccsproject         ← toolchain, device, linker cmd, origin path
├── .cproject           ← full build config (Debug + Release)
├── .project            ← Eclipse project metadata + linked build artifacts
├── .launches/
│   └── AM3352_GPIO_INTERRUPT.launch   ← debug launch config
├── .settings/
│   ├── org.eclipse.cdt.codan.core.prefs
│   ├── org.eclipse.cdt.debug.core.prefs
│   └── org.eclipse.core.resources.prefs
├── AM335x.cmd          ← TI CGT linker command file
└── main.c              ← application source (UART echo + GPIO ISR)
```

Tidak perlu ada `targetConfigs/` kalau pakai predefined ccxml global — CCS akan pakai yang di-set di **Window → Preferences → Code Composer Studio → Debug → Target Configurations**.