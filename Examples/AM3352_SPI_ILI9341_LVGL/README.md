# AM3352_SPI_ILI9341_LVGL

Project **StarterWare** untuk **AM3352** (Cortex-A8, Antminer L3+ / BeagleBone-class board) yang mengintegrasikan **LVGL v9** dengan **LCD TFT ILI9341 2.8"** via **SPI0** dan menjalankan **LVGL Music Demo**.

---

## Fitur Utama

| Fitur | Detail |
|---|---|
| **SoC** | AM3352 @ 600 MHz, ARM Cortex-A8 |
| **LCD** | ILI9341 2.8", 320x240, RGB565, landscape |
| **SPI** | SPI0, 24 MHz, mode 0, TX-only, TURBO mode |
| **DMA** | EDMA3 channel 16 untuk McSPI0 TX (opsional) |
| **LVGL** | v9, FULL render mode, dual frame buffer (307 KB) |
| **Demo** | LVGL Music Demo (`LV_USE_DEMO_MUSIC = 1`) |
| **Toolchain** | TI CGT ARM (C89), CCS Theia |
| **Debug** | SEGGER J-Link, DDR @ 0x80000000 |

---

## Pemetaan Pin

### SPI0 (channel 0) — bus ke ILI9341

| Sinyal | Header | Pad control register | Offset | Fungsi |
|---|---|---|---|---|
| **CLK** | P9_22 | `CONTROL_CONF_SPI0_SCLK` | `0x950` | SPI0 clock (mode 0) |
| **MOSI** | P9_18 | `CONTROL_CONF_SPI0_D1` | `0x958` | Output via `MCSPI_DATA_LINE_COMM_MODE_1` |
| **CS** | P9_17 | `CONTROL_CONF_SPI0_CS0` | `0x95c` | Hardware CS, active-low |

> MOSI di-drive lewat **D1 (P9_18)**, bukan D0 (P9_21).

### GPIO (control ke ILI9341)

| Sinyal | Header | SoC GPIO | Pad control register | Fungsi |
|---|---|---|---|---|
| **DC** | P8_26 | GPIO1[29] | `CONTROL_CONF_GPMC_CSN0` | LOW = command, HIGH = data |
| **RST** | P8_19 | GPIO0[22] | `CONTROL_CONF_GPMC_AD8` | Active-low reset |

### DEBUG LED

| Sinyal | SoC GPIO | Fungsi |
|---|---|---|
| **D2** | GPIO1[23] | Indikator SPI TX aktif (antminer L3+) |

---

## Mode SPI

| Parameter | Nilai |
|---|---|
| Mode | Master, single-channel, TX-only |
| Word length | 8 bit |
| Clock | 24 MHz (CLKD=0 override, FUNC_CLK/2) |
| CPOL/CPHA | Mode 0 |
| CS polarity | Active-low |
| FIFO | TX enabled, 256-byte, AFL=64 |
| TURBO | Enabled (tanpa inter-word gap) |
| Data line comm mode | `MCSPI_DATA_LINE_COMM_MODE_1` (D1 out, D0 in) |
| DMA | EDMA3 channel 16, event-triggered |

---

## Arsitektur Driver

```
main.c
  ├── Spi0Init (static)                  ← McSPI0 register-level + CLKD=0 + TURBO
  ├── Spi0TxByte / Spi0TxBuffer          ← byte/block CS per-byte
  ├── Spi0TxBufferFast                   ← high-throughput, CS dipegang penuh
  ├── Edma3Init / EdmaSpiTx              ← EDMA3 channel 16 untuk SPI TX
  ├── LcdDcLow / LcdDcHigh               ← GPIO1[29]
  └── LcdRstLow / LcdRstHigh             ← GPIO0[22]
         ↓
ili9341.c                               ← hooks dari main.c + fonts.h
  ├── ILI9341_Init()                     ← RST pulse + boot sequence
  ├── ILI9341_SetAddressWindow()
  ├── ILI9341_FillScreen / FillRect / DrawRect
  ├── ILI9341_DrawLine (Bresenham)
  ├── ILI9341_DrawCircle / FillCircle (midpoint)
  ├── ILI9341_DrawChar / DrawString      ← Font5x7
  └── ILI9341_WritePixels
         ↓
lv_port_disp.c                          ← LVGL v9 display driver
  ├── lv_port_disp_init()                ← init ILI9341 + create LVGL display
  ├── LCD_FlushDisplay()                 ← flush callback via Spi0TxBufferFast
  └── s_frameBuffer[2]                   ← dual full-frame buffer (2×153600 B)
         ↓
lv_conf.h                               ← konfigurasi LVGL v9
  ├── LV_COLOR_DEPTH 16
  ├── LV_COLOR_16_SWAP 1
  ├── LV_MEM_SIZE 256 KB
  └── LV_USE_DEMO_MUSIC 1
```

