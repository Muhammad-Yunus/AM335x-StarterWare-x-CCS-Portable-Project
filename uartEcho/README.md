# uartEcho — UART Echo Demo for BeagleBone Black

A minimal UART echo application for the TI AM335x (BeagleBone Black) using
**StarterWare 02.00.01.01** as the BSP and **Code Composer Studio 12.8.1** as
the IDE/toolchain. The firmware receives characters on UART0, echoes them back
per-byte, and once a line is complete (terminated by CR/LF **or** by an idle
gap) it prints the buffered line prefixed with `uart echo> `.

## What this project demonstrates

- UART0 polling-mode RX (no RX interrupt — simpler to reason about)
- TX via blocking `UARTCharPut` (no dropped bytes when the FIFO is full)
- Soft line-discipline: dispatch a line on CR/LF or after a short idle period
- Bare-metal C, no RTOS, no driverlib

## Toolchain

| Component       | Version    | Notes                                                                |
|-----------------|------------|----------------------------------------------------------------------|
| IDE             | CCS 12.8.1 | Installed at `C:\ti\ccs1281` (compiler shipped with the IDE)         |
| StarterWare BSP | 02.00.01.01 | Installed at `C:\ti\AM335X_StarterWare_02_00_01_01`                  |
| Target          | AM335x     | Tested on BeagleBone Black (BeagleBoneGreen should also work)        |
| Compiler        | TI CGT ARM | Bundled with CCS 12.8.1                                              |
| Serial console  | Hercules   | 115200 8N1, no local echo, no flow control                           |

## Portable layout

This project lives **outside** the StarterWare source tree on purpose — the
`uartEcho.c` and `uartEcho.cmd` here are the only files owned by the project.
All BSP headers and the GNU make build rule are picked up by relative paths
from `Debug/`. The StarterWare installation is referenced, not copied.

```
uartEcho/
├── uartEcho.c        # this project's firmware
├── uartEcho.cmd      # linker command file
├── Debug/            # generated build artefacts (.obj, .out, .map)
└── README.md
```

The link step reaches into StarterWare for runtime support, e.g.

```
C:/ti/AM335X_StarterWare_02_00_01_01/binary/armv7a/cgt_ccs/am335x/beaglebone/uart/Debug/uartEcho.out
```

so you can move or rename the project directory as long as StarterWare stays
at `C:\ti\AM335X_StarterWare_02_00_01_01`.

## Building

### From CCS

1. Launch `C:\ti\ccs1281\ccs\eclipse\ccstudio.exe`.
2. Open an existing workspace, e.g. `C:\D\MY\DEV\TI-CCS-IDE\Workspace_12`.
3. **File → Import → Existing CCS Projects into Workspace**, point it at
   `C:\D\MY\DEV\TI-CCS-IDE\Workspace_12\uartEcho`, click **Finish**.
4. Make sure the project's **Include Options** and **File Search Path** still
   resolve to:
   - `C:\ti\AM335X_StarterWare_02_00_01_01\include`
   - `C:\ti\AM335X_StarterWare_02_00_01_01\include\hw`
   - `C:\ti\AM335X_StarterWare_02_00_01_01\platform\beaglebone`
   - `C:\ti\AM335X_StarterWare_02_00_01_01\platform\utils`
5. Project → **Build All**.

The output binary lands in
`C:\ti\AM335X_StarterWare_02_00_01_01\binary\armv7a\cgt_ccs\am335x\beaglebone\uart\Debug\uartEcho.out`.

### From a plain shell (no CCS UI)

`gmake` is shipped with CCS at `C:\ti\ccs1281\ccs\utils\bin\gmake`. From a
PowerShell or CMD prompt:

```bat
cd C:\D\MY\DEV\TI-CCS-IDE\Workspace_12\uartEcho\Debug
C:\ti\ccs1281\ccs\utils\bin\gmake -k -s all
```

## Running

1. Open Hercules, connect to the BBB's UART0 (typically the FTDI cable on
   J1 — the header next to the micro-USB).
2. Configure: **115200 baud, 8 data bits, no parity, 1 stop bit, no flow
   control**.
3. Press the **Connect** button (the phone-icon) and **disable local echo**
   (the speaker-icon, or press **F4**) so Hercules only shows bytes the
   device sends.
4. Flash `uartEcho.out` to the BBB using `CCS Debug` or `LMFlashProgrammer`
   over USB.
5. After boot the device prints a banner. Type a line and press **Enter**,
   or type a string and pause (no Enter needed) — the response arrives
   ~100 ms after you stop typing.

## Expected output

```
=== uartEcho-NEWBUILD-2026-07-11-fingerprint-X9K2 ===
Type something and press Enter; echo will appear below.
hi
uart echo> hi

hello
uart echo> hello

worlds
uart echo> worlds
```

The fingerprint string `NEWBUILD-2026-07-11-fingerprint-X9K2` is a build-tag
that lets you confirm at a glance which image is currently running on the
target — bump it (or the date) whenever you change anything.

## Source layout (uartEcho.c)

| Section          | Purpose                                                                  |
|------------------|--------------------------------------------------------------------------|
| `txArray`        | Boot banner sent once on startup, chunked via `UARTFIFOWrite`.           |
| `cmdBuf` / `cmdLen` | Line-edited RX accumulator.                                           |
| `UartPutString`  | Blocking C-string TX helper, used by `DispatchLine`.                     |
| `DispatchLine`   | Emits `\r\n`, `uart echo> <buf>`, `\r\n`, `\r\n`.                        |
| RX polling       | In `main`'s super-loop, drains FIFO, echoes each byte, dispatches on LF. |
| Idle timeout     | Dispatches buffered bytes after ~100 000 idle iterations even without LF.|
| `UARTIsr`        | Drain-only: RX side is handled by polling so the ISR just clears state.  |

## Knobs

- `IDLE_LIMIT_HITS` (default `100000u`) — main-loop iterations with no new RX
  byte before a buffered line is auto-dispatched. Smaller = more responsive
  but riskier on a slow link; larger = safer but laggier.
- `CMD_BUF_SIZE` (default `64`) — maximum line length. Bytes past this limit
  are silently dropped.
