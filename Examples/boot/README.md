# boot (StarterWare → Portable CCS Project)

Project portable hasil copy dari example **StarterWare** Texas Instruments (versi **AM335X_StarterWare_02_00_01_01**) untuk board **BeagleBone (AM335x)**. Contoh `boot` adalah **secondary bootloader StarterWare** — bukan bootloader level ROM, melainkan program ARM Cortex-A8 yang lengkap yang di-load oleh bootloader level ROM dari MMC/SD (atau UART XMODEM) dan berfungsi sebagai **boot stage 2**: inisialisasi PLLs, DDR, card detect, lalu menyalin image aplikasi `app` dari SD card ke RAM dan jump ke entry point aplikasi.

Folder project ini berdiri sendiri **di luar** StarterWare installation. StarterWare berfungsi murni sebagai **library driver + include path**, sementara seluruh source `boot/*` (file `.c` dan `.asm` bootloader) hidup di folder ini dan di-build via Code Composer Studio.

## Deskripsi Singkat

- **Target device**: AM335x (Cortex-A8, little-endian)
- **Toolchain**: TI Code Composer Studio (CCS) + TI ARM CGT **v20.2.7.LTS**
- **Output binary**: `C:/ti/AM335X_StarterWare_02_00_01_01/binary/armv7a/cgt_ccs/am335x/beaglebone/bootloader/Debug_MMCSD/boot.out` + `boot.bin` + `boot_ti.bin`
- **Linker command file**: `boot.cmd`
- **Build configuration**: `Debug_MMCSD` (satu-satunya konfigurasi di project ini — output `_ti.bin` dengan header `0x402F0400 MMCSD`)

### Source File Lokal (sebelumnya dari StarterWare `bootloader/src/` + `bootloader/src/armv7a/am335x/`)

| Layer | File | Peran |
|---|---|---|
| Entry (asm) | `bl_init.asm` | Reset vector, vektor IRQ/FIQ, inisialisasi stack IRQ |
| Entry (asm) | `cp15.asm` | Enable I-cache, D-cache, branch predictor, TTB setup |
| Entry (C) | `bl_main.c` | `main()` bootloader: panggil init PLL → DDR → HS MMCSD → `HSMMCSDImageCopy()` → jump ke entry |
| Init | `bl_platform.c` | Pohon inisialisasi PLL (`PLLInit`) & DDR (`DDRInit`); board-specific init `DDRVTP` |
| Init | `bl_uart.c` | `UARTAccessible()` (trivial untuk debug) |
| Init | `cache.c` | `CacheEnable`/`CacheFlush` di DCache/ICache |
| Init | `mmu.c` | Tabel page MMU 4096 byte granularity, `MMUEnable` |
| Init | `device.c` | Helper baca ID silicon AM335x |
| MMCSD | `bl_hsmmcsd.c` | Driver MMCSD bawaan StarterWare: `HSMMCSDFsMount`, `HSMMCSDImageCopy`, `HSMMCSDFatInit`, `HSMMCSDReadFile` |
| MMCSD | `bl_copy.c` | Wrapper XMODEM via UART (`XMODEMImageCopy`) dan helper `HSMMCSDImageCopy` |
| MMCSD | `fat_mmcsd.c`, `ff.c` | FatFs R0.07e (Chan) yang di-bundle ke StarterWare untuk FS MMCSD |
| Umum | `crc16.c` | CRC16 untuk image validity check |
| Umum | `xmodem.c` | XMODEM receiver untuk fallback loading via UART |
| Porting | `board.c` | Override `BoardInfoInit`, `BoardNameGet`, dll — stub untuk hardware tanpa EEPROM |

Total ~6.6K baris kode ARM + assembler hidup di folder project ini.

### Library StarterWare yang Digunakan (link-time)

Beberapa fungsi router di `bl_platform.c` dan `bl_main.c` resolve simbol dari library pre-built StarterWare di:

```
C:/ti/AM335X_StarterWare_02_00_01_01/binary/armv7a/cgt_ccs/am335x/beaglebone/
└── lib/
    ├── platform.lib        ← GPMC, pin-mux, I2C, MMCSD register init
    ├── mmcsd.lib           ← MMCSD low-level controller driver
    ├── device.lib          ← AM335x silicon ID
    ├── utils.lib           ← crc32, bitfield helpers, snprintf/printf
    └── system.lib          ← int APIs, abort handler
```

(Lokasi file `.lib` di StarterWare asli: `binary/armv7a/cgt_ccs/am335x/beaglebone/` atau `binary/armv7a/cgt_ccs/mmcsdlib/`.)

