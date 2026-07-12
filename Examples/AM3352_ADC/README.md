# AM3352_ADC

Baca ADC AIN0 (P9_39) di BeagleBone/AM3352 dan kirim hasilnya lewat UART0
tiap 500 ms dalam format `[AIN0] raw=NNNN mV=MMMM`.

Sample jalan di StarterWare 02.00.01.01 + CCS 12.8 + ti-cgt-arm 20.2.7.LTS,
persis mengikuti pola `AM3352_GPIO_LED_DELAY`.

---

## 1. Buat project dari nol di CCS

### 1.1 Siapkan StarterWare

CCS harus bisa menemukan StarterWare header dan prebuilt library. StarterWare
default ada di `C:/ti/AM335X_StarterWare_02_00_01_01/`. Kalau path Anda
beda, ubah 4 path di langkah 1.4.

### 1.2 New CCS Project

1. **File → New → CCS Project**
2. Isi:
   - **Project name**: `AM3352_ADC`
   - **Location**: `C:/D/MY/DEV/TI-CCS-IDE/Workspace_12/`
   - **Use default location**: ON
   - **Connection**: `Texas Instruments XDS2xx USB Emulator`
     (atau emulator JTAG apa pun yang Anda pakai)
3. **Project type and toolchain**:
   - **Project type**: ARM
   - **Toolchain**: ARM Compiler (ti-cgt-arm 20.2.7.LTS)
   - **Compiler version**: 20.2.7.LTS
4. Klik **Next** beberapa kali (lewati Template/Build settings) lalu **Finish**.

### 1.3 Tambah source + linker command file

Di **Project Explorer**:

1. **File → New → File** → `main.c` (kosong dulu, isi nanti).
2. **File → New → File** → `AM335x.cmd` di root project (bukan di folder `Debug/`).
   Isi dengan linker command file persis seperti `AM3352_GPIO_LED_DELAY/AM335x.cmd`
   (DDR memory layout, `-stack 0x0008`, `-heap 0x2000`, `-e Entry`).

   > **Penting:** `AM335x.cmd` HARUS ada di root project. Kalau di `Debug/`,
   > CCS auto-generated `makefile` akan menariknya sebagai `Debug\AM335x.cmd`
   > dan referensi `Debug/main.obj` jadi double-defined.

### 1.4 Set include path + compiler flags

Klik kanan project → **Properties**:

#### Build → ARM Compiler → Include Options
Tambahkan 5 path:
```
C:/ti/ccs1281/ccs/tools/compiler/ti-cgt-arm_20.2.7.LTS/include
C:/ti/AM335X_StarterWare_02_00_01_01/include
C:/ti/AM335X_StarterWare_02_00_01_01/include/hw
C:/ti/AM335X_StarterWare_02_00_01_01/include/armv7a
C:/ti/AM335X_StarterWare_02_00_01_01/include/armv7a/am335x
```

#### Build → ARM Compiler → Advanced Options → Predefined Symbols
Tambahkan:
- `am3352`
- `A8_CORE=1`

#### Build → ARM Compiler → Language Options
- **ABI**: `eabi`

#### Build → ARM Compiler → Diagnostic Options
- **Wrap diagnostics**: `off`
- **Display error numbers**: ON

#### Build → ARM Linker → Basic Options
- **Output format**: ELF
- **Map file**: ON (output `AM3352_ADC.map`)
- **Linker command file**: `../AM335x.cmd`

#### Build → ARM Linker → Advanced Options
- **Reread libraries**: ON
- **Warn about sections**: ON
- **Generate XML link info**: ON
- **Link using RAM model**: ON

### 1.5 Linker libraries

**Build → ARM Linker → File Search Path**:

Add the 5 library dirs (atau cukup path ini):
```
C:/ti/ccs1281/ccs/tools/compiler/ti-cgt-arm_20.2.7.LTS/lib
C:/ti/AM335X_StarterWare_02_00_01_01/binary/armv7a/cgt_ccs/am335x/drivers/Debug
C:/ti/AM335X_StarterWare_02_00_01_01/binary/armv7a/cgt_ccs/am335x/system_config/Debug
C:/ti/AM335X_StarterWare_02_00_01_01/binary/armv7a/cgt_ccs/am335x/beaglebone/platform/Debug
C:/ti/AM335X_StarterWare_02_00_01_01/binary/armv7a/cgt_ccs/utils/Debug
```

