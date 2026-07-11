# AM3352_I2C_SSD1306_LCD

Project StarterWare portable yang mendemonstrasikan panel **SSD1306 OLED 128×32** lewat bus **I2C1** di SoC AM3352, dengan output debug via **UART0**. Berbasis `AM3352_I2C_SCANNER` (UART + I2C1 bring-up sama), lalu menambahkan driver SSD1306 dan tour shape/counter. Board target-nya **Antminer L3+** — sama PCB dan form-factor dengan BeagleBone Black (hanya tanpa PRU), jadi semua pad-mux dan clock-enable di `beaglebone.h` valid.

Source: `main.c` + `ssd1306/` &middot; Linker: `AM335x.cmd` &middot; Output: ELF di-load langsung ke DDR 0x80000000 via debugger (JTAG/SEGGER J-Link).

---

## Konteks Hardware

| Item | Value |
|---|---|
| SoC | TI AM3352 (Cortex-A8, little-endian, ARMv7-A) |
| Board | Antminer L3+ (PCB & form-factor = BeagleBone, tanpa PRU) |
| Panel | SSD1306 OLED 128×32 monokrom (I2C addr 0x3C) |
| Bus display | I2C1 |
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

SoC AM3352 memang sudah punya internal pull-up ~50 kΩ di pad I2C1, tapi **`I2CPinMuxSetup(1)` di StarterWare tidak mengaktifkan bit `PUDEN`** di pin-mux config — jadi pull-up internal-nya idle. Untuk I2C di 100 kHz, 50 kΩ sudah di borderline (rise-time-nya terlalu lambat), dan kalau dipakai external pull-up tidak ada, **slave SSD1306 tidak akan pernah ter-ACK dengan benar**:

- Probe ke address slave (`ssd1306_Init`) gagal — display tidak nyala.
- Bus hang di `while(I2CMasterBusBusy)` selamanya.

**Solusi**: solder dua resistor **10 kΩ** dari:
- P9_17 (SCL) → header VDD_3V3 (P9_3 atau P9_4)
- P9_18 (SDA) → header VDD_3V3 (P9_3 atau P9_4)

Verifikasi cepat dengan multimeter: saat bus idle, SDA dan SCL harus berada di sekitar 3.3 V (bukan floating 1.x V atau 0 V).