### Include Path yang Dipakai

Semua include header StarterWare dipull lewat path absolut hardcoded:

- `C:/ti/AM335X_StarterWare_02_00_01_01/include` — header SoC AM335x
- `C:/ti/AM335X_StarterWare_02_00_01_01/include/armv7a/` — asm register struct
- `C:/ti/AM335X_StarterWare_02_00_01_01/include/hw` — `hw_*.h` register layout
- `C:/ti/AM335X_StarterWare_02_00_01_01/include/armv7a/am335x` — `soc_AM335x.h`, `hw_control_AM335x.h`
- `C:/ti/AM335X_StarterWare_02_00_01_01/bootloader/include` — `bl_copy.h` (struktur `ti_header`), `bl_rprc.h`, `bl_uart.h`
- `C:/ti/AM335X_StarterWare_02_00_01_01/bootloader/include/armv7a/am335x` — pin-mux helpers
- `C:/ti/AM335X_StarterWare_02_00_01_01/mmcsdlib/include` — driver MMCSD header
- `C:/ti/AM335X_StarterWare_02_00_01_01/third_party/fatfs/src` — FatFs `.c` source path

## Mode Booting

Project ini menghasilkan bootloader **MMCSD mode**. Boot sequence AM335x dengan `boot_ti.bin`:

```
ROM Bootloader (on-chip, immutable)
  │
  ├── SysBoot pin config → MMC/SD boot selected
  ├── Pindahkan `boot_ti.bin` dari SD card ke SRAM internal (0x402F0400)
  │
  ▼
boot_ti.bin (StarterWare secondary bootloader ini)
  │
  ├── bl_init.asm: setup SP_irq, branch ke _main
  ├── bl_main.c → main():
  │     ├── SystemInit() → PLLInit + DDRInit (DDR siap)
  │     ├── UART0 init (log pesan)
  │     ├── CacheEnable / MMUEnable
  │     ├── MMCSDInit() + MMCSDCardPresent() (cek SD card)
  │     ├── HSMMCSDImageCopy() ─► baca "/app" dari SD card (FAT)
  │     │     ├── baca 8-byte ti_header (size, load_addr)
  │     │     ├── copy payload → DDR @ load_addr
  │     ├── Jump ke entry point (load_addr)
  │     └── ...
  ▼
app (aplikasi yang Anda siapkan di SD card)
  │
  ├── Lihat "Menyiapkan Aplikasi di SD Card" di bawah
```

**Output UART log saat boot sukses di SD card** (115200 8N1):

```
StarterWare AM335x Boot Loader
Copying application image from MMC/SD card to RAM
Jumping to StarterWare Application...
```

## Cara Pakai

Project ini adalah project CCS yang sudah di-ekstrak ke folder ini, jadi tidak perlu import wizard — cukup buka langsung di CCS.

