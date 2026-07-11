# AM335x StarterWare x CCS Portable Project

<p align="center">
  <img src="Doc/bg.png" alt="AM335x StarterWare Banner" width="500px">
</p>

> ⚠️ **Important — Use Case:** This project is **not** targeting the original BeagleBone. It is targeted at the **Antminer L3+** mining board, which is built around the **TI AM3352** (Cortex-A8) SoC. The PCB form factor is roughly similar to the BeagleBone Black, **but without the PRU subsystems**. This makes the Antminer L3+ a **low-cost option to repurpose** retired mining hardware as a learning platform for TI's bare-metal Cortex-A stack via StarterWare — "for fun and profit". The banner image above is the Antminer L3+ board itself.

<p align="center">
  <strong>A curated collection of standalone, portable StarterWare demo projects for the AM3352 (Antminer L3+) platform, ready to import into Code Composer Studio.</strong>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/platform-AM335x-orange?logo=texas-instruments" alt="Platform">
  <img src="https://img.shields.io/badge/toolchain-CCS%20v6%2B-blue?logo=texas-instruments" alt="CCS">
  <img src="https://img.shields.io/badge/language-C-green?logo=c" alt="Language">
  <img src="https://img.shields.io/badge/StarterWare-02.00.01.01-red" alt="StarterWare">
  <img src="https://img.shields.io/badge/status-active-success" alt="Status">
</p>

---

## Overview

This repository hosts **independent, portable** Code Composer Studio (CCS) projects derived from TI's **AM335x StarterWare 02.00.01.01** package. Each project has been extracted out of the monolithic StarterWare tree and converted into a **self-contained CCS project** that lives entirely inside its own folder.

Unlike a "copied" StarterWare project — which usually drags in hundreds of `linkedResources` pointing back into the source tree (`.../AM335X_StarterWare_02_00_01_01/...`) — these projects carry **only the application source** for the demo. The StarterWare driver and system libraries are referenced through a single, centralized **library/include path** on disk:

```
C:\ti\AM335X_StarterWare_02_00_01_01
```

This makes the projects:

- **Portable** — move the workspace anywhere, only update the StarterWare root if needed.
- **Clean** — no `linkedResources` from inside the project folder pointing into the StarterWare tree.
- **Reproducible** — every project is a standalone CCS C project you can import in one click.

---

## Prerequisites