`ili9341.c` **tidak** mengakses register SoC langsung — semua transaksi lewat hooks yang `main.c` sediakan (DC/RST GPIO + SPI TX). Ini menjaga driver tetap reusable.

---

## LVGL Display Port

Dua strategi transfer yang diimplementasikan:

### 1. `Spi0TxBufferFast` (high-throughput, default)

- CS di-assert sekali untuk seluruh blok pixel.
- TX FIFO diisi 192 byte per burst, menunggu `TXFFF` de-assert sebelum refill.
- Menunggu `EOT` (End of Transfer) sebelum CS de-assert.
- **~3× lebih cepat** daripada `Spi0TxBuffer` per-byte CS.

### 2. `EdmaSpiTx` (EDMA3, opsional)

- DMA channel 16 untuk McSPI0 TX.
- PaRAM: ACNT = byteCount, BCNT = 1, src increment, dst fixed (TX reg).
- Polling IPR untuk completion (belum pakai IRQ).
- EOT polling setelah DMA selesai.

### Frame Buffer

- Dua buffer full-frame `s_frameBuffer[2]` (2 × 153,600 B = 307 KB).
- `LV_DISPLAY_RENDER_MODE_FULL` — tanpa PARTIAL tiling overhead.
- Alokasi di `.bss` (DDR).

---

## LVGL Music Demo

Diaktifkan via `lv_conf.h`:

```c
#define LV_USE_DEMO_MUSIC          1
#define LV_DEMO_MUSIC_SQUARE       1       /* orientasi square */
#define LV_DEMO_MUSIC_LANDSCAPE    0
#define LV_DEMO_MUSIC_ROUND        0
#define LV_DEMO_MUSIC_LARGE        0
#define LV_DEMO_MUSIC_AUTO_PLAY    0       /* auto-play mati hemat CPU */
```

Demo menampilkan:
- UI music player modern seperti smartphone.
- Cover album, progress bar, tombol play/pause/next/prev.
- Spectrum visualizer dengan 4 band (bass, bass-mid, mid, mid-treble).
- Animasi zoom cover album berdasarkan bass.
- LVGL menangani semua rendering; ILI9341 hanya menerima pixel stream via SPI.

---

## Struktur File Project

```
AM3352_SPI_ILI9341_LVGL/
├── AM335x.cmd                   ← Linker cmd (DDR @ 0x80000000)
├── main.c                       ← SPI0 + GPIO + EDMA3 + tick init
├── ili9341.c                    ← Driver ILI9341 (port dari STM32H7)
├── ili9341.h                    ← Public API ILI9341
├── lv_conf.h                    ← Konfigurasi LVGL v9
├── lv_port_disp.c               ← LVGL display driver (flush callback)
├── lv_port_disp.h               ← Header display driver
├── fonts.h                      ← Font 5x7 ASCII (untuk ili9341.c)
├── README.md                    ← File ini
├── Middlewares/
│   └── lvgl/                    ← LVGL v9 library (read-only)
└── targetConfigs/               ← CCS target config (J-Link + AM3352)
```

Library StarterWare dipakai **read-only** dari `C:\ti\AM335X_StarterWare_02_00_01_01\...`.

---

## Build & Run

1. Buka project di **CCS Theia**.
2. Pastikan include path StarterWare dan library `drivers.lib / system.lib / platform.lib / utils.lib / libc.a` sudah benar.
3. Build (Ctrl+B).
4. Hubungkan **SEGGER J-Link** ke header JTAG board.
5. `Run → Debug` (F11). CCS load `.out` ke `0x80000000`.
6. Setelah CPU berjalan, LCD menampilkan **LVGL Music Demo**.

---

## Catatan Build

