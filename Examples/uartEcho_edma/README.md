# uartEcho_edma

Contoh UART Echo menggunakan EDMA (Enhanced DMA) untuk board **BeagleBone Black (AM335x)**, berbasis TI StarterWare.

## Deskripsi

Program ini mendemonstrasikan transfer data UART tanpa beban CPU per-byte:
- **UART0 RX** → **EDMA** → memori buffer → **EDMA** → **UART0 TX** (echo)
- CPU hanya mengkonfigurasi channel EDMA sekali; transfer data selanjutnya berjalan sendiri di hardware DMA

Tujuan utama: menunjukkan bahwa komunikasi serial full-duplex bisa berjalan tanpa CPU介入 tiap byte.

## Environment yang Digunakan

| Komponen       | Versi / Lokasi |
|----------------|----------------|
| StarterWare    | `C:\ti\AM335X_StarterWare_02_00_01_01` |
| CCS (Code Composer Studio) | `C:\ti\ccs1281\ccs` |
| ARM Compiler   | TI CGT 20.2.7.LTS |
| Target Board   | BeagleBone Black (AM335x, Cortex-A8) |
| Project Path   | `C:\D\MY\DEV\TI-CCS-IDE\Workspace_12\uartEcho_edma` |

Project ini **portable** — semua path di `.cproject` sudah absolut ke `C:/ti/AM335X_StarterWare_02_00_01_01/`, sehingga bisa di-build dari lokasi folder manapun tanpa harus berada di dalam tree StarterWare.

## Struktur Project

```
uartEcho_edma/
├── .cproject          # Konfigurasi build CCS (path include, library, compiler)
├── .project           # Deskripsi Eclipse project
├── .ccsproject        # Metadata CCS
├── .settings/         # Preferensi Eclipse/CDT
├── .launches/         # Konfigurasi debug launch
├── uartEcho_edma.c    # Source program utama (lokal, hasil copy dari StarterWare)
└── uartEcho_edma.cmd  # Linker command file
```

Source `.c` adalah salinan lokal dari:
`C:\ti\AM335X_StarterWare_02_00_01_01\examples\beaglebone\uart_edma\uartEcho_edma.c`

File StarterWare asli **tidak dimodifikasi** — project ini hanya membaca dari path include StarterWare, bukan menulis kembali ke sana.

## Cara Build

1. Buka CCS 12.8.1 (`C:\ti\ccs1281\ccs`)
2. Pilih menu **Project → Build Project** (atau `Ctrl+B`)
3. Pilih konfigurasi: `Debug` atau `Release`
4. Output binary ada di luar project, di dalam tree StarterWare:
   - `C:\ti\AM335X_StarterWare_02_00_01_01\binary\armv7a\cgt_ccs\am335x\beaglebone\uart_edma\Debug\uartEcho_edma.out`
   - `.bin` dan `_ti.bin` di lokasi yang sama

## Cara Test di Serial

1. Hubungkan USB-to-Serial ke header **UART0** BeagleBone (pin TX, RX, GND)
2. Buka serial terminal (PuTTY, TeraTerm, minicom) dengan setting:
   - Baud rate: **115200** (atau sesuai `uartEcho_edma.c`)
   - Data bits: 8, Parity: None, Stop bits: 1
   - Flow control: None
3. Flash file `.bin` atau `_ti.bin` ke board (via SD card atau JTAG)
4. Boot board, akan muncul pesan berikut di terminal:

```
StarterWare AM335X UART DMA application.
The application echoes the characters that you type on the console.
Please Enter 08 bytes from keyboard.
```

5. Ketik **8 karakter** (misal `abcdefgh`), lalu tunggu. Program akan mengirim balik 8 karakter yang sama:

```
abcdefgh   ← input Anda
abcdefgh   ← echo via EDMA UART TX
```

Jika echo muncul, berarti transfer RX → EDMA → memori → EDMA → TX berjalan benar.

## Konfigurasi yang Diterapkan pada Project

Agar project portable dan bisa di-build di luar folder StarterWare, dilakukan penyesuaian berikut pada file `.cproject` dan `.project`:

1. **Path absolut**: Semua `${ORIGINAL_PROJECT_ROOT}/../../../../../../../...` diganti `C:/ti/AM335X_StarterWare_02_00_01_01/...` (artifactName, postbuildStep, include paths, library paths, output file, link info).
2. **Compiler version**: `OPT_CODEGEN_VERSION` diubah dari `5.0.4` menjadi `20.2.7.LTS` (sesuai CGT yang terpasang di CCS 12.8.1).
3. **Source lokal**: Linked resource `uartEcho_edma.c` di `.project` dihapus, file `.c` di-copy ke folder project lokal.
4. **Cache build**: Folder `Debug/` dan `Release/` (beserta isi `makefile`, `objects.mk`, dll.) dihapus agar CCS regenerate konfigurasi build dari `.cproject` yang baru.

## Library yang Digunakan (dari StarterWare)

- `drivers.lib` — driver peripheral (UART, EDMA, dll.)
- `system.lib` — low-level initialization AM335x
- `platform.lib` — board-specific setup (BeagleBone)
- `utils.lib` — helper (uartStdio, dll.)
- `libc.a` — runtime C

## Catatan

- Project ini read-only terhadap StarterWare: hanya memakai path include dan library, tidak menulis/memodifikasi file di `C:\ti\AM335X_StarterWare_02_00_01_01`.
- Jika StarterWare diinstall di lokasi lain, edit path di `.cproject` (cari `C:/ti/AM335X_StarterWare_02_00_01_01/`).
- Jika versi CCS/compiler berbeda, sesuaikan `OPT_CODEGEN_VERSION` di `.cproject`.