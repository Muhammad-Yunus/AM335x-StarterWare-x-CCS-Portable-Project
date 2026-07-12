# AM3352_SPI_ILI9341

Project StarterWare portable untuk **AM3352** (Cortex-A8, Antminer L3+ /
BeagleBone-class board) yang menampilkan **demo grafis dasar pada LCD TFT
ILI9341 2.8"** melalui **SPI0**. Berangkat dari baseline
`AM3352_SPI_TX`, proyek ini menambah:

- Driver ILI9341 (port dari referensi STM32H7 `ili9341_NUCLEO_H753ZI`,
  register-by-register, tanpa dependensi LVGL).
- Driver GPIO untuk jalur DC/RST (LCD).
- Delay `delay.h` StarterWare (DMTimer7 IRQ-based) — dipakai oleh
  `ILI9341_Reset()` dan demo timing.
- Primitive grafis: fill screen, fill rect, draw rect, draw line,
  filled / hollow circle, draw text 5x7 ASCII.
- Empat scene demo berputar selamanya.

Output ELF di-load ke DDR `0x80000000` via JTAG/SEGGER J-Link.

---

## Pemetaan Pin

### SPI0 (channel 0) — bus ke ILI9341

| Sinyal | Header | Pad control register | Offset | Fungsi |
|---|---|---|---|---|
| **CLK** | P9_22 | `CONTROL_CONF_SPI0_SCLK` | `0x950` | SPI0 clock (mode 0) |
| **MOSI** | P9_18 | `CONTROL_CONF_SPI0_D1` | `0x958` | Output via `MCSPI_DATA_LINE_COMM_MODE_1` |
| **CS** | P9_17 | `CONTROL_CONF_SPI0_CS0` | `0x95c` | Hardware CS, active-low |

> MOSI di-drive lewat **D1 (P9_18)**, bukan D0 (P9_21). Probe MOSI
> selalu di P9_18 — kalau di P9_21 akan terlihat flat LOW.

### GPIO (control ke ILI9341)

| Sinyal | Header | SoC GPIO | Pad control register | Fungsi |
|---|---|---|---|---|
| **DC** | P8_26 | GPIO1[29] | `CONTROL_CONF_GPMC_CSN0` | LOW = command, HIGH = data |
| **RST** | P8_19 | GPIO0[22] | `CONTROL_CONF_GPMC_AD8` | Active-low reset (pulse 10 ms low lalu high) |

---

## Mode SPI

| Parameter | Nilai |
|---|---|
| Mode | Master, single-channel, TX/RX |
| Word length | 8 bit |
| Clock | 10 MHz (configurable lewat `MCSPI_OUT_FREQ`) |
| CPOL/CPHA | Mode 0 |
| CS polarity | Active-low, manual per byte |
| FIFO | TX + RX enabled |
| Data line comm mode | `MCSPI_DATA_LINE_COMM_MODE_1` (D1 out, D0 in) |

Catatan throughput: `Spi0TxBuffer()` saat ini melakukan CS per byte (byte
granularity), throughput efektif ~0,5–1 frame per detik untuk full
fill. Untuk optimasi nanti, pertahankan CS asserted saat push pixel
stream `LcdDcHigh()` + non-aktifkan CS hanya di akhir address-window
update — typical gain 2–4×.

---

## Arsitektur Driver

```
main.c
  ├── Spi0Init (static)                          ← McSPI0 register-level
  ├── Spi0TxByte / Spi0TxBuffer (extern)          ← exported ke ili9341.c
  ├── LcdDcLow / LcdDcHigh (extern)               ← GPIO1[29]
  └── LcdRstLow / LcdRstHigh (extern)             ← GPIO0[22]
         ↓
ili9341.c   ← pakai hooks di atas + fonts.h
  ├── ILI9341_Init()                              ← RST pulse + boot sequence
  ├── ILI9341_SetAddressWindow(x0,y0,x1,y1)       ← 0x2A, 0x2B, 0x2C
  ├── ILI9341_FillScreen(c)                       ← FillRect full-screen
  ├── ILI9341_FillRect / DrawRect
  ├── ILI9341_DrawLine (Bresenham)
  ├── ILI9341_DrawCircle / FillCircle (midpoint)
  ├── ILI9341_DrawChar / DrawString               ← Font5x7
  └── ILI9341_WritePixels                         ← raw pixel stream
         ↓
fonts.h
  └── Font5x7[95][5]                              ← 5x7 ASCII 0x20..0x7E
```

`ili9341.c` **tidak** mengakses register SoC langsung — semua
transaksi lewat hooks yang `main.c` sediakan. Ini menjaga driver
reusable kalau besok SPI-nya pindah channel.

---

## Demo

Empat scene berputar dalam `while(1)` (tiap scene dijeda ~1,5–2 detik):

1. **Color bands** — 8 warna dasar (RGB565) sebagai pita horizontal.
2. **Shapes** — empat rectangle, dua circle, dua garis diagonal.
3. **Text** — header + baris ASCII besar-kecil-angka-simbol, background
   biru solid.
4. **Pixel art border** — checker pattern di kiri, solid hijau di
   kanan, caption di bawah.

---