- **C89 default**. TI CGT ARM mewajibkan deklarasi variabel di awal blok.
- **Linker melihat symbol lintas file**. `Spi0TxByte`, `Spi0TxBuffer`, `Spi0TxBufferFast`, dan `LcdDcLow/High/RstLow/RstHigh` di-declare **tanpa `static`** agar bisa dipanggil dari `ili9341.c` / `lv_port_disp.c`.
- **`delay.h`** StarterWare butuh `IntAINTCInit()` + `IntMasterIRQEnable()` dipanggil pertama di `main()` — lihat catatan di bawah.

---

## Catatan delay.h & LVGL Tick

Dua pendekatan tick yang tersedia di kode:

### 1. DMTimer7 IRQ-based (delay.h default)
`delay(unsigned int ms)` menunggu overflow DMTimer7 via IRQ (`Sysdelay()` → busy-wait flag `flagIsr`). Wajib panggil:
```c
IntAINTCInit();         /* reset AINTC, seed vector table */
IntMasterIRQEnable();   /* clear I-bit di CPSR */
DelayTimerSetup();      /* register DMTimerIsr ke slot IRQ Timer7 */
```

### 2. Software-polled LVGL tick (alternatif)
DMTimer7 sebagai free-running counter 32-bit @ 24 MHz, di-polling tiap iterasi main loop:
- `PollLvglTick()` — baca `TCRR`, hitung delta, panggil `lv_tick_inc()`.
- Tidak perlu IRQ handler untuk tick.
- Kode tersedia di `main.c` dalam blok `#if 0`.

---

## EDMA3 SPI TX

- **Channel**: EDMA3 DMA channel 16, TCC 16.
- **Event**: McSPI0 TX event (hardware-synced).
- **Transfer**: Single-shot, ACNT = byteCount, BCNT = 1, src increment, dst constant (TX reg).
- **Completion**: Polling IPR (belum pakai IRQ AINTC).
- **Sequence**: Assert CS → enable DMA → trigger event → polling IPR → disable DMA → polling EOT → deassert CS.

---

## Catatan Driver ILI9341

- **`ILI9341_DrawChar`** re-set address window per kolom (7-pixel column isolated) agar auto-increment tidak wrap. Overhead: 5 transaksi register per karakter.
- **Boot sequence**: Reset pulse → sleep out → display on → pixel format → MAC (orientation) → gamma/PWR/FRC config.

---

## Tuning Lebih Lanjut

- **Clock SPI**: `CLKD=0 → SPI_CLK = FUNC_CLK/2 = 24 MHz` (dengan FUNC_CLK = 48 MHz). Untuk PCB pendek bisa coba `FUNC_CLK` lebih tinggi.
- **EDMA3 IRQ**: Ganti polling IPR dengan IRQ AINTC untuk non-blocking DMA.
- **Frame buffer**: Kurangi ke single buffer + PARTIAL render mode untuk hemat RAM (307 KB cukup besar untuk DDR).
- **Auto-play**: Aktifkan `LV_DEMO_MUSIC_AUTO_PLAY 1` untuk demo otomatis ~60 detik.
- **Tambah demo**: Aktifkan `LV_USE_DEMO_WIDGETS`, `LV_USE_DEMO_BENCHMARK`, dll di `lv_conf.h`.

---

## Pin Mapping Reference (LCD Header)

Berikut mapping LCD 2.8" (ILI9341) ke header BeagleBone-compatible:

| LCD Pin | Fungsi       | Header BBB | SoC       |
|---------|-------------|------------|-----------|
| 1       | GND         | -          | -         |
| 2       | VCC (5V/3V)| P9_5 / P9_6| -         |
| 3       | CS          | P9_17      | SPI0_CS0  |
| 4       | RESET       | P8_19      | GPIO0[22] |
| 5       | DC/RS       | P8_26      | GPIO1[29] |
| 6       | MOSI        | P9_18      | SPI0_D1   |
| 7       | SCK         | P9_22      | SPI0_SCLK |
| 8       | LED (BL)    | P9_7 (3V)  | -         |
| 9       | MISO        | P9_21      | SPI0_D0   |

> LED (backlight) pin LCD di-drive P9_7 (3.3V) agar nyala terus. Alternatif bisa pakai PWM untuk dimming.

---