**Build → ARM Linker → Libraries** → **+** tambahkan 5 library:
```
libc.a
C:/ti/AM335X_StarterWare_02_00_01_01/binary/armv7a/cgt_ccs/am335x/drivers/Debug/drivers.lib
C:/ti/AM335X_StarterWare_02_00_01_01/binary/armv7a/cgt_ccs/am335x/system_config/Debug/system.lib
C:/ti/AM335X_StarterWare_02_00_01_01/binary/armv7a/cgt_ccs/am335x/beaglebone/platform/Debug/platform.lib
C:/ti/AM335X_StarterWare_02_00_01_01/binary/armv7a/cgt_ccs/utils/Debug/utils.lib
```

### 1.6 Isi `main.c`

Copy paste isi `main.c` dari project ini. Yang penting:

```c
int main(void)
{
    /* UART + ADC bring-up */
    UART0ModuleClkConfig();
    UARTPinMuxSetup(0);
    /* ... */

    /* Interrupt bring-up: URUTAN INI PENTING (lihat §2.1) */
    IntAINTCInit();        /* reset AINTC, seed default vector table */
    IntMasterIRQEnable();  /* clear I-bit di CPSR */

    /* ... */

    DelayTimerSetup();     /* DMTimer7 + register DMTimerIsr */

    while(1) { /* baca ADC, kirim UART, delay(500) */ }
}
```

### 1.7 Build

**Project → Build All** (Ctrl+B).

Kalau sukses, output di `Debug/`:
- `AM3352_ADC.out`   — ELF untuk JTAG
- `AM3352_ADC.bin`   — raw binary untuk SD boot
- `AM3352_ADC.map`   — memory map (cek `Entry` ada di `0x80000000`)

### 1.8 Run di debugger

1. Klik kanan project → **Debug As → Code Composer Debug Session**
2. Pilih target **Cortex A8** di connection Anda
3. CCS akan auto-load `.out` ke target lalu reset+run

Output di UART0 (115200 8N1) akan muncul:
```
=== AM3352_ADC-NEWBUILD-2026-07-12-fingerprint-ADC2 ===
Reading AIN0 (P9_39) every 500 ms.
[AIN0] raw=2048 mV=899
[AIN0] raw=2051 mV=900
...
```

---

## 2. Troubleshooting

### 2.1 `delay()` mentok — program tidak pernah masuk main loop, atau "loncat ke alamat ngaco"

**Gejala:**
- Debugger hang begitu `IntMasterIRQEnable()` dipanggil.
- PC tiba-tiba pindah ke alamat di luar OCMC (`0xE1B0CC30` dll).
- Di Memory window, `0x4030FC00` (vector table di OCMC) berisi data acak
  alih-alih `0xE59FF018` (LDR PC, [PC, +24]).

**Penyebab:** urutan init yang salah. Tiga komponen IRQ harus di-setup
dengan urutan ini (lihat `examples/evmAM335x/dmtimer/dmtimerCounter.c`):

```
IntAINTCInit()       ← 1. reset AINTC, seed fnRAMVectors[]
IntMasterIRQEnable() ← 2. clear I-bit di CPSR
DelayTimerSetup()    ← 3. register DMTimerIsr di SYS_INT_TINT7
```

`DelayTimerSetup()` memanggil `IntSystemEnable(TINT7)` — kalau I-bit sudah
bersih tapi ISR belum terdaftar, IRQ spurious akan lompat ke alamat
default (`IntDefaultHandler`) dan debugger bingung.

**Fix:** pindahkan `DelayTimerSetup()` ke SETELAH `IntMasterIRQEnable()`.
Lihat komentar di `main.c` line 308–322 untuk versi lengkap.

---
