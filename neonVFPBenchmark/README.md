# neonVFPBenchmark

Benchmark NEON SIMD dan VFP coprocessor pada board **BeagleBone (AM335x ARM Cortex-A8)**.

Source project asli ada di StarterWare Texas Instruments
`C:\ti\AM335X_StarterWare_02_00_01_01\examples\beaglebone\neonVFPBenchmark`,
telah di-port ke workspace pribadi agar bisa di-build dari luar folder StarterWare
tanpa memodifikasi instalasi StarterWare.

## Stack

| Item            | Versi / Path                                          |
| --------------- | ----------------------------------------------------- |
| StarterWare     | `C:\ti\AM335X_StarterWare_02_00_01_01`                |
| CCS             | `C:\ti\ccs1281`                                       |
| ARM Compiler    | TI CGT 20.2.7.LTS (`ti-cgt-arm_20.2.7.LTS`)           |
| Target          | BeagleBone — AM335x ARM Cortex-A8                     |

## Tujuan

Mengukur waktu eksekusi operasi floating-point dengan tiga mode berbeda:

1. **NEON** — SIMD engine, proses banyak data per instruksi
2. **VFP** — Vector Floating Point unit standar ARM
3. **Soft-float** — tanpa coprocessor, pure C

Bandingkan throughput untuk fungsi matematika (multiply, scale, sine, cosine)
dan lihat efek compiler flag `--neon` pada performa.

## Struktur Project

```
neonVFPBenchmark/
├── .cproject          # Build config (compiler, linker, include paths)
├── .project           # Eclipse project descriptor + linked resources
├── .ccsproject        # CCS project metadata
├── neonVFPBenchmarkApp.c   # Source aplikasi (COPY dari StarterWare)
├── neonVFPBenchmark.cmd    # Linker command file
├── .settings/         # CCS/Eclipse prefs
├── Debug/             # Build artifacts (cache, dibersihkan setiap rebuild)
└── Release/           # Build artifacts
```

## Cara Build di CCS

1. Buka CCS, **Window → Open Perspective → CCS Edit**
2. **File → Import → Existing Projects into Workspace**
3. Pilih root = folder `neonVFPBenchmark` ini
4. Klik kanan project → **Build Project** (atau `Ctrl+B`)
5. Output binary ada di:
   ```
   C:\ti\AM335X_StarterWare_02_00_01_01\binary\armv7a\cgt_ccs\am335x\beaglebone\neonVFPBenchmark\Debug\
   ├── neonVFPBenchmark.out        # ELF untuk debugger
   ├── neonVFPBenchmark.bin        # Raw binary untuk boot
   └── neonVFPBenchmark_ti.bin     # Signed image untuk bootloader AM335x
   ```

Post-build step otomatis generate `.bin` + signed image lewat `tiobj2bin` dan `tiimage`.

## Cara Menjalankan di Board

Butuh board **BeagleBone (AM335x)** + koneksi serial UART0.

1. Flash `neonVFPBenchmark_ti.bin` ke board lewat JTAG/SD card/bootloader
2. Hubungkan USB-to-Serial ke header UART0 BeagleBone
3. Buka serial console: **115200 baud, 8N1**
4. Reset board → output benchmark muncul di console

## Hasil Benchmark

Sample run di BeagleBone (NEON aktif, `--neon` flag on):

```
**** Starting Benchmarking of Functions ****

VectorScale function takes 8647173 microseconds to run 100000 times
The ticks clocked by the timer is 207532160

VectorProduct function takes 2369002 microseconds to run 100000 times
The ticks clocked by the timer is 56856049

**** Benchmarking of the functions done. ****
```

### Analisis

| Function       | Total µs  | Per-iterasi | Tick/µs | Operasi per iterasi              |
| -------------- | --------- | ----------- | ------- | -------------------------------- |
| VectorScale    | 8,647,173 | 86.47 µs    | ~24     | 1× load + 1× mul + 1× add + store |
| VectorProduct  | 2,369,002 | 23.69 µs    | ~24     | 1× mul + 1× store                 |

