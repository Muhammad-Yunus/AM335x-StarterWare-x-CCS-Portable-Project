# dmtimerCounter — Portable StarterWare CCS Project

Contoh aplikasi **DMTimer2** pada BeagleBone (AM335x) yang difungsikan sebagai *down counter* UART. Diadaptasi dari `AM335X_StarterWare_02_00_01_01/examples/beaglebone/dmtimer/dmtimerCounter.c`.

## Apa yang dimaksud "portable"?

Project ini **portable** karena:

- Berada di **luar folder StarterWare** (`C:/D/MY/DEV/TI-CCS-IDE/Workspace_12/dmtimerCounter/`), bukan di dalam tree `C:/ti/AM335X_StarterWare_02_00_01_01/`.
- File `.c` aplikasi (`dmtimerCounter.c`) **disalin ke folder lokal project**, bukan di-*link*-kan ke StarterWare. Project bisa di-*zip*, dipindah ke workstation lain, atau di-*rename* tanpa kehilangan source aplikasi.
- Semua path absolut di `.cproject` mengarah ke instalasi StarterWare (`C:/ti/AM335X_StarterWare_02_00_01_01/`) untuk include header & prebuilt libraries. Folder StarterWare tetap dipakai **read-only** sebagai dependency eksternal, sama persis seperti CMSIS, vendor SDK, dsb.

Intinya: **folder StarterWare adalah dependency eksternal (jangan disentuh), folder project ini adalah working copy aplikasi Anda** — pola yang sama dengan project CCS/SDK apapun.

## Kebutuhan environment

Project ini sudah dikonfigurasi dan diverifikasi untuk:

| Komponen | Versi / Lokasi |
|---|---|
| **Code Composer Studio (CCS)** | `C:/ti/ccs1281/` (CCS 12.8.1) |
| **TI ARM Compiler (CGT)** | `20.2.7.LTS` |
| **AM335X StarterWare** | `C:/ti/AM335X_StarterWare_02_00_01_01/` (wajib install di path ini) |
| **Target board** | BeagleBone (AM335x Cortex-A8) |

CCS lain (mis. CCS 11 atau 20.x) umumnya kompatibel selama CGT `20.2.7.LTS` tersedia. Bila lokasi StarterWare Anda berbeda, edit semua path `C:/ti/AM335X_StarterWare_02_00_01_01/` di `.cproject` (termasuk include path, library path, artifact name, postbuild step) ke lokasi instalasi Anda.

## Struktur project

```
dmtimerCounter/
├── .cproject          # Konfigurasi build (include, library, compiler version)
├── .project           # Metadata Eclipse + linked resource (.out artifact)
├── dmtimerCounter.c   # Source aplikasi (copy lokal dari StarterWare)
├── dmtimerCounter.cmd # Linker command file
├── Debug/             # Build output Debug (otomatis)
└── Release/           # Build output Release (otomatis)
```

### Yang **tidak** boleh diutak-atik

- `C:/ti/AM335X_StarterWare_02_00_01_01/**` — StarterWare dipakai read-only. Jangan modifikasi, jangan rebuild dari sini.

## Cara build di CCS

1. Buka CCS, *Import* project `dmtimerCounter` ke workspace (CCS → *Project Explorer* → *Import CCS Projects*).
2. Pilih build configuration **Debug** atau **Release** di toolbar.
3. **Project → Build Project** (atau `Ctrl+B`).
4. Output `dmtimerCounter.out` akan tertulis di `C:/ti/AM335X_StarterWare_02_00_01_01/binary/armv7a/cgt_ccs/am335x/beaglebone/dmtimer/<Config>/` (lokasinya mengikuti konvensi StarterWare).
5. Hubungkan BeagleBone via JTAG, jalankan debug session, lalu **Resume** (`F8`).

## Output serial (UART0)

Hubungkan terminal serial ke **UART0** BeagleBone (115200 baud, 8N1 default StarterWare) untuk melihat:

```
Tencounter: 9
```

Aplikasi menulis prefix `Tencounter: `, lalu *down counter* dari **9 → 0** dengan interval ±700ms per angka. Setelah counter habis:

```
Tencounter: 9
                                      ← (angka 8, 7, 6, ... 0 muncul di sini)
Success! Passed with status code S_PASS
```

Lalu program halt (`while(1)`). Tekan *Reset* di CCS untuk mengulang.

## Apa yang dilakukan aplikasi

1. Enable clock untuk DMTimer2 (`DMTimer2ModuleClkConfig`).
2. Init UART console (`ConsoleUtilsInit`, type `CONSOLE_UART`).
3. Enable IRQ, daftarkan interrupt handler DMTimer2 ke AINTC.
4. Setup DMTimer (prescaler, mode, load value) supaya overflow tiap ±700ms.
5. Cetak `Tencounter: ` ke UART.
6. Start timer; setiap overflow, ISR decrement `cntValue` dan program utama cetak angka via `\b` (backspace + angka).
7. Saat `cntValue == 0`, stop timer dan cetak `Success! Passed with status code S_PASS`.

## Catatan teknis

- `OPT_CODEGEN_VERSION` di `.cproject` = `20.2.7.LTS`. Naikkan/turunkan hanya bila Anda yakin CGT versi itu terinstal di CCS Anda.
- Linker menggunakan `dmtimerCounter.cmd` lokal (dari StarterWare, sudah dicopy sebagai resource project). Section mapping (RAM_MODEL) cocok untuk boot via CCS debugger; untuk boot dari SD card/MMC perlu *post-process* dengan `tiobj2bin` + `tiimage` (sudah termasuk di postbuild step `.cproject`).
- Libraries yang di-link: `drivers.lib`, `system.lib`, `platform.lib`, `utils.lib` — semuanya prebuilt StarterWare di `${ConfigName}/` (Debug/Release). Bila StarterWare Anda belum di-build, build di tree StarterWare dulu (`examples/beaglebone/dmtimer/` tidak otomatis membangun library ini).

## Lisensi

Source aplikasi (`dmtimerCounter.c`) merupakan bagian dari Texas Instruments StarterWare, dilisensikan di bawah BSD-like license yang tercantum di header file.