## Catatan delay.h

`delay(unsigned int ms)` di StarterWare menunggu overflow DMTimer7 lewat
IRQ (`Sysdelay()` → busy-wait flag `flagIsr`). Agar IRQ dapat
menyala, **kedua baris ini wajib dipanggil pertama kali di `main()`**:

```c
IntAINTCInit();         /* reset AINTC, seed vector table */
IntMasterIRQEnable();   /* clear I-bit di CPSR */
```

Setelah itu panggil `DelayTimerSetup()` (setelah SPI0 up) untuk
register `DMTimerIsr` ke slot IRQ Timer7. Lewati dua langkah ini dan
`delay()` hang selamanya. Pola ini mengikuti
`AM3352_GPIO_LED_DELAY` di workspace yang sama.

---

## Struktur File Project

```
AM3352_SPI_ILI9341/
├── AM335x.cmd              ← Linker cmd (DDR @ 0x80000000)
├── main.c                  ← SPI0 + GPIO + ILI9341 driver glue + demo
├── ili9341.c               ← Driver ILI9341 (ported dari STM32H7)
├── ili9341.h               ← Public API
├── fonts.h                 ← 5x7 ASCII font
├── README.md               ← File ini
└── targetConfigs/          ← CCS target config (J-Link + AM3352)
```

Library StarterWare dipakai **read-only** dari
`C:\ti\AM335X_StarterWare_02_00_01_01\...`. Tidak ada file di folder
StarterWare yang diubah atau dicopy ke project ini.

---

## Build & Run

Konfigurasi project sama persis dengan `AM3352_SPI_TX` baseline
(include path StarterWare, library `drivers.lib / system.lib /
platform.lib / utils.lib / libc.a`, linker cmd `AM335x.cmd`). Cukup
tambahkan dua file baru (`ili9341.c`, `fonts.h`) ke build (CCS
auto-pickup karena satu folder). Setelah build sukses:

1. Hubungkan SEGGER J-Link ke header JTAG board.
2. `Run → Debug` (F11). CCS load `.out` ke `0x80000000`.
3. Setelah CPU berjalan, LCD akan menampilkan empat scene demo
   berputar.

---

## Catatan Build (lesson learned)

Beberapa hal yang perlu diketahui saat menyentuh project ini:

- **C89 default**. TI CGT ARM di project StarterWare default ke C89
  (bukan C99/C11). Artinya **deklarasi variabel harus di awal blok**,
  tidak boleh di tengah setelah statement. Pola yang biasa kita tulis
  di gcc seperti:
  ```c
  for (uint16_t i = 0; i < N; i++) { ... }   // OK di gcc/clang, ERROR di CGT
  ```
  harus ditulis ulang jadi:
  ```c
  uint16_t i;        /* C89: declare at top of block */
  for (i = 0; i < N; i++) { ... }
  ```
  Begitu juga `static uint8_t buf[N]` atau `int16_t e2` yang sering
  muncul di tengah blok — semua dipindahkan ke atas blok. Lihat
  `ILI9341_DrawPixel`, `ILI9341_FillRect`, `ILI9341_DrawLine`,
  `ILI9341_DrawChar` untuk pola yang sudah konsisten.

- **Linker melihat symbol lintas file**. `Spi0TxByte` dan
  `Spi0TxBuffer` di `main.c` di-declare **tanpa `static`** supaya
  `ili9341.c` bisa extern-declare dan memanggilnya. `Spi0Init`
  tetap `static` karena tidak dipanggil dari file lain.

- **Trailing newline di header**. CGT ARM memunculkan warning
  `last line of file ends without a newline` untuk header tanpa `\n`
  di akhir. `ili9341.h` dan `fonts.h` sudah diakhiri newline.

---

## Catatan Driver

- **`ILI9341_DrawChar` re-set address window per kolom**. Setiap
  kolom 7-pixel di-isolasi dengan `ILI9341_SetAddressWindow()`
  tunggal-kolom sebelum push, supaya ILI9341 auto-increment tidak
  wrap ke kolom berikutnya di tengah glyph. Overhead: 5 transaksi
  register per karakter — tidak terasa di SPI 10 MHz.

- **CS per byte di `Spi0TxBuffer`**. Agak konservatif; lihat tuning
  di bawah untuk varian yang lebih cepat.

---

## Tuning Lebih Lanjut

- **Clock SPI**: ubah `MCSPI_OUT_FREQ` di `main.c`. 10 MHz adalah
  kompromi aman untuk kabel breadboard panjang ke LCD; untuk PCB
  pendek bisa naik ke 20–40 MHz (cek timing budget di datasheet
  ILI9341).
- **Throughput**: replace `Spi0TxBuffer()` dengan loop yang menahan
  CS asserted selama pixel stream aktif, lalu deassert di akhir
  fill — typical gain 2–4×.
- **Orientasi**: ubah argumen `ILI9341_CMD_MAC` di `ILI9341_Init()`
  (0x28 = landscape, 0x48 = portrait, 0xE8 = portrait flipped,
  0x18 = landscape flipped).
- **Tambah scene**: tulis fungsi `Demo_*()` baru di `main.c`, panggil
  dari loop utama.