| Component | Version / Detail |
|---|---|
| Code Composer Studio | v6.x or newer with ARM compiler support |
| StarterWare Package | `AM335X_StarterWare_02_00_01_01` installed at `C:\ti\` |
| Target Board | **Antminer L3+** (TI AM3352, Cortex-A8) — repurposed mining hardware |
| Emulator / Debugger | XDS100v2, XDS200, or equivalent (for flash/debug) |

> The StarterWare driver libraries and include paths are pulled from:
> `C:\ti\AM335X_StarterWare_02_00_01_01`
>
> **Note on compatibility:** StarterWare 02.00.01.01 was originally written for the BeagleBone (AM335x). The Antminer L3+ uses the AM3352 — same ARM Cortex-A8 core, but **without the PRU** subsystems. Any demo that depends on PRU features will not work on this board. Stick to the GPIO / UART / Timer / MMCSD / RTC demos and you're good.

---

## Getting Started

1. Install **AM335x StarterWare 02.00.01.01** into `C:\ti\AM335X_StarterWare_02_00_01_01`.
2. Open Code Composer Studio and select this workspace (`Workspace_12`).
3. **File → Import → Existing CCS Projects** → browse to this folder.
4. Pick the demos you want from the list and click **Finish**.
5. Build, launch a debug session, and run on the target.

---

## Project Index

> Each project below lives in its own folder inside this workspace. Click the project name to jump straight to the folder.

### Core / Boot

- [**`boot/`**](./boot/) — Secondary bootloader (BL) that brings the AM3352 out of reset, configures the clock tree, PLLs and DDR, then loads the application image either from the **MMC/SD card** (FAT filesystem via the bundled `fat_mmcsd.c` + `ff.c`) or via **XMODEM over UART** as a fallback. Includes pin-mux setup, MMU/cache bring-up (`mmu.c`, `cache.c`, `cp15.asm`), CRC16 verification and the platform init glue (`bl_platform.c`, `board.c`, `device.c`). The canonical "hello world" of StarterWare — start here if you need a known-good baseline for custom boot flows on the Antminer L3+.
- [**`demo/`**](./demo/) — All-in-one showcase that exercises multiple StarterWare drivers (GPIO, UART, timers, etc.) from a single binary. Useful as a sanity check that your toolchain, libraries, and target are wired up correctly.

### GPIO

- [**`gpioLEDBlink/`**](./gpioLEDBlink/) — Toggles a user LED via GPIO at a fixed interval. The classic blinky demo for verifying pin-mux and GPIO configuration on AM335x — minimal, rock-solid, and the recommended first test on a fresh board.

### Timers

- [**`dmtimerCounter/`**](./dmtimerCounter/) — Configures a DMTimer in free-running counter mode and prints the tick value over UART. Demonstrates clock-source selection, prescaler setup, and reading the counter without interrupt overhead.
- [**`wdtReset/`**](./wdtReset/) — Enables the Watchdog Timer and intentionally lets it expire to force a system reset. Verifies that WDT servicing is required and provides a starting point for any application that needs a safety supervisor.

### Communication

- [**`uartEcho/`**](./uartEcho/) — UART interrupt-driven echo server. Every byte received on the console UART is echoed back to the sender. Demonstrates UART pin-mux, FIFO configuration, and the interrupt-handler skeleton for serial protocols.
- [**`enetEcho/`**](./enetEcho/) — CPSW Ethernet echo using the StarterWare EMAC/GMII driver stack. Incoming Ethernet frames are looped back out the same interface — a great baseline for any L2 switch or TCP/IP offload project on AM335x.

### Storage

- [**`hsMmcSdRw/`**](./hsMmcSdRw/) — High-speed MMC/SD read/write demo that initializes an SD card via the MMCSD controller, performs block-level reads and writes, and verifies the round-trip. Use it to validate SD-card bring-up and the 8-bit MMCSD data path.

### Timekeeping

- [**`rtcClock/`**](./rtcClock/) — Real-Time Clock demo that initializes the AM335x RTC peripheral, sets a time/date, and reads it back. _⚠️ Work in progress — currently being ported; the RTC time value refuses to be set reliably and is still under investigation._

---

## Project Status

| Project | Status | Notes |
|---|---|---|
| `boot` | ✅ Stable | — |
| `demo` | ✅ Stable | — |
| `gpioLEDBlink` | ✅ Stable | — |
| `dmtimerCounter` | ✅ Stable | — |
| `wdtReset` | ✅ Stable | — |
| `uartEcho` | ✅ Stable | — |
| `enetEcho` | ✅ Stable | — |
| `hsMmcSdRw` | ✅ Stable | — |
| `rtcClock` | 🚧 WIP | RTC time-set failing — porting in progress |

---

## How These Projects Are Different

A typical StarterWare "copied project" pulls source files via Eclipse `linkedResources` like this:

```xml
<link>
  <name>src/gpio.c</name>
  <locationURI>PARENT-2-Project_LOC/gpio.c</locationURI>
</link>
```

That means moving the project breaks it — the `PARENT-N-Project_LOC` chain only resolves correctly if the project sits at the exact same depth inside the StarterWare tree.

**This repository does it differently.** Each project folder contains:

- The demo's own `main.c` and application source
- The CCS `.project`, `.cproject`, `.ccsproject` and launch files
- Build output (`Debug/`, `Release/`)

The only external dependency is the **StarterWare library + include path**, which is a single environment-variable / project-variable pointer. Update one variable, and every project follows.

---

## Folder Layout

```
Workspace_12/
├── README.md                  ← you are here
├── Doc/
│   └── bg.png                 ← banner image
├── boot/                      ← CCS project: boot
├── demo/                      ← CCS project: demo
├── dmtimerCounter/            ← CCS project: dmtimerCounter
├── enetEcho/                  ← CCS project: enetEcho
├── gpioLEDBlink/              ← CCS project: gpioLEDBlink
├── hsMmcSdRw/                 ← CCS project: hsMmcSdRw
├── rtcClock/                  ← CCS project: rtcClock (WIP)
├── uartEcho/                  ← CCS project: uartEcho
└── wdtReset/                  ← CCS project: wdtReset
```

---

## Contributing

When adding a new demo:

1. Create a new folder at the root of this workspace.
2. Import the StarterWare demo source into it as a standalone CCS project.
3. Replace `linkedResources` to source files with a single library/include path pointing at `C:\ti\AM335X_StarterWare_02_00_01_01`.
4. Verify the project builds clean and flashes to the target.
5. Add an entry to the **Project Index** in this README.

---

## License & Credits

These projects are derived from Texas Instruments' **AM335x StarterWare** package. Refer to the original StarterWare documentation for licensing details of the underlying driver libraries.

<p align="center"><sub>Built for the BeagleBone • Powered by StarterWare 02.00.01.01</sub></p>