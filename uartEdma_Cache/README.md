# uartEdma_Cache — UART TX via EDMA with Cache/MMU Enabled

A sample application from **StarterWare 02.00.01.01** that demonstrates UART
transmit using the EDMA3 controller on the TI AM335x (BeagleBone Black), with
the MMU and L1/L2 cache both enabled. The firmware pushes two 26-byte buffers
from DDR RAM to UART0 through EDMA (no CPU copy), proving that cached memory
stays coherent with the DMA engine when the software follows the right cache
maintenance sequence.

## What this project demonstrates

- UART0 transmit via EDMA3 channel `EDMA3_CHA_UART0_TX` — the CPU is not in
  the byte-copy loop
- EDMA3 dummy PaRAM set (`DUMMY_CH_NUM = 5`) used as a transfer "link" so the
  same channel can be re-armed for the second TX without manual reconfiguration
- MMU set up for three regions — DDR (Normal, WB-WA / WT-NoWA), OCMC (same),
  and peripherals (Device, Execute-Never)
- L1 + L2 cache enabled after MMU (`CacheEnable(CACHE_ALL)`)
- For the second TX only, the `Cache_Clean` builds compile with
  `CACHE_FLUSH` defined, so `CacheDataCleanBuff` runs before EDMA is armed.
  This is the cache-coherence trick the example is built to teach: the CPU
  must push dirty lines to RAM before EDMA can see them.
- EDMA3 completion callback (`callback`) sets a `flag` that the main loop
  polls to know the transfer finished

## What this project does **not** do

- It does **not** receive or echo input. No EDMA channel is requested for
  UART0 RX, and no RX interrupt is registered. Once the two TX buffers are
  out, the firmware spins in `while(1);`. See the sibling `uartEcho` project
  for an interactive example.

## Toolchain

| Component       | Version    | Notes                                                                |
|-----------------|------------|----------------------------------------------------------------------|
| IDE             | CCS 12.8.1 | Installed at `C:\ti\ccs1281` (compiler shipped with the IDE)         |
| StarterWare BSP | 02.00.01.01 | Installed at `C:\ti\AM335X_StarterWare_02_00_01_01`                  |
| Target          | AM335x     | Tested on BeagleBone Black (BeagleBoneGreen should also work)        |
| Compiler        | TI CGT ARM 20.2.7.LTS | Codegen version baked into `.cproject` / `.ccsproject` |
| Serial console  | Hercules   | 115200 8N1, no local echo, no flow control                           |

## Portable layout

This project lives **outside** the StarterWare source tree on purpose — the
project is a self-contained Eclipse/CDT project that references the StarterWare
installation through **absolute paths** baked into `.cproject` and `.ccsproject`,
not via the original `PARENT-6-ORIGINAL_PROJECT_ROOT` linked-resource trick.
This means the project can be moved or renamed freely; only the StarterWare
install root matters.

```
uartEdma_Cache/
├── uartEdma_Cache.c    # this project's firmware (copied from StarterWare)
├── uartEdma_Cache.cmd  # linker command file (copied from StarterWare)
├── .cproject           # 4 build configurations: Debug, Release,
│                       # Debug_CacheClean, Release_CacheClean
├── .ccsproject         # device + codegen tool version
├── .project            # Eclipse project metadata (with build artefact links)
└── README.md
```

Source `.c` is **not** a linked resource pointing into StarterWare. To keep
the StarterWare install unmodified, `uartEdma_Cache.c` is a local copy. If
you want to pull updates from StarterWare, `cp` over it manually — do **not**
edit StarterWare's copy.

The 4 build configurations:

| Configuration         | `CACHE_FLUSH` defined? | Purpose                                          |
|-----------------------|------------------------|--------------------------------------------------|
| Debug                 | no                     | MMU + cache on, no explicit cache flush          |
| Release               | no                     | optimised, same as Debug w.r.t. cache            |
| Debug_CacheClean      | yes                    | `CacheDataCleanBuff` before second EDMA arming   |
| Release_CacheClean    | yes                    | optimised + cache flush                          |

The two clean-cache configs exist to show the contrast — without the flush,
the second 26-byte block may come out garbage on hardware that actually has
dirty cache lines for that buffer. With the flush it always comes out clean.

