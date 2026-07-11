# demo (StarterWare → Portable CCS Project)

Project portable hasil copy dari example **StarterWare** Texas Instruments (versi **AM335X_StarterWare_02_00_01_01**) untuk board **BeagleBone (AM335x)**. Contoh `demo` adalah demo utama StarterWare yang menampilkan menu interaktif lewat UART dengan sub-demo GPIO, Timer, MMCSD, WWW (HTTP server), RTC, dan Power Management.

Berbeda dari `gpioLEDBlink`, project ini adalah **demo bundle besar** yang menginkludifikasi semua driver StarterWare (GPIO, I2C, MMCSD, ENET, RTC, DMTimer, UART, WDT) dan middleware (lwIP 1.4.0, FatFs, raw HTTP server). Folder project ini berdiri sendiri di luar StarterWare installation.

## Deskripsi Singkat

- **Target device**: AM335x (Cortex-A8, little-endian)
- **Toolchain**: TI Code Composer Studio (CCS) + TI ARM CGT **v20.2.7.LTS**
- **Output binary**: `Debug/demo.out` + `Debug/demo.bin` + `Debug/demo_ti.bin`
- **Linker command file**: `demo.cmd`
- **Source code utama**: `demoMain.c` (interaksi menu), `demoGpio.c`, `demoTimer.c`, `demoRtc.c`, `demoSdFs.c`, `demoSdRw.c`, `demoDvfs.c`, `demoPwrMgmnt.c`, `demoUart.c`, `demoI2c.c`, `demoEnet.c`, `demoLedIf.c`
- **Header lokal** (sebelumnya hanya tersedia di StarterWare `examples/beaglebone/demo/`): `demoMain.h`, `demoDvfs.h`, `demoEnet.h`, `demoGpio.h`, `demoI2c.h`, `demoLedIf.h`, `demoPwrMgmnt.h`, `demoRtc.h`, `demoSdFs.h`, `demoSdRw.h`, `demoTimer.h`, `demoUart.h`, `demoCfg.h`, `cm3image.h`, `cm3wkup_proxy.h`, `pm33xx.h`, `pmic.h`, `lwipopts.h`
- **Driver StarterWare lokal**: `am335x_clock_data.c`, `cm3wkup_proxy.c`, `pmic.c`
- **Middleware lokal**: `fat_mmcsd.c`, `ff.c` (FatFs), `httpd_io.c` (HTTP server), `lwiplib.c` (lwIP porting + bundled stack sources)

Setelah boot via JTAG lalu `go`, di UART0 muncul menu:

```
 Select/Execute/Goto:
  0. WWW
  1. RTC demo
  2. GPIO demo
  3. Timer demo
  4. MMCSD info
  5. PM demo
```

Tiap item menggerakkan sub-demo masing-masing.

## Cara Pakai

Project ini adalah project CCS yang sudah di-ekstrak ke folder ini, jadi tidak perlu import wizard — cukup buka langsung di CCS.

1. Buka **Code Composer Studio**.
2. Pilih workspace tempat folder `demo` ini berada (atau workspace yang berisi folder ini).
3. Jika project belum muncul di **Project Explorer**, lakukan import manual:
   - **File → Import… → CCS Projects**
   - Pilih **Search projects in the workspace** atau arahkan **Select root directory** ke folder `demo` ini, lalu klik **Finish**.
4. Pilih project `demo` di Project Explorer.
5. **Project → Clean…** untuk memastikan output build bersih, lalu **Project → Build Project** (atau `Ctrl+B`) untuk compile.
6. Setelah build sukses, `Debug/demo.out` siap di-load ke BeagleBone via debugger (misal JTAG/XDS).

## Yang Berubah dari Versi StarterWare Asli

File asli `demo` di StarterWare di-generate oleh CCS import wizard dan me-refer banyak path dengan `${ORIGINAL_PROJECT_ROOT}/../../../../../../../` (relatif terhadap build artifact directory). Karena project ini di-copy ke workspace terpisah, semua path tsb harus di-hardcode absolut.