Rasio tick/µs konsisten di ~24 MHz (DMTimer7 clock). VectorScale lebih lambat
karena FMA-style pattern `C[i] = A[i]*π + B[i]` plus load konstanta `π` dari
memori tiap iterasi, sedangkan VectorProduct hanya single multiply.

Tick ratio cocok di kedua benchmark → pembacaan timer valid, bukan overflow.

## Apa yang Berbeda dari Project StarterWare Asli

Project StarterWare asli ditulis dengan **path relatif**:

```xml
${ORIGINAL_PROJECT_ROOT}/../../../../../../../include/...
${ORIGINAL_PROJECT_ROOT}/../../../../../../../binary/...
```

Variabel `ORIGINAL_PROJECT_ROOT` di-resolve ke path TIREX, jadi hanya jalan kalau
project di-import langsung dari dalam folder StarterWare. Saat project di-copy
ke workspace pribadi, variabel ini jadi tidak ter-resolve → include path gagal
→ `cannot open source file "beaglebone.h"`.

Versi port ini mengubah semua path ke **absolute**:

```xml
C:/ti/AM335X_StarterWare_02_00_01_01/include/...
C:/ti/AM335X_StarterWare_02_00_01_01/binary/...
```

Compiler version juga di-pin ke **20.2.7.LTS** (aslinya 5.0.4 yang tidak terinstall
di CCS modern). Source file `.c` di-copy ke project root dan link resource ke
StarterWare dihapus dari `.project`.

## Cara Restore ke StarterWare (kalau perlu diff dengan asli)

Source `neonVFPBenchmarkApp.c` di project ini identik dengan
`C:\ti\AM335X_StarterWare_02_00_01_01\examples\beaglebone\neonVFPBenchmark\neonVFPBenchmarkApp.c`
— kalau StarterWare di-update, copy manual dari path tersebut ke project root.

File yang di-modifikasi dari versi StarterWare:
- `.cproject` — path absolut + compiler version
- `.project` — linked resource `.c` dihapus

File yang tidak diubah:
- `neonVFPBenchmarkApp.c` (identical copy)
- `neonVFPBenchmark.cmd`
- `.ccsproject`
- `.settings/`

## Modul Hardware yang Dipakai

- **DMTimer7** — pengukuran waktu eksekusi (24 MHz tick)
- **UART0** — output hasil benchmark ke serial console
- **MMU + Cache** — di-setup oleh `system.lib`
- **NEON/VFP coprocessor** — opsional, via compiler flag `--neon`

## Library yang Di-link

| Library        | Path                                                                 |
| -------------- | -------------------------------------------------------------------- |
| `drivers.lib`  | `binary/armv7a/cgt_ccs/am335x/drivers/Debug/`                        |
| `system.lib`   | `binary/armv7a/cgt_ccs/am335x/system_config/Debug/`                  |
| `platform.lib` | `binary/armv7a/cgt_ccs/am335x/beaglebone/platform/Debug/`            |
| `utils.lib`    | `binary/armv7a/cgt_ccs/utils/Debug/`                                 |
| `libc.a`       | TI runtime support (dari compiler)                                   |

Library ini harus sudah ter-build di StarterWare. Jika belum, buka dan build
project library-nya di CCS dulu (StarterWare menyediakan project library terpisah).

## Troubleshooting

**`cannot open source file "beaglebone.h"`**
Path include di `.cproject` salah arah. Cek bahwa semua INCLUDE_PATH entries
mulai dengan `C:/ti/AM335X_StarterWare_02_00_01_01/...` (bukan
`${ORIGINAL_PROJECT_ROOT}/...`).

**`fatal error #1965` di file source di path StarterWare**
Build cache masih nyangkut referensi lama. Hapus isi folder `Debug/` dan
`Release/`, lalu rebuild. Project sudah punya `.c` lokal di root —
tidak boleh ambil dari StarterWare lagi.

**Compiler version not found**
Pastikan CGT 20.2.7.LTS terinstall di
`C:\ti\ccs1281\ccs\tools\compiler\ti-cgt-arm_20.2.7.LTS\`.

**Library `.lib` not found saat link**
Library StarterWare belum di-build. Buka CCS project library yang relevan
(`drivers`, `system_config`, `platform`, `utils`) di StarterWare dan build dulu.