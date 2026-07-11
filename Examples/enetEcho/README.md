# enetEcho — CCS Project (Portable Copy)

Project **enetEcho** ini adalah hasil **copy** dari example
`enet_echo` milik **TI AM335X StarterWare**. Project sudah di-port
agar bisa berdiri sendiri (portable) — semua source `.c` dan `.h`
yang dipakai aplikasi sudah disalin ke folder project lokal, dan
semua path relatif di `.cproject` sudah diganti menjadi path
**absolut** ke instalasi StarterWare.

Library StarterWare (`drivers.lib`, `system.lib`, `platform.lib`,
`utils.lib`) tetap dibaca langsung dari folder instalasi
StarterWare (tidak dicopy). Folder StarterWare **tidak dimodifikasi**.

---

## Persyaratan

| Komponen | Versi / Lokasi |
|---|---|
| **CCS (Code Composer Studio)** | `C:\ti\ccs1281\ccs` (CCS 12.8.1) |
| **TI ARM Compiler** | `ti-cgt-arm_20.2.7.LTS` (otomatis dipilih oleh CCS lewat `OPT_CODEGEN_VERSION`) |
| **AM335X StarterWare** | `C:\ti\AM335X_StarterWare_02_00_01_01` |
| **Target board** | BeagleBone (AM335x, ARM Cortex-A8) |
| **Library StarterWare (prebuilt)** | Harus sudah dibuild sekali. Path: `C:\ti\AM335X_StarterWare_02_00_01_01\binary\armv7a\cgt_ccs\am335x\…\Debug\` |

Pastikan folder StarterWare persis di:
```
C:\ti\AM335X_StarterWare_02_00_01_01\
```
Kalau lokasinya berbeda, edit path absolut di `.cproject` (cari
`C:/ti/AM335X_StarterWare_02_00_01_01/` di file tersebut — ada di
include path, library, dan post-build step).

---

## Struktur Project

```
enetEcho/
├── .cproject          # Build config (Debug + Release), path include absolut
├── .project           # Linked resource sudah dilepas dari StarterWare
├── .ccsproject
├── .launches/
├── .settings/
├── echod.c            # ← copy dari StarterWare lwip/echoserver_raw
├── enetEcho.c         # ← copy dari StarterWare examples/beaglebone/enet_echo
├── lwiplib.c          # ← copy dari StarterWare lwip/ports/cpsw
├── locator.c          # ← tetap dibaca dari StarterWare (di-include oleh lwiplib.c)
├── lwipopts.h         # ← copy dari StarterWare (project-specific lwIP config)
├── enetEcho.cmd       # Linker command file
└── README.md          # File ini
```

File `locator.c` **tidak** dicopy ke lokal — `#include "locator.c"`
di dalam `lwiplib.c` langsung resolve ke StarterWare lewat include
path `…/lwip-1.4.0/ports/cpsw/`. Kalau dicopy lokal, akan jadi
duplicate symbol saat linking.

---

## Cara Build

Di CCS:
1. **Import project**: `File → Import → Existing Projects into Workspace`
   → arahkan ke folder `enetEcho/`.
2. **Refresh**: klik kanan project → **Refresh (F5)**.
3. **Clean**: `Project → Clean…` → centang `enetEcho` → **OK**.
4. **Build**: `Project → Build Project` (Ctrl+B).

Build sukses akan menghasilkan file di dalam StarterWare
(`binary\armv7a\cgt_ccs\am335x\beaglebone\enet_echo\Debug\`):
- `enetEcho.out`  — ELF utama
- `enetEcho.bin`  — binary mentah
- `enetEcho_ti.bin` — bootable image (melewati step `tiimage`)

---

## Serial Console (UART0)

Hubungkan board BeagleBone lewat USB-to-Serial ke PC. Buka terminal
serial (PuTTY / TeraTerm / `minicom`) dengan setting:
- Baud: **115200**
- Data bits: 8, Parity: None, Stop bits: 1, Flow control: None
- Port: sesuai device manager (mis. `COM3`)

---

## Menjalankan Aplikasi

Setelah di-flash / dimuat lewat JTAG dan board boot, di serial akan
muncul banner:

```
StarterWare Ethernet Echo Application.

Acquiring IP Address for Port 1...

PHY found at address 0 for Port 1 of Instance 0.
Performing Auto-Negotiation...
Auto-Negotiation Successful.
Transfer Mode : 100 Mbps Full Duplex.
PHY link verified for Port 1 of Instance 0.
DHCP Trial 1...

Port 1 IP Address Assigned: 192.168.0.126
```

(Layanan Echo berjalan di **port TCP 2000**.)

---

## Koneksi TCP dari PC (uji Echo)

Setelah IP muncul di serial, buka terminal **baru** di PC (yang
satu jaringan dengan board) dan jalankan `nc` (netcat):

```bash
nc 192.168.0.126 2000
```

Ganti `192.168.0.126` dengan IP yang muncul di serial (diatur DHCP).
Kalau DHCP gagal atau ingin IP statis, edit `lwipopts.h` (define
`STATIC_IP_ADDRESS_PORT1` dan kawan-kawan), lalu rebuild.

Setelah `nc` connect, ketik pesan apa saja lalu Enter — karakter
yang Anda ketik akan dikembalikan oleh board (echo). Untuk keluar,
tekan `Ctrl+C`.

---

## Catatan Portabilitas

Project ini **bisa dipindah ke PC / workspace lain** tanpa mengubah
apa pun di StarterWare, asalkan:

1. Folder StarterWare ada persis di `C:\ti\AM335X_StarterWare_02_00_01_01`.
   Kalau beda, cukup edit `.cproject` (search-replace
   `C:/ti/AM335X_StarterWare_02_00_01_01/` jadi path baru).
2. Library StarterWare (`.lib`) untuk target sudah dibuild di
   lokasi yang dirujuk `.cproject`.

Kalau Anda sering rebuild di PC dengan path StarterWare berbeda,
lihat script `make-project-portable.ps1` di project tetangga
(`gpioLEDBlink/`) sebagai referensi otomasi.