1. Buka **Code Composer Studio**.
2. Pilih workspace `C:\D\MY\DEV\TI-CCS-IDE\Workspace_12\`.
3. Jika `boot` belum muncul di Project Explorer, import manual:
   - **File → Import… → CCS Projects**
   - **Select root directory**: arahkan ke folder `boot` di workspace ini, klik **Finish**.
4. Pilih project `boot` di Project Explorer.
5. **Project → Clean…** untuk memastikan output bersih, lalu **Project → Build Project** (`Ctrl+B`) untuk compile.
6. Setelah build sukses:
   - File `boot_ti.bin` bisa langsung di-load ke SRAM AM335x via debugger (XDS100/XDS200) untuk booting sekali pakai.
   - Untuk deployment permanen, flash ke SD card `/MLO` atau bootloader lainnya.

## Yang Berubah dari Versi StarterWare Asli

File asli `boot` di StarterWare (`C:/ti/AM335X_StarterWare_02_00_01_01/bootloader/`) di-generate oleh CCS import wizard dan me-refer banyak path dengan `${ORIGINAL_PROJECT_ROOT}/../../../../../../../...` (relatif terhadap build artifact directory). Karena project ini di-copy ke workspace terpisah, semua path tsb harus di-hardcode absolut.

### Perubahan `.cproject`

- **Include path**: 8 path `${ORIGINAL_PROJECT_ROOT}/../../../../../../../...` diganti ke absolut `C:/ti/AM335X_StarterWare_02_00_01_01/...` (pertain ke include header, FatFs source, library include).
- **Library path**: 3 path `${ORIGINAL_PROJECT_ROOT}/../../../../../../../...` untuk `.lib` diganti absolut ke `C:/ti/AM335X_StarterWare_02_00_01_01/binary/armv7a/cgt_ccs/...` (platform.lib, mmcsd.lib, device.lib, utils.lib, system.lib).
- **Output binary path** (`Debug_MMCSD/boot.out` + `_ti.bin`) → absolut `C:/ti/.../binary/armv7a/cgt_ccs/am335x/beaglebone/bootloader/Debug_MMCSD/`.
- **Post-build step** (`tiobj2bin.bat` + `tiimage.exe`) masih pakai form `../../../../../../../...` **relatif**, karena `<builder>` di `.cproject` line 16 bekerja di working directory `boot/`. CCS resolve `${ProjName}` relatif terhadap working directory build, jadi relative path di post-build step bisa tetap dipakai. **Untuk konsistensi jika porting path, lebih aman ubah juga ke absolut** — catatan ini hanya supaya tidak ada kejutan saat beberapa path terlihat berbeda style.
- **Tambahan link-order**: `device.lib, mmcsd.lib` ditambah eksplisit agar urutan link dapat diprediksi.
- **Compiler version**: `OPT_CODEGEN_VERSION` diubah dari `5.0.4` ke `20.2.7.LTS`.

### Perubahan `.project`

- `<linkedResources>` StarterWare asli berisi 9 `<link>` ke `PARENT-6-ORIGINAL_PROJECT_ROOT/...` yang menunjuk ke file `.c`/`.asm` StarterWare. **9 link tsb dihapus** — semua file sumber sekarang hidup di folder project ini (lihat bagian berikutnya).
- Yang tersisa: 1 linked artifact `Debug_MMCSD/boot.out` (mengikuti pola `gpioLEDBlink` — link ke external build artifact hasil linker).

### File Sumber yang Di-copy ke Lokal

Semua source StarterWare yang sebelumnya linked sekarang berada di folder project ini:

```
bl_init.asm          ← reset/IRQ vector
cp15.asm             ← I/D-cache, MMU TTBR0 setup
bl_main.c            ← entry point bootloader
bl_platform.c        ← inisialisasi PLL/DDR/peripheral
bl_uart.c            ← UART0 helper
board.c              ← override BoardInfo, BoardNameGet, dll
cache.c              ← DCache/ICache flush & enable
mmu.c                ← MMU page table & enable
crc16.c              ← CRC16 image integrity
device.c             ← DEVICE ID register reader (memanggil device.lib)
xmodem.c             ← UART XMODEM receiver
bl_hsmmcsd.c         ← StarterWare MMCSD driver wrapper
bl_copy.c            ← image copy wrapper (MMCSD + XMODEM)
fat_mmcsd.c          ← fat_mmcsd glue (StarterWare)
ff.c                 ← FatFs R0.07e
```

Dengan semua sumber ini di lokal, project **self-contained**: source code lengkap untuk build ulang berjalan tanpa copy tambahan dari StarterWare tree.

### Library yang Tetap Di-reference dari StarterWare (link time)

Tidak semua implement bisa dipindah ke lokal — beberapa mungkin tertanam dalam library jadi lebih efisien. Daftar `.lib` yang tetap dipakai:

```
platform.lib    ← GPMC, pin-mux, I2C init
mmcsd.lib       ← MMCSD register-level controller driver
device.lib      ← AM335x silicon ID reader
utils.lib       ← generic helpers (CRC32, bit ops, printf)
system.lib      ← low-level int handlers
```

Library tetap di `C:/ti/AM335X_StarterWare_02_00_01_01/binary/armv7a/cgt_ccs/am335x/beaglebone/lib/` — pastikan library tsb sudah ada sebelum build. Kalau tidak, perlu build StarterWare terlebih dulu via makefile di `C:/ti/AM335X_StarterWare_02_00_01_01/build/armv7a/cgt_ccs/`.

### File Lokal Hasil Porting untuk Hardware Antminer L3+

Project ini awalnya ditargetkan ke BeagleBone Black. Untuk hardware **Antminer L3+** (AM3352, tanpa EEPROM AT24), fungsi board-aware perlu di-override. Override disimpan sebagai file lokal di project, **TIDAK** mengubah sumber StarterWare:

- **`board.c`** — override `BoardInfoInit`, `BoardNameGet`, `BoardVersionGet`, `BoardIdGet` menjadi stub. Mengembalikan `("Antminer L3+", "1.0", BOARD_UNKNOWN)` tanpa transaksi I2C ke EEPROM AT24. Ini penting karena kalau bootloader panggil `BoardInfoInit` asli, dia gagal di I2C stick dengan `AT24` di address `0x50` karena Antminer tidak punya chip itu di address tsb → boot hang.

## Menyiapkan Aplikasi di SD Card

Setelah bootloader di-load ke board (lewat debugger, atau flash permanen), Anda memerlukan **binary aplikasi** bernama `app` di root SD card agar bootloader bisa boot MMCSD ke aplikasi. Format file `app`:

```
offset 0   uint32 image_size    ← ukuran total file `app` (little-endian)
offset 4   uint32 load_addr     ← alamat DDR tujuan (umumnya 0x80000000)
offset 8   uint8[] payload      ← image ARM mentah (raw binary) sebanyak (image_size - 8)
```

Header ini cocok dengan struktur `ti_header` di StarterWare `bl_copy.h`. Bootloader:
1. Buka file `"/app"` dari filesystem FAT di SD card.
2. Baca 8-byte header untuk `image_size` dan `load_addr`.
3. Copy `image_size - 8` byte payload ke DDR mulai `load_addr`.
4. Jump ke `load_addr` sebagai entry point aplikasi.

**Cara termudah**: build salah satu project demo StarterWare (mis. `gpioLEDBlink` atau `demo`) — post-build step StarterWare menghasilkan file `_ti.bin` yang sudah sesuai format ini. Copy ke root SD card:

```
sdcard_root/
└── app          ← (nama persis tanpa extension)
```

Misalnya, dari build `gpioLEDBlink`:
1. Setelah **Project → Build Project** di project `gpioLEDBlink` mode Release, ambil file:
   ```
   C:\ti\AM335X_StarterWare_02_00_01_01\binary\armv7a\cgt_ccs\am335x\beaglebone\gpio\Release\gpioLEDBlink_ti.bin
   ```
2. Copy ke root SD card.
3. Rename `gpioLEDBlink_ti.bin` menjadi `app` (tanpa extension).
4. Pasang SD card ke board, nyalakan — bootloader akan copy `app` ke DDR @ `0x80000000` lalu jump ke sana.

**Verifikasi MMCSD boot sudah benar** (kami sudah melakukan ini di Antminer L3+):
- UART0 di 115200 8N1 akan menampilkan:
  ```
  StarterWare AM335x Boot Loader
  Copying application image from MMC/SD card to RAM
  Jumping to StarterWare Application...
  ```
- Jika tiga baris tsb muncul lalu aplikasi tidak berkedip LED, aplikasi memang sampai di-DDR dan re-ekskusi, tapi mungkin sedang memanggil BoardInfoInit (override dengan stub di folder ini) atau ada app-side issue — bukan bootloader.

## Build Outputs

Path absolut output setelah **Project → Build Project** di CCS (build configuration `Debug_MMCSD`):

```
C:\ti\AM335X_StarterWare_02_00_01_01\binary\armv7a\cgt_ccs\am335x\beaglebone\bootloader\Debug_MMCSD\
├── boot.out         ← ELF (debugger)
├── boot.bin         ← raw ARM binary
├── boot_ti.bin      ← ti_header + raw binary, header text "0x402F0400 MMCSD" ← ARTIKEL UTAMA untuk booting
├── boot_linkInfo.xml
├── boot.map         ← linker map
├── makefile
├── bl_*.obj, board.obj, cache.obj, ...
└── ccsObjs.opt
```

## Catatan

- Project ini adalah salinan dari `bootloader/` StarterWare Texas Instruments, sehingga license-nya mengikuti BSD-style TI yang ada di header setiap file sumber.
- Folder `Debug_MMCSD/` (cache build) boleh dihapus secara periodik jika ingin clean build dari nol. Toolchain CGT 20.2.7 kadang menyimpan `*.obj` cache yang stale saat konfigurasi include berubah.
- Path absolut pakai separator Windows (`C:/ti/...`) dengan forward slash — sesuai format include path di toolchain CGT. Jangan diubah ke backslash.
- Beberapa dependency build JTAG ke `boot.out` diasumsikan via `Debug_MMCSD/boot.out` lewat `EXTERNAL_BUILD_ARTIFACT`. Kalau Anda mengubah nama konfigurasi, link ini perlu di-update manual.
- Bootloader ROM AM335x membaca `MLO` (Master Loader Object) terlebih dahulu; kalau `boot_ti.bin` ingin dijalankan langsung tanpa `MLO`, perlu rename dan letakkan di awal partisi SD card dengan nama `MLO`. Untuk booting sekunder via `MLO` (yang bisa Melewati inisialisasi PLL/DDR jika MLO sudah menanganinya), cukup letakkan `boot_ti.bin` dengan nama apapun `MLO` referensi.
