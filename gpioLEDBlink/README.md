# gpioLEDBlink

Project portable hasil copy dari example **StarterWare** Texas Instruments (versi **AM335X_StarterWare_02_00_01_01**) untuk board **BeagleBone (AM335x)**. Contoh ini mengedipkan LED menggunakan pin **GPIO1[23]**.

## Deskripsi Singkat

- **Source code**: `gpioLEDBlink.c`
- **Linker command file**: `gpioLEDBlink.cmd`
- **Target device**: AM335x (Cortex-A8, little-endian)
- **Modul GPIO**: GPIO1, pin 23
- **Toolchain**: TI Code Composer Studio (CCS) + CGT ARM v4.9.1

Program melakukan inisialisasi GPIO1[23] sebagai output, lalu toggle HIGH/LOW dengan delay sederhana di dalam loop `while(1)`.

## Cara Pakai

Project ini adalah project CCS yang sudah di-ekstrak ke folder ini, jadi tidak perlu import wizard — cukup buka langsung di CCS.

1. Buka **Code Composer Studio**.
2. Pilih workspace tempat folder `gpioLEDBlink` ini berada (atau workspace yang berisi folder ini).
3. Jika project belum muncul di **Project Explorer**, lakukan import manual:
   - **File → Import… → CCS Projects**
   - Pilih **Search projects in the workspace** atau arahkan **Select root directory** ke folder `gpioLEDBlink` ini, lalu klik **Finish**.
4. Pilih project `gpioLEDBlink` di Project Explorer.
5. **Project → Build All** (atau tekan `Ctrl+B`) untuk compile.
6. Setelah build sukses, output `Debug/gpioLEDBlink.out` siap di-load ke BeagleBone via debugger (misal JTAG).

## Catatan

- Header `soc_AM335x.h`, `beaglebone.h`, dan `gpio_v2.h` berasal dari paket **AM335X_StarterWare_02_00_01_01**. Pastikan paket StarterWare sudah ter-install di `C:\ti\AM335X_StarterWare_02_00_01_01\` dan path include-nya sudah dikonfigurasi di CCS agar build sukses.
- Project ini adalah salinan dari contoh bawaan StarterWare (`C:/ti/AM335x_StarterWare_Free/build/armv7a/cgt_ccs/am335x/beaglebone/gpio`), sehingga license-nya mengikuti BSD-style Texas Instruments yang ada di header `gpioLEDBlink.c`.