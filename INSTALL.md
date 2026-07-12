# Installation Guide — AM335x StarterWare x CCS on Antminer L3+

This guide walks through setting up the full toolchain on Windows:

1. **Code Composer Studio (CCS) v12.8.1** — installed at `C:\ti\ccs1281`
2. **AM335x StarterWare 02.00.01.01** — installed at `C:\ti\AM335X_StarterWare_02_00_01_01`
3. **Import the driver/core library projects** from the StarterWare tree into CCS (build only, then delete from view)
4. **Import the portable sample programs** in this repository

> 🎯 **Target hardware:** Antminer L3+ (TI AM3352, Cortex-A8). The Antminer L3+ has the same die as the BeagleBone Black except the **PRU subsystems are not populated**, so PRU-only demos will not run. Stick to GPIO / UART / Timer / MMCSD / RTC / EDMA — they're all Cortex-A8 main peripherals and work fine.

---

## Table of Contents

- [1. Prerequisites](#1-prerequisites)
- [2. Install Code Composer Studio](#2-install-code-composer-studio)
- [3. Install AM335x StarterWare](#3-install-am335x-starterware)
- [4. Import the StarterWare driver & library projects](#4-import-the-starterware-driver--library-projects)
- [5. Build the driver & library projects](#5-build-the-driver--library-projects)
- [6. Remove the driver projects from the CCS view (keep on disk)](#6-remove-the-driver-projects-from-the-ccs-view-keep-on-disk)
- [7. Import the portable sample programs](#7-import-the-portable-sample-programs)
- [8. Build & run a sample](#8-build--run-a-sample)
- [Troubleshooting](#troubleshooting)

---

## 1. Prerequisites

| Requirement | Notes |
|---|---|
| OS | Windows 10 / 11 (64-bit) |
| Disk space | ~6 GB for CCS + ~1 GB for StarterWare |
| RAM | 4 GB minimum, 8 GB recommended |
| Admin rights | Required for the CCS installer |
| Internet | Required for the download links below |

---

## 2. Install Code Composer Studio

### 2.1 Download

Grab the **offline installer** for **CCS 12.8.1** from TI:

🔗 **<https://www.ti.com/tool/download/CCSTUDIO/12.8.1>**

Pick the **Windows** build. The offline installer is a single `.exe` (around 2 GB) — easier than the web installer because it doesn't need to redownload components.

### 2.2 Run the installer

1. Right-click the downloaded `.exe` → **Run as administrator**.
2. When prompted for the install location, set:

   ```
   C:\ti\ccs1281
   ```

3. In the **Select Components** screen, make sure to tick:

   - ✅ **Sitara AM3x ARM Processors** (this pulls in the ARM compiler, debug drivers, and the XDS emulation support for AM335x)

   The other families are optional — only AM3x is needed for the Antminer L3+.

4. Accept the license agreements and click **Next → Install**.
5. Wait for the installation to finish (~10–20 minutes depending on disk speed).
6. **Launch CCS** when prompted.

> 💡 If you skipped the launcher, start it manually from `C:\ti\ccs1281\ccs\eclipse\ccstudio.exe`.

---

## 3. Install AM335x StarterWare

### 3.1 Download

Grab the installer from TI:

🔗 **<https://www.ti.com/tool/download/STARTERWARE-AM335X/02.00.01.01>**

This is a single Windows installer `.exe` (~150 MB).

### 3.2 Run the installer

1. Right-click the downloaded installer → **Run as administrator**.
2. When prompted for the install location, accept the default:

   ```
   C:\ti\AM335X_StarterWare_02_00_01_01
   ```

   > ⚠️ **Do not change the install path.** The portable projects in this repo assume the exact path `C:\ti\AM335X_StarterWare_02_00_01_01`. If you absolutely must use a different path, you'll have to update the include / library variables in every `.ccsproject` file in this repo.
3. Click **Next → Install → Finish**.

You should now see a folder layout like:

```
C:\ti\AM335X_StarterWare_02_00_01_01\
├── build\
├── driverlib\
├── examples\
├── grlib\
├── hardware\
├── include\
├── libraries\
├── mmcsdlib\
├── platform\
├── system\
├── tools\
└── utils\
```

---

## 4. Import the StarterWare driver & library projects

These five projects contain the **driver source code and core runtime libraries** that the portable sample programs link against. They must be built once, then removed from the CCS Project Explorer view (but **kept on disk**).

### 4.1 Start the import

In CCS:

1. **Project → Import CCS Projects...**
2. In the *Select search-directory* field, browse to:

   ```
   C:\ti\AM335X_StarterWare_02_00_01_01
   ```

3. CCS will scan the folder and list **a lot** of projects — most of them are the example programs that come with StarterWare.

### 4.2 Deselect everything, then tick only these five

From the list, **uncheck** everything and then check **only** the following five:

- ✅ **`drivers`**
- ✅ **`mmcsdlib`**
- ✅ **`platform`**
- ✅ **`system`**
- ✅ **`utils`**

> 💡 Quick filter tip: type `drivers` (or `mmcsdlib`, etc.) in the filter box above the project list to narrow the list down to one project at a time.

### 4.3 ⚠️ Do NOT check "Copy projects into workspace"

In the **Import CCS Projects** dialog there is a checkbox labelled:

```
☐ Copy projects into workspace
```

**Leave this box UNCHECKED.**

> 🚫 **JANGAN checklist "Copy projects into workspace".**
>
> If you tick this, CCS will **duplicate** the entire `drivers/`, `mmcsdlib/`, `platform/`, `system/`, `utils/` trees into your workspace folder. That:
> - Bloats the workspace with thousands of files you don't need.
> - Breaks the source-of-truth principle — now there are two copies of StarterWare and you don't know which one CCS is actually compiling.
> - Causes the portable projects in this repo to mis-resolve library paths.

Click **Finish** to complete the import.

---

## 5. Build the driver & library projects

The five imported projects must be built **once** so that the `.lib` files they produce exist on disk. The portable projects in this repo link against those `.lib` outputs.

1. In the **Project Explorer**, select the five imported projects (`drivers`, `mmcsdlib`, `platform`, `system`, `utils`).
2. **Project → Build All** (or right-click → **Build Project**).

The build will take a few minutes — `drivers` alone has hundreds of source files. Expected output for each project:

| Project | Output |
|---|---|
| `drivers` | `C:\ti\AM335X_StarterWare_02_00_01_01\build\armv7a\cgt_ccs\am335x\drivers.lib` |
| `mmcsdlib` | `…\build\armv7a\cgt_ccs\am335x\mmcsdlib.lib` |
| `platform` | `…\build\armv7a\cgt_ccs\am335x\platform.lib` |
| `system` | `…\build\armv7a\cgt_ccs\am335x\system.lib` |
| `utils` | `…\build\armv7a\cgt_ccs\am335x\utils.lib` |

If the build succeeds for all five, you're done with this step.

---

## 6. Remove the driver projects from the CCS view (keep on disk)

Now that the libraries are built, you don't want the driver source code cluttering your CCS Project Explorer every time you open the workspace.

1. In the **Project Explorer**, select the five projects (`drivers`, `mmcsdlib`, `platform`, `system`, `utils`).
2. Right-click → **Delete**.
3. A dialog appears asking **"Delete project(s) from CCS?"** with the option:

   ```
   ☑ Delete project contents on disk (cannot be undone)
   ```

   **UNCHECK this box.** Make sure it is empty/blank.

   > 🚫 **JANGAN checklist "Delete project contents on disk".**
   >
   > The `.ccsproject` / `.cproject` / `.project` files in the StarterWare tree are throwaway, but the **source code and the newly-built `.lib` files MUST stay on disk**. Ticking this would wipe out everything you just built, and you'd have to reinstall StarterWare.

4. Confirm the delete.

What you should see now:

- ✅ The five projects are **gone from Project Explorer**.
- ✅ The folder `C:\ti\AM335X_StarterWare_02_00_01_01\build\armv7a\cgt_ccs\am335x\` still contains `drivers.lib`, `mmcsdlib.lib`, `platform.lib`, `system.lib`, `utils.lib`.
- ✅ The source folders (`drivers/`, `mmcsdlib/`, `platform/`, `system/`, `utils/`) are untouched.

This is exactly what we want — the libraries are **compiled and sitting on disk**, but invisible inside CCS so the Project Explorer stays clean.

---

## 7. Import the portable sample programs

Now import the **portable demo projects** shipped in this repository.

1. **Project → Import CCS Projects...**
2. In the *Select search-directory* field, browse to the **`Examples/`** folder inside this workspace, e.g.:

   ```
   C:\D\MY\DEV\TI-CCS-IDE\Workspace_12\Examples
   ```

   > 💡 You can also point at the workspace root — CCS will recurse and pick up everything under `Examples/`. Pointing directly at `Examples/` is faster and avoids any future folders (e.g. tooling, scripts) that you might add at the workspace root.

3. CCS will scan the folder and list all the projects under `Examples/`. Tick the ones you want to play with.

   **Legend:**
   - *no suffix* — ported directly from the StarterWare `examples/<board>/` source. Application code is still the TI reference implementation, only the project structure was made portable.
   - `*(custom-built, StarterWare ref)*` — built from an **empty CCS project**. The application source is hand-written, but it still links against `drivers.lib` / `platform.lib` / `system.lib` / `utils.lib` from `C:\ti\AM335X_StarterWare_02_00_01_01` for the hardware drivers.

   **Projects:**

   - `boot` — secondary bootloader (loads `app` from MMC/SD or XMODEM)
   - `demo` — multi-driver showcase
   - `gpioLEDBlink` — GPIO blinky *(StarterWare reference)*
   - `AM3352_GPIO_LED` — minimal busy-wait blinky
   - `AM3352_GPIO_LED_DELAY` — blinky with IRQ-based `delay()`
   - `AM3352_GPIO_LED_TIMER` — blinky with polled DMTimer7
   - `AM3352_GPIO_LED_SEQUENCE` — 4-LED running-light animation
   - `AM3352_GPIO_INTERRUPT` — GPIO input interrupt on P9_12 (GPIO1[28]) + UART0 echo baseline
   - `AM3352_I2C_SCANNER` — I2C1 bus scanner on P9_17 (SCL) / P9_18 (SDA) @ 100 kHz
   - `AM3352_I2C_SSD1306_LCD` — SSD1306 OLED 128×32 driver over I2C1
   - `AM3352_ADC` — ADC AIN0 (P9_39) one-shot @ 500 ms over UART0
   - `dmtimerCounter` — DMTimer free-running counter
   - `AM3352_PWM_LED` — eHRPWM0A on P9_22 (GPMC_AD2, MUXMODE 6) (🚧 WIP — pin not toggling yet)
   - `wdtReset` — watchdog timer reset demo
   - `irqPreemption` — Cortex-A8 GIC IRQ preemption / nested-interrupt test
   - `neonVFPBenchmark` — NEON SIMD + VFPv3 floating-point benchmark
   - `uartEcho` — UART interrupt echo
   - `uartEcho_edma` — UART echo with EDMA-driven TX/RX
   - `uartEdma_Cache` — UART + EDMA + L1/L2 cache coherency combo
   - `enetEcho` — Ethernet L2 echo
   - `enetLwip` — LwIP TCP/IP stack with HTTP server
   - `AM3352_SPI_TX` — SPI0 TX baseline: continuous `0xAF` on P9_22/P9_18/P9_17 @ 100 kHz
   - `AM3352_SPI_ST7735` — ST7735 1.44" TFT LCD driver over SPI0 @ ~4 MHz
   - `edmaTest` — EDMA3 memory-to-memory copy
   - `hsMmcSdRw` — MMC/SD block read/write
   - `rtcClock` — RTC demo (🚧 WIP — currently being ported)

4. **Leave "Copy projects into workspace" UNCHECKED.** The projects in this repo are already self-contained and portable — copying them into a separate workspace folder defeats the purpose.

5. Click **Finish**.

Each project should appear in the Project Explorer with its own source files and a working build configuration.

---

## 8. Build & run a sample

Let's use `gpioLEDBlink` as the canonical sanity check:

1. Right-click `gpioLEDBlink` → **Build Project**.
2. Once built, connect your XDS debugger to the Antminer L3+ JTAG header.
3. Right-click `gpioLEDBlink` → **Debug As → Code Composer Debug Session**.
4. CCS will load the `.out` file and halt at `main()`. Press **F8** (Resume) to run.
5. An LED on the board should start blinking. 🎉

Repeat for any other project you imported.

---

## Troubleshooting

### ❌ "Cannot find file `gpio_v2.h`" (or similar header)

- Make sure StarterWare is installed at `C:\ti\AM335X_StarterWare_02_00_01_01`. If you installed it elsewhere, you'll need to edit the include-path variable in every `.ccsproject` file in this repo.

### ❌ "Cannot open file `drivers.lib`" at link time

- You skipped **Step 5**. Go back and build the five driver / library projects in CCS before linking the portable samples.
- Or you accidentally ticked **"Delete project contents on disk"** in Step 6 — the `.lib` files are gone. Reinstall StarterWare and rebuild the five projects.

### ❌ "Workspace already in use" / `.metadata` lock

- Another CCS instance is running against the same workspace folder, OR a previous CCS crashed and left `workspace/.metadata/.lock` behind. Close all CCS instances, then delete `.metadata/.lock` if present.

### ❌ "Project already exists" when re-importing

- You previously imported the project but didn't check the box to **delete from disk**. The `.project` file is still in the workspace folder. Either:
  - Use **Project → Existing Eclipse Projects** instead, or
  - Manually delete the `<project>/.project`, `<project>/.cproject`, `<project>/.ccsproject` files from inside the project folder (safe — they regenerate on import).

### ❌ Board doesn't respond to debugger

- Verify the JTAG connection (XDS100v2 / XDS200) is plugged into the correct header on the Antminer L3+.
- Verify the Antminer L3+ is powered externally (the JTAG does not power the board).
- For the `boot` project, ensure the target configuration's GEL file matches AM3352 (the AM3358 GEL will also work, but the AM3352 GEL avoids an irritating console warning).

---

## What you should have on disk when done

```
C:\ti\
├── ccs1281\                                    ← Code Composer Studio
└── AM335X_StarterWare_02_00_01_01\             ← StarterWare (source + built .lib files)

<your workspace>\
├── .metadata\                                  ← CCS workspace state (auto-generated)
├── RemoteSystemsTempFiles\                     ← RSE cache (auto-generated)
├── README.md
├── INSTALL.md                                  ← you are here
├── .gitignore
├── Doc\
│   └── bg.png                                  ← banner image
└── Examples\                                   ← portable CCS projects
    ├── AM3352_GPIO_LED\
    ├── AM3352_GPIO_LED_DELAY\
    ├── AM3352_GPIO_LED_SEQUENCE\
    ├── AM3352_GPIO_INTERRUPT\
    ├── AM3352_I2C_SCANNER\
    ├── AM3352_I2C_SSD1306_LCD\
    ├── AM3352_GPIO_LED_TIMER\
    ├── AM3352_ADC\
    ├── AM3352_PWM_LED\
    ├── AM3352_SPI_TX\
    ├── AM3352_SPI_ST7735\
    ├── boot\
    ├── demo\
    ├── dmtimerCounter\
    ├── edmaTest\
    ├── enetEcho\
    ├── enetLwip\
    ├── gpioLEDBlink\
    ├── hsMmcSdRw\
    ├── irqPreemption\
    ├── neonVFPBenchmark\
    ├── rtcClock\
    ├── uartEcho\
    ├── uartEcho_edma\
    ├── uartEdma_Cache\
    └── wdtReset\
```

---

<p align="center"><sub>Happy hacking on the Antminer L3+ — for fun and profit.</sub></p>