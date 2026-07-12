# AM3352_SPI_ST7735

Project StarterWare portable yang **menjalankan panel LCD TFT ST7735 1.44"** (128×128 piksel, RGB565) lewat antarmuka SPI0 dari SoC AM3352 — di board Antminer L3+ (form-factor BeagleBone). Dibangun di atas scaffold `AM3352_SPI_TX` yang sudah terverifikasi transmit-byte-nya di osiloskop.

Tujuan project: jadi demonstrasi end-to-end bahwa sinyal SPI, GPIO (DC + RST), dan inisialisasi register ST7735 hidup di target AM3352 — tanpa perlu peripheral tambahan di luar panel LCD.

---

## Apa yang Dilakukan Program

Setelah boot, urutan di `main.c`:

1. **Enable clock GPIO0 & GPIO1**, pinmux **DC (P8_26)** dan **RST (P8_19)** ke mode GPIO (mode 7), set sebagai output, idle LOW.
2. **Enable McSPI0 functional clock**, pinmux SPI0 pads (P9_22/18/17), aktifkan master mode single-channel dengan D1 sebagai MOSI (`DATA_LINE_COMM_MODE_1`), 8-bit word, ~4 MHz, CPOL=0/CPHA=0, CS aktif-LOW (manual per-byte).
3. **Hardware-reset LCD**: RST LOW 50 ms → HIGH 50 ms (sesuai datasheet ST7735).
4. **ST7735_Init()** — mengirim urutan register lengkap lewat SPI:
   - `SWRESET` ×2 + `SLPOUT` + delay 120 ms,
   - frame-rate control (FRMCTR1/2/3),
   - power control (PWCTR1–5, VMCTR1),
   - line inversion (INVCTR=0x07),
   - inversion off (default untuk panel 1.44" ini),
   - pixel format RGB565 (`COLMOD=0x05`),
   - gamma positif & negatif (GMCTRP1/N1),
   - `NORON` + `TEAROFF` + `DISPON`,
   - `MADCTL=0xC8` (portrait + RGB order; bit3=RGB — kalau keliru, RED/BLUE/CYAN terlihat tertukar).
5. **Demo pattern** — replika dari `lcd.c::LCD_Test()` reference, **dengan backlight PWM dihilangkan** (AM3352 tidak punya TIM1 channel 2 terdedikasi untuk LCD backlight):
   - **Splash (2 detik)**: `ST7735_DrawBitmap(0, 0, logo_128_128)` — logo Texas Instruments 128×128 dari file `logo_128_128.c` (dump dari `logo.bmp`), lihat bagian [Membuat atau Mengganti Bitmap Splash](#membuat-atau-mengganti-bitmap-splash).
   - **Counter + progress bar (2 detik, 200 langkah × `delay(10)`)**:
     - Counter 3-digit (`000..099`) **putih** di pojok kanan-atas (X=94, Y=4), overlay di atas logo.
     - Progress bar **putih** di 3 baris terbawah (Y=125–127), dari 0 → 128 piksel.
   - **Info screen** (`FillScreen(BLACK)` kemudian loop forever): 4 baris teks di pojok kiri-atas:
     - Y=4 `Antminer L3+` (merah)
     - Y=20 `AM3352 / SPI0` (biru)
     - Y=36 `ST7735 1.44 LCD TFT` (cyan)
     - Y=52 `RGB565 OK` (hijau)
6. Loop forever — display dibiarkan menyala.

---

## Pemetaan Pin

### SPI0 (channel 0) — LCD

| Sinyal | Pin header | Pad control register | Offset | Fungsi |
|---|---|---|---|---|
| **CLK** | P9_22 | `CONTROL_CONF_SPI0_SCLK` | `0x950` | SPI0 clock (CPOL=0, CPHA=0) |
| **MOSI** | P9_18 | `CONTROL_CONF_SPI0_D1` | `0x958` | SPI0 data line 1 → output enabled oleh `MCSPI_DATA_LINE_COMM_MODE_1` |
| **CS** | P9_17 | `CONTROL_CONF_SPI0_CS0` | `0x95c` | Hardware chip-select 0, active-low, manual per transfer |

> **Penting.** MOSI ada di **P9_18 (D1)**, bukan P9_21 (D0). Probe MOSI selalu di P9_18 — kalau di P9_21, garis akan terlihat flat LOW meskipun CLK jalan.

### GPIO (LCD control)

| Sinyal | Pin header | SoC GPIO | Pad control register | Fungsi |
|---|---|---|---|---|
| **DC** | P8_26 | GPIO1[29] | `CONTROL_CONF_GPMC_CSN0` | Data/Command select untuk protokol ST7735 (LOW = command, HIGH = data) |
| **RST** | P8_19 | GPIO0[22] | `CONTROL_CONF_GPMC_AD8` | Reset pulse hardware untuk panel |

### Mode SPI

| Parameter | Nilai |
|---|---|
| Mode | Master, single-channel, transmit-and-receive |
| Word length | 8 bit |
| Clock rate | 4 MHz (configurable lewat `MCSPI_OUT_FREQ`) |
| CPOL / CPHA | Mode 0 (idle LOW, sample on rising edge) |
| CS polarity | Active-low |
| CS control | Manual per transfer via `McSPICSAssert` / `McSPICSDeAssert` |
| Data line comm mode | `MCSPI_DATA_LINE_COMM_MODE_1` (D1 output, D0 input) |
| FIFO | TX & RX enabled |

---

## Struktur File

```
AM3352_SPI_ST7735/
├── main.c          ← host glue: GPIO + SPI init, RST pulse, demo paint (LCD_Test pattern)
├── st7735.c        ← ST7735 1.44" driver (init + primitives + 5x7 font + DrawBitmap)
├── st7735.h        ← public driver API (command + data IO contract)
├── logo.bmp        ← source splash bitmap (128×128, BMP 16-bit RGB565, Windows format)
├── logo_128_128.c  ← splash bitmap dump dari logo.bmp (header BMP + pixel data sebagai const array)
├── AM335x.cmd      ← TI CGT linker command file
└── README.md
```

Driver ST7735 di sini adalah versi ramping dari referensi STM32 BSP di
`C:\D\MY\DEV\STM32CubeIDE\workspace_1.18.1\ST7735_1.44_WEACT_WB55CG\Drivers\BSP\ST7735`
— urutan register, frame-rate, power-control, gamma, dan nilai MADCTL/COLMOD dipertahankan, tapi semua callback HAL & struct IO dibuang. Driver hanya tahu tiga fungsi IO:

- `ST7735_WriteCommand(cmd)` — DC LOW + 1 byte SPI
- `ST7735_WriteData(buf, len)` — DC HIGH + N byte SPI
- `ST7735_DelayMs(ms)` — delay blocking kasar (~ms)

Tiga fungsi itu diimplementasikan di `main.c` di atas primitif McSPI/GPIO StarterWare.

`ST7735_DrawBitmap()` punya dua transformasi data yang penting:

1. **Byte-swap per-piksel.** File BMP 16-bit Windows menyimpan tiap piksel sebagai `[LSB, MSB]` (little-endian). ST7735 RAM write (CMD `0x2C`) menerima byte MSB-first. Setiap 2 byte di-swap sebelum dikirim ke LCD.
2. **Flip vertikal.** BMP menyimpan baris **bottom-up**, ST7735 menulis **top-down**. `DrawBitmap()` set window per-baris dan ambil baris dari `(height - 1 - row)` di BMP.

Tanpa dua transformasi itu, gambar akan berwarna krem/negatif dan terbalik 180°.

---

## Membuat atau Mengganti Bitmap Splash

File `logo_128_128.c` adalah dump byte-per-byte dari gambar BMP yang siap di-include ke source C. Array `logo_128_128[]` berisi **header BMP lengkap (54 byte)** lalu pixel data. `ST7735_DrawBitmap()` parse header langsung (offset, width, height, file size) untuk tahu di mana pixel data mulai dan ukurannya.

### Syarat gambar sumber

| Properti | Nilai yang dibutuhkan |
|---|---|
| Format file | **BMP** (Windows bitmap, uncompressed / `BI_RGB`) |
| Resolusi | **128 × 128 piksel** |
| Bit depth | **16-bit** |
| Color encoding | **RGB565** (R=5 bit, G=6 bit, B=5 bit) |
| Urutan byte per piksel | Little-endian (standar BMP) — di-swap sendiri oleh `DrawBitmap` |
| Urutan baris | Bottom-up (standar BMP) — di-flip sendiri oleh `DrawBitmap` |

Cara paling aman untuk dapat file BMP yang sesuai: buka gambar Anda di editor gambar (mis. GIMP, Photoshop, Paint.NET), lalu:

1. Crop / resize ke **128 × 128**.
2. Convert ke **Indexed 16-bit RGB565**. Di GIMP: **Image → Mode → Indexed** lalu re-export, atau pakai plugin **Export to BMP** dengan opsi bit-depth 16.
3. Save as `.bmp` (Windows BMP, uncompressed).

### Generate `logo_128_128.c` pakai Bin2C.exe

[`Bin2C.exe`](http://www.deje.dk/binary2c/) adalah utilitas Windows kecil (GUI, portable, no-install) yang mengonversi file biner arbitrary menjadi array `const unsigned char name[] = { 0x.., 0x.., ... };` siap include ke C.

Langkah-langkah:

1. Siapkan gambar sumber 128×128 16-bit BMP sesuai tabel di atas, simpan mis. sebagai `logo.bmp`.
2. **Double-click** `Bin2C.exe` — dialog GUI akan terbuka.
3. Di dialog, klik tombol pilih file, arahkan ke `logo.bmp`, lalu klik **Convert** (atau tombol serupa). Output C akan tergenerate di tempat yang ditentukan dialog.
4. Rename / save output sebagai `logo_128_128.c` di folder project (overwrite yang lama). Pastikan nama variabel array dalam output cocok dengan `extern const unsigned char logo_128_128[];` di `main.c` — bila Bin2C memberi nama berbeda (mis. `logo_128_128_data`), rename deklarasi di output atau edit extern di `main.c` agar sinkron.
5. Rebuild CCS project. Tidak perlu ubah `main.c` maupun `st7735.c` selama nama variabel tetap cocok.

### Verifikasi setelah ganti bitmap

Logo akan muncul benar apabila:
- Piksel tampil di orientasi yang benar (tidak terbalik): byte-swap + row-flip di `DrawBitmap` sudah menangani ini untuk BMP standar.
- Warna sesuai: bila panel Anda kebetulan butuh `MADCTL=0xC0` (BGR) ganti bit 3 di `st7735.c::madctl_value`. Board Antminer L3+ yang dipakai di sini butuh `0xC8`.

---

Boot sequence di `main()`: **`IntAINTCInit()` → `IntMasterIRQEnable()` → `DelayTimerSetup()`** dulu (sebelum inisialisasi GPIO/SPI apa pun). Ketiganya wajib karena `delay()` (StarterWare `utils/delay.c` + `platform/beaglebone/sysdelay.c`) menunggu overflow DMTimer7 lewat IRQ — tanpa AINTC di-reset dan I-bit di-clear di CPSR, IRQ Timer7 tidak pernah masuk dan `delay()` stuck di `while(flagIsr == FALSE)`. Pola ini mengikuti `examples/evmAM335x/dmtimer/dmtimerCounter.c` (StarterWare reference), identik dengan project `AM3352_GPIO_LED_DELAY` dan `AM3352_ADC`.

> **Troubleshooting `delay()` hang** (gejala umum dari project `AM3352_ADC`): kalau urutan init IRQ dibalik, debugger akan hang begitu `IntMasterIRQEnable()` dipanggil atau PC loncat ke alamat ngaco di OCMC. Selalu: `IntAINTCInit()` → `IntMasterIRQEnable()` → **`DelayTimerSetup()`** → inisialisasi peripheral lain → baru `delay()` pertama.

---

## API Driver yang Tersedia

| Fungsi | Kegunaan |
|---|---|
| `ST7735_Init()` | Jalankan init sequence panel (panggil setelah RST pulse). |
| `ST7735_SetWindow(x0,y0,x1,y1)` | Pilih window piksel, mulai state tulis RAM. |
| `ST7735_SetPixel(x,y,color)` | Tulis satu piksel (lambat — pakai FillRect untuk area besar). |
| `ST7735_DrawHLine / DrawVLine` | Gambar garis. |
| `ST7735_FillRect(x,y,w,h,color)` | Isi blok piksel solid. |
| `ST7735_FillScreen(color)` | Alias untuk `FillRect` full panel. |
| `ST7735_DrawChar(x,y,c,fg,bg,size)` | Render satu karakter ASCII (5×7, size = pixel-per-dot). |
| `ST7735_DrawString(x,y,s,fg,bg,size)` | Render string ASCII. |
| `ST7735_DrawBitmap(x,y,bmp)` | Render BMP file yang sudah di-dump ke const array (header BMP lengkap, little-endian, RGB565). |

Warna RGB565 tersedia sebagai makro (`ST7735_BLACK`, `ST7735_RED`, `ST7735_GREEN`, `ST7735_BLUE`, `ST7735_WHITE`, `ST7735_CYAN`, `ST7735_MAGENTA`, `ST7735_YELLOW`).

---

## Cara Verifikasi di Target

1. **Scope/LA di pin header** (urutan probe disarankan):
   - CH1 → P9_17 (CS): pulsa LOW sepanjang tiap burst transfer.
   - CH2 → P9_22 (CLK): 8 pulsa per byte, ~250 ns periode @ 4 MHz.
   - CH3 → P9_18 (MOSI): terlihat bit-bit data command/data.
   - CH4 → P8_26 (DC): LOW selama command, HIGH selama data burst.
   - CH5 → P8_19 (RST): pulsa LOW → HIGH di awal program.

2. **Visual di panel LCD** (128×128) — tiga fase berurutan:
   - **Splash (2 detik)**: logo Texas Instruments 128×128 (warna merah "TI" + teks "Texas Instruments" di bawah).
   - **Counter + bar (2 detik)**: counter 000..099 persen di pojok kanan-atas (putih di atas logo), bar putih tumbuh dari kiri di 3 baris terbawah.
   - **Info screen (selamanya)**: layar hitam dengan 4 baris teks di pojok kiri-atas — `Antminer L3+` (merah), `AM3352 / SPI0` (biru), `ST7735 1.44 LCD TFT` (cyan), `RGB565 OK` (hijau).

3. **Kalau layar putih polos/blank**:
   - Cek CS hold-time — burst harus di-deassert setelah MCSPI_CH0STAT_TXS.
   - Cek MOSI ada di P9_18 (D1), bukan P9_21 (D0).
   - Cek backlight LCD (kalau panel WEAct 1.44" butuh LED pin tersendiri, default tanpa backlight kadang terlihat sangat redup — masih bisa dilihat di kondisi remang).

---

## Context Hardware

| Item | Value |
|---|---|
| SoC | TI AM3352 (Cortex-A8, little-endian, ARMv7-A) |
| Board | Antminer L3+ (PCB & form-factor = BeagleBone, tanpa PRU) |
| McSPI0 functional clock | 48 MHz dari L4 interconnect |
| Debug probe | SEGGER J-Link |
| LCD panel | ST7735 1.44" 128×128, RGB565 |

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

1. **Project name**: `AM3352_SPI_ST7735`.
2. **Location**: biarkan default.
3. **Connection**: **SEGGER J-Link Emulator** (sesuai probe yang dipakai).
   Asumsinya sebuah shared global target configuration `AM3352_bb_shared_target_configuration.ccxml`
   sudah ada di user-level config (bukan di dalam folder project). Field target configuration
   boleh dikosongkan / pilih dari dropdown referensi ke config global. **JANGAN** centang
   *"Create new target configuration"* — kita pakai yang global.
4. **Project type**: `Cortex A.AM3352`.
5. **Project templates and examples**: pilih `☑ Empty Project (with main.c)` → auto-generate `main.c` kosong.
6. **Toolchain**: **TI v20.2.7.LTS**.
7. **Finish**.

### Include Path StarterWare

**Properties → Build → Arm Compiler → Include Options**, tambahkan:
```
${PROJECT_ROOT}
${CG_TOOL_ROOT}/include
C:/ti/AM335X_StarterWare_02_00_01_01/include
C:/ti/AM335X_StarterWare_02_00_01_01/include/hw
C:/ti/AM335X_StarterWare_02_00_01_01/include/armv7a
C:/ti/AM335X_StarterWare_02_00_01_01/include/armv7a/am335x
```

### Linker Command File

**Properties → General → Linker command file**: `AM335x.cmd`.

### Link Library StarterWare

**Properties → Build → Arm Linker → File Search Path**, tambahkan ke **Libraries**:
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
C:/ti/AM335X_StarterWare_02_00_01_01/binary/armv7a/cgt_ccs/am335x/drivers/Debug
C:/ti/AM335X_StarterWare_02_00_01_01/binary/armv7a/cgt_ccs/am335x/system_config/Debug
C:/ti/AM335X_StarterWare_02_00_01_01/binary/armv7a/cgt_ccs/am335x/beaglebone/platform/Debug
C:/ti/AM335X_StarterWare_02_00_01_01/binary/armv7a/cgt_ccs/utils/Debug
```

### Pre-define

**Properties → Build → Arm Compiler → Pre-define NAME**: `am3352`.

---

## Build & Flash

### Build

1. Pilih **Debug** di toolbar.
2. **Project → Build All** (`Ctrl+B`).
3. Output `.out` di `${workspace_loc:/AM3352_SPI_ST7735/Debug/AM3352_SPI_ST7735.out}`.

### Debug / Flash via J-Link

1. Hubungkan J-Link ke header JTAG board.
2. Pilih target configuration global yang sesuai.
3. **Run → Debug** (`F11`) — CCS load `.out` ke 0x80000000 (DDR) dan PC mulai dari `Entry`.