## Building

### From CCS

1. Launch `C:\ti\ccs1281\ccs\eclipse\ccstudio.exe`.
2. Open an existing workspace, e.g. `C:\D\MY\DEV\TI-CCS-IDE\Workspace_12`.
3. **File → Import → Existing CCS Projects into Workspace**, point it at
   `C:\D\MY\DEV\TI-CCS-IDE\Workspace_12\uartEdma_Cache`, click **Finish**.
4. Confirm the project's **Include Options** still resolve to:
   - `C:\ti\AM335X_StarterWare_02_00_01_01\include`
   - `C:\ti\AM335X_StarterWare_02_00_01_01\include\hw`
   - `C:\ti\AM335X_StarterWare_02_00_01_01\include\armv7a`
   - `C:\ti\AM335X_StarterWare_02_00_01_01\include\armv7a\am335x`
5. Pick a configuration (e.g. **Debug_CacheClean**) and **Project → Build**.

The output binaries land in
`C:\ti\AM335X_StarterWare_02_00_01_01\binary\armv7a\cgt_ccs\am335x\beaglebone\cache_mmu\<Config>\uartEdma_Cache_Cache.out`
(Debug / Release) or `..._CacheFlush.out` (the `_CacheClean` configs).

## Running

1. Open Hercules, connect to the BBB's UART0 (typically the FTDI cable on
   J1 — the header next to the micro-USB).
2. Configure: **115200 baud, 8 data bits, no parity, 1 stop bit, no flow
   control**.
3. Press **Connect** (the phone-icon) and **disable local echo**
   (the speaker-icon, or press **F4**) so Hercules only shows bytes the
   device sends.
4. Flash `uartEdma_Cache_Cache.out` (or `_CacheFlush.out` for the clean
   build) using `CCS Debug` or `LMFlashProgrammer` over USB.

## Expected output

Immediately after boot, exactly two 26-byte bursts — no prompt, no echo of
anything you type:

```
abcdefghijklmnopqrstuvwxyz
ABCDEFGHIJKLMNOPQRSTUVWXYZ
```

Anything you type after that is ignored — the firmware is now spinning in
`while(1);`. If you need interactive RX, use the `uartEcho` project instead.

## Source layout (uartEdma_Cache.c)

| Section              | Purpose                                                                  |
|----------------------|--------------------------------------------------------------------------|
| `MMUConfigAndEnable` | Three-region page table: DDR, OCMC, device peripherals                   |
| `main`               | MMU → cache → UART/EDMA init → fill `a..z` → TX1 → fill `A..Z` → TX2    |
| `UartTransmitData`   | Fills EDMA3 PaRAM for UART0 TX, arms a dummy-link, enables the transfer  |
| `EDMA3Initialize`    | Reset controller, un-mask IRQ, route EDMA completion/error into AINTC    |
| `edma3ComplHandler`  | IPR scanner; clears bits in ICR and invokes per-TCC callback             |
| `callback`           | Sets `flag = 1` so the polling loop in `main` falls through              |
| `TxDummyPaRAMConfEnable` | PaRAM set that the real transfer chains to — gives clean re-trigger |
| `UARTInitialize`     | Module reset, DMA Mode-1 pin-mux, Config-Mode-B, 8N1                     |
| `UartBaudRateSet`    | Divisor for 115200 baud at 48 MHz, oversampling ×42                      |

## Knobs

- `TX_BUFFER_SIZE` (default `26u`) — size of each TX block. Drop it to `1` if
  you want to see per-byte EDMA completion callbacks firing.
- `BAUD_RATE_115200` (`115200u`) — change to e.g. `9600` for slow-log
  experiments, and adjust `UART_MODULE_INPUT_CLK` if your clock tree differs.
- `DUMMY_CH_NUM` (`5u`) — the PaRAM slot used as the dummy link target; any
  free slot works, but reusing the same slot every transfer is the trick that
  makes the re-trigger cheap.
- `CACHE_FLUSH` — defined only in the two `*_CacheClean` configurations
  (toggle by hand in `.cproject` if you want to flip a single build).
