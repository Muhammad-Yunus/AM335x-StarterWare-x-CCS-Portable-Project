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
| Code Composer Studio | **[v12.8.1](https://www.ti.com/tool/download/CCSTUDIO/12.8.1)** — must include **Sitara AM3x ARM Processors** component |
| StarterWare Package | **version 02.00.01.01** — install **`AM335X_StarterWare_02_00_01_01`** at `C:\ti\` |
| Target Board | **Antminer L3+** (TI AM3352, Cortex-A8) — repurposed mining hardware |
| Emulator / Debugger | **J-Link** (tested & confirmed working) — solder a JTAG cable onto the Antminer L3+ board first. Follow the [BeagleBone Black JTAG connector soldering tutorial](https://dr-kino.github.io/2020/07/22/Beaglebone-black-soldering-jyag-connector/) (the JTAG footprint on the Antminer L3+ is in the same rear-edge position). XDS100v2 / XDS200 will also work if you prefer TI emulators. |

> The StarterWare driver libraries and include paths are pulled from:
> `C:\ti\AM335X_StarterWare_02_00_01_01`
>
> **Note on compatibility:** StarterWare 02.00.01.01 was originally written for the BeagleBone (AM335x). The Antminer L3+ uses the AM3352 — same ARM Cortex-A8 core, but **without the PRU** subsystems. Any demo that depends on PRU features will not work on this board. Stick to the GPIO / UART / Timer / MMCSD / RTC demos and you're good.

---

## Getting Started

Looking for the full step-by-step setup (download CCS, install StarterWare, import driver projects, build libraries, then import the portable samples)?

👉 **See [`INSTALL.md`](./INSTALL.md)** for the complete installation guide.

Quick summary:

1. Install **Code Composer Studio v12.8.1** into `C:\ti\ccs1281` with **Sitara AM3x ARM Processors** support.
2. Install **AM335x StarterWare 02.00.01.01** into `C:\ti\AM335X_StarterWare_02_00_01_01`.
3. In CCS, **Project → Import CCS Projects...** → point at the StarterWare root → tick **only `drivers`, `mmcsdlib`, `platform`, `system`, `utils`** → build all five (this produces the `.lib` files).
4. Delete those five projects from the Project Explorer **without** ticking "Delete project contents on disk" — the libraries stay on disk but disappear from the CCS view.
5. **Project → Import CCS Projects...** → point at the `Examples/` folder inside this workspace → tick the portable demos you want.
6. Build, launch a debug session, and run on the Antminer L3+.

---

## Project Index

> Each project below lives in its own folder under [`Examples/`](./Examples/) inside this workspace. Click the project name to jump straight to the folder.

### Core / Boot

- [**`Examples/boot/`**](./Examples/boot/) — Secondary bootloader (BL) that brings the AM3352 out of reset, configures the clock tree, PLLs and DDR, then loads the application image either from the **MMC/SD card** (FAT filesystem via the bundled `fat_mmcsd.c` + `ff.c`) or via **XMODEM over UART** as a fallback. Includes pin-mux setup, MMU/cache bring-up (`mmu.c`, `cache.c`, `cp15.asm`), CRC16 verification and the platform init glue (`bl_platform.c`, `board.c`, `device.c`). The canonical "hello world" of StarterWare — start here if you need a known-good baseline for custom boot flows on the Antminer L3+.
- [**`Examples/demo/`**](./Examples/demo/) — All-in-one showcase that exercises multiple StarterWare drivers (GPIO, UART, timers, etc.) from a single binary. Useful as a sanity check that your toolchain, libraries, and target are wired up correctly.

### GPIO

- [**`Examples/gpioLEDBlink/`**](./Examples/gpioLEDBlink/) — Toggles a user LED via GPIO at a fixed interval. The classic blinky demo for verifying pin-mux and GPIO configuration on AM335x — minimal, rock-solid, and the recommended first test on a fresh board.

### Timers

- [**`Examples/dmtimerCounter/`**](./Examples/dmtimerCounter/) — Configures a DMTimer in free-running counter mode and prints the tick value over UART. Demonstrates clock-source selection, prescaler setup, and reading the counter without interrupt overhead.
- [**`Examples/wdtReset/`**](./Examples/wdtReset/) — Enables the Watchdog Timer and intentionally lets it expire to force a system reset. Verifies that WDT servicing is required and provides a starting point for any application that needs a safety supervisor.

### Interrupt Handling

- [**`Examples/irqPreemption/`**](./Examples/irqPreemption/) — Test of **nested / pre-empting interrupts** on the Cortex-A8 GIC. Two or more interrupt sources of different priorities are wired so that a higher-priority IRQ is allowed to preempt a lower-priority one mid-handler, exercising the ARM Generic Interrupt Controller's preemption model. Indispensable for anyone writing real-time code on AM3352 — proves your ISR priorities and nesting rules are wired up correctly.

### Performance / SIMD

- [**`Examples/neonVFPBenchmark/`**](./Examples/neonVFPBenchmark/) — Benchmark of the Cortex-A8 **NEON SIMD** and **VFPv3 floating-point** units. Runs a series of fixed-point and floating-point workloads (vector add/mul, dot product, FFT-style loops, scalar-vs-vector comparisons) and prints cycle counts over UART. Use it to quantify how much speedup you get from compiling with NEON intrinsics vs. plain C, and to verify the VFP/NEON pipeline is healthy on a particular Antminer L3+ board.

### Communication

- [**`Examples/uartEcho/`**](./Examples/uartEcho/) — UART interrupt-driven echo server. Every byte received on the console UART is echoed back to the sender. Demonstrates UART pin-mux, FIFO configuration, and the interrupt-handler skeleton for serial protocols.
- [**`Examples/uartEcho_edma/`**](./Examples/uartEcho_edma/) — Same UART echo use-case as `uartEcho`, but with the **EDMA3 controller** moving bytes between the UART FIFO and RAM buffers instead of CPU-driven interrupt service. Demonstrates UART-triggered EDMA events, PaRAM linking for continuous RX, and how to keep the CPU out of the byte-pump loop on high-throughput serial links.
- [**`Examples/uartEdma_Cache/`**](./Examples/uartEdma_Cache/) — UART + EDMA + **L1/L2 cache coherency** combo. Echoes bytes through EDMA-managed buffers while exercising Cortex-A8 cache maintenance operations (`CacheDataClean` / `CacheDataInvalidate`) so the CPU sees the data the DMA just wrote, and the DMA sees the data the CPU just wrote. Critical reference for anyone shipping code that mixes DMA with the cache — without the right cache ops you get phantom bytes from RAM.
- [**`Examples/enetEcho/`**](./Examples/enetEcho/) — CPSW Ethernet echo using the StarterWare EMAC/GMII driver stack. Incoming Ethernet frames are looped back out the same interface — a great baseline for any L2 switch or TCP/IP offload project on AM335x.
- [**`Examples/enetLwip/`**](./Examples/enetLwip/) — Full **LwIP** (lightweight IP) TCP/IP stack demo running on top of the StarterWare EMAC driver. Comes with a minimal **HTTP server** (`httpd.c`) so the Antminer L3+ can serve a real webpage over Ethernet. Tweakable via `lwipopts.h`. Demonstrates network bring-up, DHCP/static IP, socket-style LwIP APIs, and how to host a web UI on a bare-metal Cortex-A8 with zero RTOS.

### Memory & DMA

- [**`Examples/edmaTest/`**](./Examples/edmaTest/) — Enhanced DMA (EDMA3) controller demo that configures a transfer channel and a param set, kicks off a memory-to-memory copy, and verifies that the destination buffer matches the source. Demonstrates region/instance setup, PaRAM entry programming, event triggering, transfer-completion polling and the EDMA interrupt hook. Essential reference for offloading bulk data movement away from the Cortex-A8 core.

### Storage

- [**`Examples/hsMmcSdRw/`**](./Examples/hsMmcSdRw/) — High-speed MMC/SD read/write demo that initializes an SD card via the MMCSD controller, performs block-level reads and writes, and verifies the round-trip. Use it to validate SD-card bring-up and the 8-bit MMCSD data path.

### Timekeeping

- [**`Examples/rtcClock/`**](./Examples/rtcClock/) — Real-Time Clock demo that initializes the AM335x RTC peripheral, sets a time/date, and reads it back. _⚠️ Work in progress — currently being ported; the RTC time value refuses to be set reliably and is still under investigation._

---

## Project Status

| Project | Status | Notes |
|---|---|---|
| `boot` | ✅ Stable | — |
| `demo` | ✅ Stable | — |
| `gpioLEDBlink` | ✅ Stable | — |
| `dmtimerCounter` | ✅ Stable | — |
| `wdtReset` | ✅ Stable | — |
| `irqPreemption` | ✅ Stable | — |
| `uartEcho` | ✅ Stable | — |
| `uartEcho_edma` | ✅ Stable | — |
| `uartEdma_Cache` | ✅ Stable | — |
| `enetEcho` | ✅ Stable | — |
| `enetLwip` | ✅ Stable | — |
| `edmaTest` | ✅ Stable | — |
| `neonVFPBenchmark` | ✅ Stable | — |
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
├── INSTALL.md
├── Doc/
│   └── bg.png                 ← banner image
└── Examples/                  ← all portable CCS projects live here
    ├── boot/                  ← CCS project: boot
    ├── demo/                  ← CCS project: demo
    ├── dmtimerCounter/        ← CCS project: dmtimerCounter
    ├── edmaTest/              ← CCS project: edmaTest
    ├── enetEcho/              ← CCS project: enetEcho
    ├── enetLwip/              ← CCS project: enetLwip
    ├── gpioLEDBlink/          ← CCS project: gpioLEDBlink
    ├── hsMmcSdRw/             ← CCS project: hsMmcSdRw
    ├── irqPreemption/         ← CCS project: irqPreemption
    ├── neonVFPBenchmark/      ← CCS project: neonVFPBenchmark
    ├── rtcClock/              ← CCS project: rtcClock (WIP)
    ├── uartEcho/              ← CCS project: uartEcho
    ├── uartEcho_edma/         ← CCS project: uartEcho_edma
    ├── uartEdma_Cache/        ← CCS project: uartEdma_Cache
    └── wdtReset/              ← CCS project: wdtReset
```

---

## Contributing

When adding a new demo:

1. Create a new folder under `Examples/` at the root of this workspace.
2. Import the StarterWare demo source into it as a standalone CCS project.
3. Replace `linkedResources` to source files with a single library/include path pointing at `C:\ti\AM335X_StarterWare_02_00_01_01`.
4. Verify the project builds clean and flashes to the target.
5. Add an entry to the **Project Index** in this README (with the `Examples/<name>/` prefix in the link).

---

## License & Credits

These projects are derived from Texas Instruments' **AM335x StarterWare** package. Refer to the original StarterWare documentation for licensing details of the underlying driver libraries.

<p align="center"><sub>Built for the BeagleBone • Powered by StarterWare 02.00.01.01</sub></p>