# hsMmcSdRw

High-Speed MMC/SD card read/write example for BeagleBone (AM335x), ported from
Texas Instruments AM335X StarterWare v02.00.01.01.

## Description

This project is derived from the StarterWare example
`examples/beaglebone/hs_mmcsd/`. It demonstrates how to use the High-Speed MMCSD
(HSMMCSD) controller on the AM335x to read and write files to an SD card using
the FatFs filesystem library.

The project is **portable**: it can live anywhere on disk and does not depend
on being inside the StarterWare source tree. All source files have been copied
locally and all build paths in `.cproject` are absolute references to the
StarterWare installation root.

## Requirements

| Component                | Version / Location                                          |
|--------------------------|-------------------------------------------------------------|
| Code Composer Studio     | CCS 12.8.1 (`C:\ti\ccs1281\ccs`)                            |
| ARM Compiler (TI CGT)    | ti-cgt-arm 20.2.7.LTS                                       |
| StarterWare installation | `C:\ti\AM335X_StarterWare_02_00_01_01\`                     |
| Target board             | BeagleBone (AM335x, Cortex-A8)                              |

If StarterWare is installed at a different location, edit the absolute paths
in `.cproject` (search for `C:/ti/AM335X_StarterWare_02_00_01_01/`).

## Building the mmcsdlib dependency

The HS MMCSD low-level driver is provided by StarterWare as a prebuilt static
library. Only the **Release** variant ships with StarterWare out of the box, so
the `mmcsdlib` project must be built once for **Debug** before this project can
link successfully.

Steps:

1. In CCS, import the project located at:
   ```
   C:\ti\AM335X_StarterWare_02_00_01_01\build\armv7a\cgt_ccs\mmcsdlib\
   ```
2. Set its active build configuration to **Debug** (CCS only ships the
   Release `.lib`, the Debug variant must be generated locally).
3. Build it. The output is:
   ```
   C:\ti\AM335X_StarterWare_02_00_01_01\binary\armv7a\cgt_ccs\mmcsdlib\Debug\libmmcsd.lib
   ```

This is a one-time setup; the resulting `.lib` is reused across all Debug
builds of `hsMmcSdRw`.

## Building hsMmcSdRw

Because the project is portable, this folder can live anywhere on disk. It
keeps its references to StarterWare via the absolute path configured in
`.cproject`, so simply open it in CCS and build:

1. Open this folder as a CCS project (it is a portable copy of the original
   StarterWare example, not a checked-out subtree of the StarterWare tree).
2. Select the **Debug** build configuration.
3. Build the project.

## Project layout

```
hsMmcSdRw/
├── .cproject              # Build definitions (absolute paths to StarterWare)
├── .project               # Eclipse project metadata
├── .ccsproject            # CCS-specific project metadata
├── hsMmcSdRw.cmd          # Linker command file (from StarterWare)
├── crc16.c / crc16.h      # CRC16 (xmodem)
├── xmodem.c               # XMODEM protocol
├── ff.c                   # FatFs filesystem core
├── fat_mmcsd.c            # FatFs glue to HSMMCSD driver
├── hs_mmcsd_fs.c          # Filesystem demo over HSMMCSD
├── hs_mmcsd_rw.c          # Raw HSMMCSD read/write demo
└── Debug/                 # Generated build artefacts (delete to clean)
```

## How portability is achieved

- All `.c` sources originally from StarterWare have been copied into this
  project folder, so the project is self-contained in terms of source code.
- All references to the StarterWare tree in `.cproject` use **absolute paths**
  (`C:/ti/AM335X_StarterWare_02_00_01_01/...`) rather than the
  `${ORIGINAL_PROJECT_ROOT}/../../../../../../../...` relative form that
  CCS generates when copying a project out of the original example.
- The compiler version is fixed to `20.2.7.LTS` in both Debug and Release
  configurations.
- The `.project` file no longer carries `linkedResources` or `variableList`
  entries pointing back into the StarterWare source tree.

The only remaining external dependency is the StarterWare installation root
(referenced for headers, libraries, and the post-build image tools) and the
StarterWare `tools/ti_image/tiimage.exe` used by the post-build step.

## Running on the target

1. Connect the BeagleBone via JTAG.
2. Insert a FAT-formatted SD card into the MMCSD slot.
3. Launch the Debug configuration — CCS will load `hsMmcSdRw.out` onto the
   target.
4. Open a serial console (UART0, 115200 8N1) to interact with the demo menu.

## Serial console output

When the demo is running, the UART console exposes a small shell with the
following commands:

```
Available commands
------------------
help : Display list of commands
h    : alias for help
?    : alias for help
ls   : Display list of files
chdir: Change directory
mkdir: Create directory
cd   : alias for chdir
pwd  : Show current working directory
cat  : Show contents of a text file : cat <FILENAME>
       Write to a file : cat <INPUTFILE> > <OUTPUTFILE>
       Read from UART : cat dev.UART
       Write from UART : cat dev.UART > <OUTPUTFILE>
rm   : Delete a file or an empty directory
```

Example session:

```
/> mkdir data
/> ls
D-HS- 2026/07/05 08:23         0  SYSTEM~1
D---- 2007/06/05 11:38         0  data

   0 File(s),         0 bytes total
   2 Dir(s),   15140832K bytes free
/>
cd data
/data> pwd
/data
/data> mkdir test
/data> cd test
/data/test>
```