### Perubahan `.cproject`

- **Include path**: `${ORIGINAL_PROJECT_ROOT}/../../../../../../../...` diganti `C:/ti/AM335X_StarterWare_02_00_01_01/...` untuk seluruh sub-tree (lwIP, FatFs, MMCSD, beaglebone header, `include/hw`, `include/armv7a`, dll). Catatan: include path `C:/ti/.../examples/beaglebone/demo` sudah dihapus sepenuhnya setelah 18 header demo di-copy ke lokal — project tidak bergantung lagi pada StarterWare source tree untuk headers sub-demo.
- **Library path** (drivers.lib, system.lib, platform.lib, utils.lib, libmmcsd.lib) → absolute `C:/ti/AM335X_StarterWare_02_00_01_01/binary/armv7a/cgt_ccs/...`.
- **Output binary path** (`Debug/demo.out`, `Debug/demo.bin`, `Debug/demo_ti.bin`) → absolute `C:/ti/.../binary/armv7a/cgt_ccs/am335x/beaglebone/demo/Debug/`.
- **Post-build step** (`tiobj2bin`, `tiimage.exe`) → absolute path.
- **Tambahan include path**: `C:/ti/AM335X_StarterWare_02_00_01_01/third_party/lwip-1.4.0/ports/cpsw` (tanpa `/include`) agar `lwiplib.c` dapat menemukan `locator.c` lewat `#include "locator.c"`.
- **Compiler version**: `OPT_CODEGEN_VERSION` diubah dari `5.0.4` ke `20.2.7.LTS`.

### Perubahan `.project`

- 20 `<linkedResources>` ke `PARENT-6-ORIGINAL_PROJECT_ROOT/...` (untuk 20 file .c/.asm StarterWare) dihapus; sebagai gantinya file sumber di-copy ke lokal (lihat bagian berikut). Yang tersisa hanya 2 linked artifact (`Debug/demo.out`, `Release/demo.out`) seperti pola `gpioLEDBlink`.

### File Sumber yang Di-copy ke Lokal

Semua source StarterWare yang sebelumnya linked sekarang berada di folder project ini:

```
am335x_clock_data.c    cm3wkup_proxy.c     demoDvfs.c           demoEnet.c
demoGpio.c             demoI2c.c            demoLedIf.c          demoMain.c
demoPwrMgmnt.c         demoRtc.c            demoSdFs.c           demoSdRw.c
demoTimer.c            demoUart.c           fat_mmcsd.c          ff.c
httpd_io.c             lwiplib.c            pmic.c               slpWkup_cgttms470.asm
```

Bersamaan dengan source .c di atas, 18 file header yang tadinya hanya ada di StarterWare `examples/beaglebone/demo/` juga di-copy ke lokal sehingga `.c` lokal bisa resolve `#include "demoMain.h"` dll tanpa path StarterWare. Daftar header lokal: `demoMain.h`, `demoDvfs.h`, `demoEnet.h`, `demoGpio.h`, `demoI2c.h`, `demoLedIf.h`, `demoPwrMgmnt.h`, `demoRtc.h`, `demoSdFs.h`, `demoSdRw.h`, `demoTimer.h`, `demoUart.h`, `demoCfg.h`, `cm3image.h`, `cm3wkup_proxy.h`, `pm33xx.h`, `pmic.h`, `lwipopts.h`.

`lwiplib.c` adalah sumber lwIP yang me-bundle via `#include` semua file source lwIP (`src/api/*.c`, `src/core/*.c`, `src/netif/*.c`, `ports/cpsw/*.c` seperti `perf.c`, `sys_arch.c`, `cpswif.c`, `locator.c`). File .c tambahan ini **tidak** di-copy ke lokal — mereka di-resolve oleh compiler lewat include path `third_party/lwip-1.4.0/ports/cpsw` yang sudah ditambah.

### File Lokal Hasil Porting untuk Hardware yang Berbeda

