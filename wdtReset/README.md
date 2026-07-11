# wdtReset — AM335x Watchdog Timer Reset Example

CCS project hasil copy dari StarterWare example `wdtReset` (AM335x StarterWare 02.00.01.01) yang sudah di-*port* agar bisa di-build langsung dari workspace tanpa bergantung pada struktur folder StarterWare asli.

## Deskripsi

Program ini mengaktifkan Watchdog Timer (WDT) di AM335x (BeagleBone) lalu me-reset-nya setiap kali user menekan tombol di terminal serial. Jika tidak ada input dalam waktu lebih dari 4 detik, WDT akan melakukan system reset.

Output di terminal serial (115200 8N1) saat program running:

```
USB Host mode controller at 47401800 using PIO, IRQ 0
Program Reset!Input any key at least once in every 4 seconds to avoid a further reset.
```

## Versi yang digunakan

| Komponen     | Versi                                                          |
| ------------ | -------------------------------------------------------------- |
| CCS IDE      | 12.8.1 (`C:\ti\ccs1281`)                                       |
| StarterWare  | 02.00.01.01 (`C:\ti\AM335X_StarterWare_02_00_01_01`)           |
| Compiler     | TI ARM CodeGen 20.2.7.LTS                                      |
| Target       | BeagleBone (AM335x, ARM Cortex-A8)                             |

## Asal Example

Source asli berada di StarterWare dan TIDAK boleh dimodifikasi (hanya dipakai read-only sebagai include & library):

- Source asli: `C:\ti\AM335X_StarterWare_02_00_01_01\examples\beaglebone\watchdogTimer\wdtReset.c`
- Linker command asli: `C:\ti\AM335X_StarterWare_02_00_01_01\build\armv7a\cgt_ccs\am335x\beaglebone\watchdogTimer\wdtReset.cmd`
- Library StarterWare: `drivers.lib`, `system.lib`, `platform.lib`, `utils.lib`
- Header StarterWare: `include/`, `include/hw/`, `include/armv7a/`, `include/armv7a/am335x/`

## Yang Berubah dari Example Asli

Versi asli project hasil *Copy Project to Workspace* di CCS menggunakan variabel `ORIGINAL_PROJECT_ROOT` dan path relatif `../../../../../../../` yang mengasumsikan project berada tepat di dalam struktur tree StarterWare. Setelah di-port, semua path jadi **absolute** dan source `.c` di-copy ke local project.

### 1. Path relative → absolute di `.cproject`

Semua `${ORIGINAL_PROJECT_ROOT}/../../../../../../../` diganti jadi `C:/ti/AM335X_StarterWare_02_00_01_01/`:

- `artifactName` (Debug & Release)
- `postbuildStep` (tiobj2bin + tiimage)
- Compiler `INCLUDE_PATH` (`include`, `include/hw`, `include/armv7a`, `include/armv7a/am335x`)
- Linker `OUTPUT_FILE`, `LIBRARY` (drivers, system_config, platform, utils), `XML_LINK_INFO`

### 2. Compiler version

`OPT_CODEGEN_VERSION` diubah dari `5.0.4` → `20.2.7.LTS` (di Debug & Release).

### 3. Source `.c` jadi local

`wdtReset.c` di-remove dari `<linkedResources>` di `.project`, lalu di-copy dari StarterWare ke root project ini. Variable `ORIGINAL_PROJECT_ROOT` juga dihapus.

### 4. Cache build dihapus

Folder `Debug/` dan `Release/` dikosongkan dari file cache lama (`.obj`, `.d`, `.map`, `makefile`, dll).

## Struktur Project

```
wdtReset/
├── .ccsproject           # Device & linker command settings
├── .cproject             # Build config (compiler, linker, paths)
├── .project              # Eclipse project metadata
├── wdtReset.c            # Source (local copy dari StarterWare)
├── wdtReset.cmd          # Linker command file
├── README.md             # File ini
├── Debug/                # Output build Debug
└── Release/              # Output build Release
```

## Cara Build di CCS

1. Buka CCS 12.8.1.
2. **File → Import → Code Composer Studio → Existing CCS Eclipse Projects**.
3. Browse ke folder `wdtReset` ini, checklist project `wdtReset`, **Finish**.
4. Pilih project → **Project → Build Project** (atau `Ctrl+B`).

Output artifact akan ditulis ke:

```
C:/ti/AM335X_StarterWare_02_00_01_01/binary/armv7a/cgt_ccs/am335x/beaglebone/watchdogTimer/Debug/wdtReset.out
C:/ti/AM335X_StarterWare_02_00_01_01/binary/armv7a/cgt_ccs/am335x/beaglebone/watchdogTimer/Debug/wdtReset.bin
C:/ti/AM335X_StarterWare_02_00_01_01/binary/armv7a/cgt_ccs/am335x/beaglebone/watchdogTimer/Debug/wdtReset_ti.bin
```

> Catatan: artifact path masih di dalam folder StarterWare (di luar project). Ini adalah pola bawaan StarterWare — semua output project dikonsolidasikan di `binary/armv7a/cgt_ccs/...`. Folder `binary/` di StarterWare aman ditulis, hanya `examples/`, `include/`, dan source code yang tidak boleh dimodifikasi.

## Catatan Warning

Build menghasilkan warning `#1557-D` yang merujuk ke `C:\ti\AM335X_StarterWare_02_00_01_01\include\hw\hw_watchdog.h` line 91 (spasi antara backslash dan newline pada line splice). Ini adalah isi file StarterWare dan tidak boleh diedit — warning bersifat advisory (`-D`), tidak menggagalkan build.
