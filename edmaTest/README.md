# edmaTest

CCS project untuk BeagleBone (AM335x) yang menjalankan contoh **EDMA3 transfer test** dari Texas Instruments StarterWare. Contoh ini menyalin 10 byte dari satu buffer RAM ke buffer lain menggunakan EDMA3 Channel Controller tanpa intervensi CPU, lalu memverifikasi hasilnya melalui UART console.

Source code single-file `edmaTest.c` berisi dua mode — **EDMA** atau **QDMA** — dipilih lewat macro compile-time `CH_TYPE_DMA`. Empat build configuration di project ini mencocokkan dua mode × Debug/Release.

---

## Provenance — kenapa project ini ada di sini

Contoh `edmaTest` di StarterWare asli berada di dalam tree build StarterWare:

```
C:/ti/AM335X_StarterWare_02_00_01_01/examples/beaglebone/edma/
```

…dengan project file di `build/armv7a/cgt_ccs/am335x/beaglebone/edma/`, yang seluruh path-nya menggunakan variable `${ORIGINAL_PROJECT_ROOT}/../../../../../../../...` relatif terhadap lokasi StarterWare. Artinya project StarterWare asli **tidak portable** — kalau folder StarterWare dipindah, project langsung rusak.

Project ini adalah **copy-out / portable version**:

- Source file `edmaTest.c` di-*copy* ke local workspace, bukan di-link ke StarterWare.
- Linker command file `edmaTest.cmd` di-*copy* juga.
- Semua path absolut di-hardcode ke `C:/ti/AM335X_StarterWare_02_00_01_01/` (lihat `.cproject`), sehingga StarterWare hanya dibutuhkan sebagai *external library + header source*, bukan sebagai parent project.

Folder StarterWare **tidak boleh dimodifikasi** oleh project ini — kalau StarterWare di-upgrade ke versi lain, cukup edit path di `.cproject` dan semua include / library / output path akan resolve ulang tanpa perubahan source code.

---

## Toolchain

| Komponen | Versi | Lokasi |
|---|---|---|
| **Code Composer Studio (CCS)** | 12.8.1 (build 12.8.1.00005) | `C:/ti/ccs1281/ccs/` |
| **TI ARM CGT (compiler)** | 20.2.7.LTS | Dideteksi oleh CCS dari `com.ti.cgt.arm.` feature |
| **StarterWare** | 02.00.01.01 | `C:/ti/AM335X_StarterWare_02_00_01_01/` |
| **Target board** | BeagleBone (AM335x, Cortex-A8) | — |

CGT 20.2.7.LTS jauh lebih baru dari StarterWare asli (yang dirancang untuk CGT 5.x). Project ini sudah di-*rebaseline* ke CGT modern — `OPT_CODEGEN_VERSION` di `.cproject` di-set ke `20.2.7.LTS`, dan binary library StarterWare (drivers.lib, system.lib, platform.lib, utils.lib) sudah dibangun dengan CGT yang cocok di StarterWare 02.00.01.01.

---

## Build configurations

Project ini punya **4 configuration** karena satu source file berisi dua mode:

| Configuration | `-DCH_TYPE_DMA` | Output `.out` | Fungsi `main()` panggil |
|---|---|---|---|
| `Debug_EDMA`   | defined   | `edmaTest.out` | `EDMAAppEDMA3Test()`  — manual-trigger EDMA, channel 63 |
| `Release_EDMA` | defined   | `edmaTest.out` | `EDMAAppEDMA3Test()`  — manual-trigger EDMA, channel 63 |
| `Debug_QDMA`   | undefined | `qdmaTest.out` | `EDMAAppQDMA3Test()`  — QDMA trigger-word, channel 0 |
| `Release_QDMA` | undefined | `qdmaTest.out` | `EDMAAppQDMA3Test()`  — QDMA trigger-word, channel 0 |

Output ditulis ke StarterWare binary tree:

```
C:/ti/AM335X_StarterWare_02_00_01_01/binary/armv7a/cgt_ccs/am335x/beaglebone/edma/<ConfigName>/
```

Post-build step menjalankan `tiobj2bin` → `armofd` → `armhex` → `mkhex4bin` → `tiimage` untuk menghasilkan `edmaTest.bin` dan `edmaTest_ti.bin` (image siap SD card untuk bootloader StarterWare).

Library yang di-link:

- `libc.a` (RTS)
- `binary/.../utils/<ConfigName>/utils.lib`
- `binary/.../am335x/drivers/<ConfigName>/drivers.lib`
- `binary/.../am335x/system_config/<ConfigName>/system.lib`
- `binary/.../am335x/beaglebone/platform/<ConfigName>/platform.lib`

---

## Cara build & run

### 1. Pastikan StarterWare library sudah di-build

Sebelum project ini bisa link, library StarterWare di `binary/armv7a/cgt_ccs/...` harus sudah ada. Kalau belum:

```
cd C:/ti/AM335X_StarterWare_02_00_01_01/build/armv7a/cgt_ccs/am335x/beaglebone
make
```

Ini menghasilkan `drivers.lib`, `system.lib`, `platform.lib`, dan `utils.lib` di folder `Debug/` dan `Release/` masing-masing.

### 2. Buka project di CCS

- Launch CCS 12.8.1 dari `C:/ti/ccs1281/ccs/eclipse/ccstudio.exe`
- Workspace: `C:/D/MY/DEV/TI-CCS-IDE/Workspace_12/`
- File → Import → CCS Projects → select `edmaTest/`

### 3. Pilih configuration & build

- Right-click project → **Build Configurations** → **Set Active** → pilih `Debug_EDMA` (atau salah satu dari 4)
- Right-click project → **Build Project** (`Ctrl+B`)

### 4. Run di board

- Hubungkan BeagleBone via USB atau serial debug probe
- Launch configuration debug sesuai target
- Buka terminal serial (115200 baud, 8N1) ke UART0
- Setelah download & run, output yang diharapkan:

```
EDMA3Test: Data write-read matching PASSED.
EDMA3Test PASSED.
```

(`EDMA3Test` untuk config `_EDMA`, `QDMA3Test` untuk config `_QDMA`.)

---

## Struktur file

```
edmaTest/
├── .cproject        — CCS build configuration (4 configs, absolute paths ke StarterWare)
├── .ccsproject      — device family + CGT version metadata
├── .project         — Eclipse project descriptor (linked resources sudah dibersihkan)
├── .settings/       — Eclipse internal preferences
├── edmaTest.c       — source code (copy dari StarterWare/examples/beaglebone/edma/)
├── edmaTest.cmd     — linker command file (copy dari StarterWare)
└── README.md        — file ini
```

Tidak ada folder build cache (`Debug/`, `Release/`, `*_EDMA/`, `*_QDMA/`) di source control — dibuat ulang setiap build.

---

## Yang divalidasi

- ✅ EDMA3 Channel Controller AM335x berfungsi
- ✅ AINTC interrupt routing (`SYS_INT_EDMACOMPINT`, `SYS_INT_EDMAERRINT`) berfungsi
- ✅ ISR (`_EDMAAppEdma3ccComplIsr`) terpicu dan clear IPR dengan benar
- ✅ PaRAM Set (src/dst addr, aCnt/bCnt/cCnt=10/1/1, ITCINTEN+TCINTEN) terkonfigurasi dengan benar
- ✅ StarterWare library linkage cocok dengan CGT 20.2.7.LTS
- ✅ EDMA manual-trigger A-sync 10 byte end-to-end tanpa CPU intervensi

Hasil uji: **EDMA3Test PASSED** di UART console (2026-07-11).