Project ini awalnya ditargetkan ke BeagleBone Black. Untuk hardware **Antminer L3+** (AM3352, no AT24 EEPROM, TPS65217 PMIC behaviour berbeda), beberapa fungsi StarterWare butuh di-override. Override disimpan sebagai file lokal di project, **TIDAK** mengubah sumber StarterWare:

- **`board.c`** — override `BoardInfoInit`, `BoardNameGet`, `BoardVersionGet`, `BoardIdGet` menjadi stub. Mengembalikan `("Antminer L3+", "1.0", BOARD_UNKNOWN)` tanpa transaksi I2C ke EEPROM AT24.
- **`pmic.c`** — `configVddOpVoltage` dan `setVdd1OpVoltage` dijadikan no-op (tidak melakukan I2C write ke TPS65217). PMIC Antminer L3+ berbeda dan tidak kompatibel dengan urutan init BeagleBone.
- **`dmtimer.c`** — `DMTimer1msModuleClkConfig_Local` (nama di-rename agar tidak konflik dengan `platform.lib`) adalah versi non-blocking dari `DMTimer1msModuleClkConfig`. Versi StarterWare melakukan `while` polling pada register status PRCM (`CM_WKUP_TIMER1_CLKCTRL.IDLEST`) yang tidak settle pada clock Antminer L3+, menyebabkan board hang. Patch di `demoTimer.c` line 334 menggunakan versi `_Local` ini.
- **`demoTimer.c` line 334** — pemanggilan diubah dari `DMTimer1msModuleClkConfig(4)` ke `DMTimer1msModuleClkConfig_Local(4)`.

## Hasil di Hardware Antminer L3+

Menu `demo` saat dijalankan di Antminer L3+ (yang form-factor-nya mirip BeagleBone Black tapi tanpa EEPROM):

| Pilihan | Status | Keterangan |
|---|---|---|
| 0. WWW | **Jalan** | HTTP server lwIP tayang halaman web default |
| 1. RTC demo | Parsial | Input RTC muncul, tapi nilai waktu tidak tertampil (sama dengan behavior di project `rtcClock`) |
| 2. GPIO demo | **Jalan** | Saat menu dibuka, `LedOn()` dipanggil — LED D2 nyala |
| 3. Timer demo | **Jalan** | Saat menu dibuka, `Timer2Start()` — LED berkedip via overflow interrupt |
| 4. MMCSD info | **Jalan** | File system ops (`cd`, `ld`, `pwd`) berfungsi |
| 5. PM demo | Tidak aktif | `configVddOpVoltage` di-disable, hanya flag internal yang diset |

## Catatan

- Header `soc_AM335x.h`, `beaglebone.h`, `hw_*.h`, `gpio_v2.h`, dan semua driver include berasal dari paket **AM335X_StarterWare_02_00_01_01** di `C:\ti\AM335X_StarterWare_02_00_01_01\`. Library pre-built di `binary\armv7a\cgt_ccs\am335x\...` (drivers.lib, system.lib, platform.lib, utils.lib, libmmcsd.lib) dan `binary\armv7a\cgt_ccs\mmcsdlib\...` harus sudah ada di lokasi tsb; kalau library StarterWare tidak ada, perlu build StarterWare lebih dulu via makefile di `C:\ti\AM335X_StarterWare_02_00_01_01\build\armv7a\cgt_ccs\`.
- Project ini adalah salinan dari contoh bawaan StarterWare (`C:/ti/AM335X_StarterWare_02_00_01_01/examples/beaglebone/demo`), sehingga license-nya mengikuti BSD-style Texas Instruments yang ada di header setiap file sumber.
- Folder `Debug/` dan `Release/` (cache build) sebaiknya dihapus secara periodik jika ingin clean build dari nol, karena linker cache `*.obj` yang dihasilkan toolchain CGT 20.2.7 kadang stale saat konfigurasi include berubah.
- Semua path absolut pakai separator Windows (`C:/ti/...`) dengan forward slash — sesuai format yang dipakai include path di toolchain CGT. Jangan diubah ke backslash.
