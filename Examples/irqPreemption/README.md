# irqPreemption

Contoh demonstrasi **IRQ preemption bertingkat (3 level)** untuk BeagleBone (AM335x). Proyek ini di-*copy* dari StarterWare dan di-*port* agar bisa di-*build* di luar folder StarterWare — StarterWare hanya dipakai sebagai **library driver & header**, bukan sebagai sumber yang di-*build* bersama.

## Apa yang didemonstrasikan

Tiga ISR dijalankan dengan prioritas berbeda di ARM Interrupt Controller (AINTC). Saat UART0 menerima input, UART ISR (prioritas terendah) berjalan lebih dulu, lalu di-*preempt* berurutan oleh RTC ISR dan DMTimer2 ISR (prioritas tertinggi). Setelah tiap ISR tinggi selesai, eksekusi kembali ke ISR yang tertunda.

| Interrupt  | Sumber                | Prioritas AINTC |
|------------|-----------------------|-----------------|
| DMTimer2   | Timer overflow (TINT2) | 1 (tertinggi)   |
| RTC        | Alarm tiap detik      | 2 (sedang)      |
| UART0      | Input karakter UART   | 4 (terendah)    |

Output yang diharapkan di terminal serial (115200 8N1):

```
StarterWare Interrupt Preemption Demonstration.
UART ISR Entry.
        RTC ISR Entry.
                Timer ISR Entry.
                Timer ISR Exit.
        RTC ISR Exit.
UART ISR Exit.
```

## Requirement

| Komponen         | Versi / Lokasi                                       |
|------------------|------------------------------------------------------|
| StarterWare      | `C:/ti/AM335X_StarterWare_02_00_01_01/`              |
| Code Composer Studio | **12.8.1.00005** (`C:/ti/ccs1281/ccs/`)           |
| ARM CGT          | `ti-cgt-arm_20.2.7.LTS` (di-bundle CCS)              |
| Target           | BeagleBone / AM335x (Cortex-A8)                      |
| Terminal serial  | 115200 baud, 8N1                                      |

> StarterWare **tidak boleh dimodifikasi**. Proyek ini hanya referensi via include path dan library path.

## Layout proyek

```
irqPreemption/
├── .ccsproject                # metadata CCS (product config)
├── .cproject                  # build settings — MEMPERTAHANKAN path StarterWare sebagai include/lib absolut
├── .project                   # metadata Eclipse project, linked resources untuk .out artifact saja
├── .launches/irqPreemption.launch   # debug launch config (XDS110 / JTAG sesuai board)
├── irqPreemption.c            # sumber aplikasi (di-copy dari StarterWare, dimodifikasi path include otomatis)
├── irqPreemption.cmd          # linker command file (alokasi section ke DDR_MEM 0x80000000)
├── Debug/                     # cache build Debug — boleh dihapus untuk rebuild bersih
├── Release/                   # cache build Release — boleh dihapus untuk rebuild bersih
└── README.md
```

## Cara StarterWare dipakai sebagai *library only*

Proyek ini **tidak** meng-*build* ulang StarterWare. StarterWare hanya diakses di tiga titik (semua *absolute path* di `.cproject`):

1. **Include path** (compiler `-I`):
   - `C:/ti/AM335X_StarterWare_02_00_01_01/include`
   - `C:/ti/AM335X_StarterWare_02_00_01_01/include/hw`
   - `C:/ti/AM335X_StarterWare_02_00_01_01/include/armv7a`
   - `C:/ti/AM335X_StarterWare_02_00_01_01/include/armv7a/am335x`
2. **Library** (linker `-l`):
   - `drivers.lib`  — `binary/armv7a/cgt_ccs/am335x/drivers/${ConfigName}/`
   - `system.lib`   — `binary/armv7a/cgt_ccs/am335x/system_config/${ConfigName}/`
   - `platform.lib` — `binary/armv7a/cgt_ccs/am335x/beaglebone/platform/${ConfigName}/`
   - `utils.lib`    — `binary/armv7a/cgt_ccs/utils/${ConfigName}/`
3. **Pre-define**: `-Dam335x`

Library di atas harus sudah ter-*build* di StarterWare (lihat "Build StarterWare sekali" di bawah). Tanpa library ini, link akan gagal dengan `undefined reference` ke fungsi StarterWare.

## Build proyek ini

### Lewat CCS IDE

1. Pilih **Window → Preferences → Code Composer Studio → Products**, pastikan **SYS/BIOS** dan **StarterWare** tidak perlu diinstal/didaftarkan — cukup ARM compiler ter-*detect*.
2. **File → Import → Existing CCS Projects**, arahkan ke folder `irqPreemption/`. Pilih proyek `irqPreemption`.
3. Pilih konfigurasi **Debug** atau **Release** di dropdown toolbar.
4. **Project → Build Project** (`Ctrl+B`).
5. Output `.out` akan ditulis ke StarterWare di:
   ```
   C:/ti/AM335X_StarterWare_02_00_01_01/binary/armv7a/cgt_ccs/am335x/beaglebone/irq_preemption/${ConfigName}/irqPreemption.out
   ```
   File `.bin` dan `_ti.bin` dihasilkan oleh *post-build step* (lihat `.cproject` `postbuildStep`).

### Build artifact path

| Artifact         | Path                                                                              |
|------------------|-----------------------------------------------------------------------------------|
| `.out` (ELF)     | `…/irq_preemption/Debug/irqPreemption.out`                                        |
| `.bin`           | `…/irq_preemption/Debug/irqPreemption.bin`                                        |
| `_ti.bin` (boot) | `…/irq_preemption/Debug/irqPreemption_ti.bin` (header untuk boot ROM AM335x)      |
| `.map`           | `${ProjName}.map` di `${BuildDirectory}` (= `Debug/` atau `Release/`)             |

## Konfigurasi build utama (`.cproject`)

- **Toolchain**: TI v20.2.7.LTS, target `mv7A8`, EABI, little-endian, code state 32, dwarf debug
- **Pre-define**: `am335x`
- **Initialization model**: RAM (`.text:Entry` di 0x80000000)
- **Post-build**: `tiobj2bin` → `mkhex4bin` → `tiimage` (header `0x80000000`) untuk hasilkan `_ti.bin`

## Membersihkan cache build

Untuk rebuild bersih, hapus folder cache dan biarkan gmake meregenerasi `makefile`/`sources.mk`/`subdir_*.mk` dari `.cproject`:

```
rm -rf Debug Release
```

(setelahnya CCS akan meregenerasi otomatis saat build.)

## Catatan port dari StarterWare

Yang berubah dibanding proyek StarterWare asli:

- `${ORIGINAL_PROJECT_ROOT}/../../../../../../../…` → absolute `C:/ti/AM335X_StarterWare_02_00_01_01/…`
- `OPT_CODEGEN_VERSION` dari `5.0.4` (referensi StarterWare) → `20.2.7.LTS` (CCS 12.8.1)
- `irqPreemption.c` di-*copy* ke lokal; linked resource StarterWare dihapus
- `OUTPUT_TYPE=executable` + `LINK_ORDER` ditambahkan agar linker tahu urutan library

File asli di StarterWare **tidak disentuh** — port bersifat satu arah.