> 💡 **OLED tidak menampilkan apa-apa setelah flash?** Jika pin I2C-nya sudah benar (SCL = P9_17, SDA = P9_18, sudah di-pinmux via `I2CPinMuxSetup(1)`, firmware bisa cetak SCL 100 kHz di scope), **coba dulu pull-up 10 kΩ ke VDD_3V3 di kedua line** seperti di section [Pull-Up Resistor Wajib](#-pull-up-resistor-wajib). Itu 95% solusinya — bukan bug firmware.

---

## Contoh Output UART

Probe loop hanya-baca — tidak ada interaksi host. Berikut contoh saat firmware boot:

```
+---------------------------------------------+
|  AM3352  SSD1306  128x32  OLED  Demo        |
|  I2C1  SCL=P9_17  SDA=P9_18  addr=0x3C      |
+---------------------------------------------+

I2C1 ready, init SSD1306...
ssd1306_Init OK — splash on.
```

Setelah splash, OLED berganti menampilkan tour shape (rect, filled rect, circle, filled circle, line, triangle), progress bar 0%→100%, dan counter 0..99 dalam font 11×18. UART tidak mem-print setiap shape — semua visual hanya di OLED.

---

## Yang Dilakukan Firmware

1. **UART0 bring-up** — 115200 8N1, FIFO enabled, line-status interrupt registered.
2. **ConsoleUtilsInit** + `CONSOLE_UART` → `ConsoleUtilsPrintf` aktif.
3. **Banner ASCII box** — info board + pin I2C.
4. **I2C1 master** — `SetupI2C1Master()`: clock enable, pin-mux, soft-reset, 100 kHz from 48 MHz.
5. **Banner "I2C1 ready"**.
6. **`ssd1306_Init()`** — display OFF, set mux ratio, set offset, set clock, set charge pump, set contrast, set memory mode (horizontal addressing), set COM output scan direction, display ON.
7. **Loop demo** — `DrawSplash` → `DrawShapeTour` → `DrawProgress` → `DrawCounter` → repeat.

---

## Struktur File Final Project

```
AM3352_I2C_SSD1306_LCD/
├── .ccsproject         ← toolchain, device, linker cmd, origin path
├── .cproject           ← full build config (Debug + Release)
├── .project            ← Eclipse project metadata + linked build artifacts
├── .launches/
│   └── AM3352_I2C_SSD1306_LCD.launch   ← debug launch config
├── .settings/
│   ├── org.eclipse.cdt.codan.core.prefs
│   ├── org.eclipse.cdt.debug.core.prefs
│   └── org.eclipse.core.resources.prefs
├── targetConfigs/
│   ├── AM3352.ccxml            ← target configuration (fallback lokal)
│   └── readme.txt
├── AM335x.cmd          ← TI CGT linker command file
├── README.md           ← you are here
├── main.c              ← application source — UART + I2C1 + SSD1306 demo loop
└── ssd1306/
    ├── ssd1306.c       ← drawing primitives: line/rect/circle/triangle, buffer flush
    ├── ssd1306.h       ← public API
    ├── fonts.c         ← bitmap font 7×10, 11×18, 16×26 (lookup table, ROM)
    └── fonts.h         ← font struct + Font_7x10 / Font_11x18 / Font_16x26
```

Tidak perlu ada `targetConfigs/` sendiri kalau pakai predefined ccxml global — CCS akan pakai yang di-set di **Window → Preferences → Code Composer Studio → Debug → Target Configurations**.

---

## Catatan: Target Configuration

Project ini awalnya tidak punya `targetConfigs/AM3352.ccxml` saat pertama kali dibuat. **Gejalanya**: build sukses, `.out` terbentuk normal, tapi flashing "diam" — UART tidak menampilkan apa-apa, OLED tidak nyala, seolah board hang. **Bukan** masalah firmware atau hardware. Tanpa `.ccxml` di proyek ini, CCS tidak punya definisi probe/board yang benar untuk download `.out` ke target (alamat entry, reset sequence). Build tools (gmake + armcl) tidak peduli — mereka hanya menghasilkan `.out` di folder Debug/.

Solusi minimal: copy `AM3352.ccxml` ke `targetConfigs/` (atau pakai shared User Defined di CCS Preferences). Setelah itu flash langsung jalan.

---

## Driver SSD1306: API Singkat

`ssd1306.c`/`ssd1306.h` menyediakan:

```c
int  ssd1306_Init(void);                          /* return 1 kalau panel ACK di 0x3C, 0 kalau gagal */
void ssd1306_Clear(void);
void ssd1306_UpdateScreen(void);                 /* flush framebuffer ke panel via I2C */
void ssd1306_SetCursor(uint8_t x, uint8_t y);     /* kolom, page (0..3 untuk 128x32) */
void ssd1306_WriteString(char *str, FontDef font);
void ssd1306_SetColor(ssd1306_COLOR color);      /* White atau Black */
void ssd1306_DrawLine(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1);
void ssd1306_DrawRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h);
void ssd1306_FillRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h);
void ssd1306_DrawCircle(uint8_t cx, uint8_t cy, uint8_t r);
void ssd1306_FillCircle(uint8_t cx, uint8_t cy, uint8_t r);
void ssd1306_DrawTriangle(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2);
void ssd1306_DrawBitmap(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const uint8_t *buf);
```

Transport I2C di belakang driver dipanggil lewat dua callback lemah di `main.c`:

```c
void ssd1306_I2C_WriteCommand(uint8_t command);
void ssd1306_I2C_WriteData(uint8_t *data, uint16_t size);
```

Keduanya polling-only (tidak ISR) — sama dengan pola I2C scanner, jadi deterministik untuk transfer pendek 1..32 byte per flush.

---

## Catatan Build

`main.c` dideklarasikan sebagai source pertama (`Debug/sources.mk`), `ssd1306/fonts.c` & `ssd1306/ssd1306.c` sebagai subdir kedua. Linker menarik objs sesuai referensi simbol: kalau `main.c` tidak menyebut simbol dari `ssd1306/*.c`, linker akan drop objs itu — `sin/cos` tidak akan masuk ke image. Untuk firmware OLED demo saat ini, semua simbol dipakai.