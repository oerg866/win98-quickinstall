![](assets/logobig.png)

# Windows 9x QuickInstall - (C) 2012 - 2026 E.Voirin (oerg866)

```
Windows is a trademark that belongs to Microsoft Corporation.
This project has NO ENDORSEMENT FROM ANY INVOLVED PARTIES, SUCH AS MICROSOFT.
Please don't sue. I just like old computers :(
```

| | | |
|--------------------------|--------------------------|--------------------------|
| ![](assets/install1.png) | ![](assets/install2.png) | ![](assets/install3.png) |
| ![](assets/install4.png) | ![](assets/install5.png) | ![](assets/install6.png) |
| ![](assets/windows1.png) | ![](assets/windows2.png) |                          |

## What is QuickInstall?

Windows 9x QuickInstall is a framework intended to replace the original Windows 9x setup environment and in doing so provide an extremely quick installation process paired with integrated patches and drivers. In that sense, it is not dissimilar to *nlite*, but the method used to achieve this is very diifferent.

It uses a Linux-based custom installation environment that quickly copies a full installation image to a target directory of your choosing.

In fact, doing it this way opens up this old operating system to a lot of modern hardware, giving you the possibility to add patches, drivers and tools to tailor the resulting installation image to your needs. 

**You can finally install Windows 9x from USB without any fuss!**
Or from a CD/DVD-R. Or you can boot from a floppy disk!

It does not matter how you boot the system or where the QuickInstall data is located, the installation environment will find it!

This also means that on older systems, you can install it from USB, but boot the machine from floppy disk!

The custom Linux-based installer is paired with some tools to allow hard disk partitioning and formatting, and uses a custom data packing method that is optimized for streaming directly from a CD or DVD-ROM to the hard disk without any seeking.

While this means that the effort for a user to build an ISO is higher than for example nLite, but the degree of customizability is also massively increased.

A set of reference images is also provided to get you installing your favorite flavor of vintage Windows as fast as possible!

## How "Quick" is QuickInstall *really*?

Depends where!

- In VirtualBox on a modern machine? 15 seconds.
- On a Pentium III 866MHz with a DVD-ROM and an ATA133 disk? 60 seconds.
- On a 486 DX4-100? 4-5 minutes.

In *any* case, QuickInstall is at least an ***order of magnitude (i.e. 10x)*** faster than the official Windows 98 `setup.exe`

## Reference Images (aka. I don't want to read and/or do all of this.)

The reference images come in three flavors:

- Windows 98 SE: **Stock installation**
- Windows 98 SE: **98Lite Micro De-Bloated installation with DirectX 8.1**
- Wiindows ME: **98Lite Micro De-Bloated installation with DirectX 8.1**

### Reference Image Content:

* Microsoft Patches (QFEs), Windows 98 SE:
  - Q239696, Q239887, Q245682, Q253697, Q253711, Q269601, Q270063, Q273017, Q276602, Q280448, Q281533, Q288430, Q291362, Q293793, Q293197, Q306453, Q381307
* Microsoft Patches (QFEs), Windows ME:
  - Q268452, Q276602, Q278289, Q280800, Q280127, Q290831, Q296773, Q301453, Q301540, Q305826
* Common patches:
  - Microsoft Installer 2.0
  - R. Loew RAM Patch
  - R. Loew SATA Patch
  - R. Loew LBA48 Patch
  - SweetLow patches:
    - Patch for RLoew's AHCI.PDR 3.0 - disabled nonfunctional handler of IDE_PASS_THROUGH to prevent BSOD on SMART access
    - Patch for (the latest version of) APIX.VXD (for Windows 98SE) - disabled special handling for devices on Port Driver ESDI_506.PDR
    - Patches for ESDI_506.PDR from Windows 98SE / RLoew's Terabyte Plus Pack 2.1
    - Patches for SMARTVSD.VXD - works for any combination of ATA channels and devices on these channels and more
    - Right .INF for PCI Bus Master ATA Controllers including those in PCI Native Mode with RLoew's "SATA" Patch/Terabyte Package
    - Bug fix VMM.VXD from Windows 98(SE) & ME on handling >4GiB addresses and description of problems with resource manager on newer BIOSes
  - Microsoft Layer for Unicode (unicows)
  - Windows 9x TLB invalidation bug patch (Patcher9x by JHRobotics)
  - oerg866 SYSDMCPL HW Detection Speedup Patch
  - oerg866 DOS Mouse Acceleration Fix
  - WinRAR 3.93 pre-installed

* Extras inside `extras` folder:
  - **Microsoft Updates**: DirectX 9.0C (7-2007), .NET 2.0, IE6 SP1, VB6 / VC6 / VC2005 Runtime, Directory Services Client
  - **KernelEx**: - v4.5.2, v4.5.2016.18 Update, 4.22.26pre2 Update, 4.22.25.2-TMT Cumulative Installer

    **NOTE: This can brick your system, especially on the 98Lite versions this should not be used**
  - Revolutions Pack 9.7 + Updates + Extra Fonts / Themes
  - **Benchmarks**: 3DMark 99 + 2000 + 2001 SE, Super Pi Mod, Roadkill Disk Speed, Atto Disk Benchmark
  - **CPU/Hardware Tools**: CPU-Z Vintage Edition, CPUFSB, HWiNFO32, HDAT2, K6INIT, WPCREDIT, PCIEDIT, Video Memory Tester (VMT / VMTCE)
  - **Utilities**: Total Commander, Paragon NTFS, IrfanView, TCP Optimizer
  - **Drivers**:
    - ALi AGP (1.90, 1.82) + Utility (1.40), ALi Integrated (2.092)
    - AMD: AMD75x Driver Pack (1.20), AMD76x Driver Pack
    - VIA 4in1 (4.35, 4.43), VIA IDE Driver (3.20B with RLOEW fix), VIA Latency Patch
    - SiS: 5600/600 AGP, SiS 961/964 IDE, SiS 964 RAID, SiS IDE 2.13, UIDE1.02, AGP/USB/ATA133 1.21
    - Intel: INF Installer (6.3.0.1007)
    - nVidia: GeForce (45.23, 81.98), nForce (4.20)


## Building the Framework, Bugs, License, etc.

See [BUILDING.md](./BUILDING.md).

# Supported Target Operating Systems

* Microsoft Windows **98** (Build 4.10.1998)
* Microsoft Windows **98 Second Edition** (Build 4.10.2222)
* Microsoft Windows **Millenium Edition** (Build 4.90.3000)

Support for international versions is not properly tested. It should work and in my testing it does, but YMMV. Please report bugs!

**NO versions of Windows 95 are supported due to non-PNP device detection being part of the DOS-based installer stage.**

# System requirements to use a QuickInstall Image
  - i486-class CPU, at least a 486SX (but it will be very slow)
  - 24 MiB of memory
  - An IDE / SATA / SCSI controller supported by Linux

# How to boot a QuickInstall Image

There are several provided methods to boot into Windows 98 QuickInstall:

1. CD/DVD-ROM boot

   The Windows 9x QuickInstall ISO image can be booted on any computer that supports floppy-emulation CD-ROM boot.

   **Recommended if you have a PC that supports CD-ROM boot.**

1. 1.44M floppy boot with DOS (`dosflop.img`)

   This is a 1.44M floppy disk image that contains FreeDOS and `LOADLIN` to boot the kernel directly off the CD-ROM.

   **Recommended** if you have a computer that does not support CD-ROM boot or wish to install from a hard disk.
   
   *Also recommended if you have the QuickInstall files on an ATA/ATAPI media or other device that is exposed using Int 13h*
   
   If you have a SCSI CD-ROM, the image must be modified accordingly.


1. 1.44M floppy boot with tiny kernel (`floppy.img`)

   This is a 1.44M floppy disk image that contains a proper kernel.

   **Recommended only** if you have QuickInstall on a non-ATA/ATAPI media and your BIOS does not support CD-ROM boot.

   **The kernel on this image is limited in functionality, driver support and does not enable kernel logs (no `printk`, no `dmesg`)**

   Due to the strong compression, it takes a very long time to boot on slow systems. It is only meant as a last resort.

1. Removable media such as USB Flash Drives

   Using the system preparation script it is possible to create bootable USB images. The steps to do so are described below.

# System requirements to build QuickInstall **Images**

- **Windows 7, 8.1, 10 or 11**

  OR
  
  **Modern Linux variant or *WSL***

  Tested with:

  * **Windows 10** 21H2, Build 19045.6456
  * Windows Subsystem For Linux (Ubuntu 22.04)
  * Ubuntu 22.04 (native)

- `python` (3.8 or newer)

  * On **Windows 7 and 8.1**:

    Download a python installation package from https://www.python.org/downloads/

  * On **Windows 10 and 11**:
  
    Use the Microsoft Store to download an appropriate variant.
  
  * On **Linux (Debian, Ubuntu)**:

    `sudo apt install python3.8`

- **Script requirements**

  Run the following command in the framework directory:

  `pip3 install -r requirements.txt`

  ***WARNING:*** *There are no third-party tools to modify Windows 9x-format registry DAT files, but doing so is an essential part of the image building process. This uses an MS-DOS emulation layer and the 16-Bit part of the Windows 9x registry editor (`regedit.exe`). This means that currently, on platforms other than Windows, `wine` is needed to be installed:*

  - `sudo apt install wine` (Debian, Ubuntu, ...)
  - `sudo pacman -S wine` (Arch, ...)

- A Hard Disk image (IMG/RAW, VHD, VMDK, VDI) containing a fully set up Windows 9x installation according to the guidelines below.

# The system preparation script (`sysprep.py`) 

  This script serves the purpose of preparing an installation for packaging into an ISO and/or USB image file.

  It takes the following parameters:

  * `--iso <ISO>`
    
    Instructs the script to create an ISO image with the given file name
  
  * `--usb <USB>`

    Instructs the script to create an USB key image with the given file name
  
  * `--osroot <Image File> <Display Name>`

    Specifies a Windows 98 / ME hard disk image as the source.
    
    `Display Name` is the name this image will have in the Installer selection (if you build an output image with more than one OS on it)

    **This can be specified multiple times, in which case the installation wizard will show a selection menu.**

  * `--extra <EXTRA>`

    Instructs the script to add the files in this directory to the final ISO/USB output.

    Default: `_EXTRA_CD_FILES_` in the framework directory

    **This can be specified multiple times, all the files in all the directories will be added in this case**

    The data will be put into the `extra` folder in the output image

  * `--drivers <DRIVERS>`

    Instructs the script to process and *slipstream* all drivers in this directory.

    *Slipstream* means that these drivers will be installed automatically when the hardware for them is detected at any point of the installation's lifetime, even when the hardware is not yet present at the time of installation.

    Default: `_DRIVER_` in the framework directory. It already contains a curated selection of drivers (from the `win98-driver-lib-base` repository)

    **This parameter can only be specified once.**

  * `--extradrivers <EXTRADRIVERS>`

    Instructs the script to process all drivers in this directory and add them into the extra drivers directory.

    **These drivers are NOT slipstreamed** and thus not automatically installed. They are however made available on the resulting installation media and can be installed by pointing the Windows 98 / ME `Add New Hardware` wizard to the `DRIVER.EX` directory on the output image.

    The reason for this folder's existence is the vast selection of hardware available for the operating systems and the varying compatibility / size of them.
    
    Very large drivers are recommended to go in here, as well as drivers for which it cannot be assumed that different versions have different compatibility and speed.

    For example, it is better to choose an older driver for an older *nVidia GeForce* card even though a newer one would also support this hardware for speed reasons, whilst the newer driver should also be available, in case newer hardware is present.

    Default: `_EXTRA_DRIVER_` in the framework directory. It already contains a curated selection of drivers.

    **This parameter can only be specified once.**

  * `--verbose VERBOSE`

  This parameter controls console output verbosity of the script.

  Where `VERBOSE` is either `True` or `False`.

  The default is `False`.

  **This parameter is currently broken, sorry. It's always quiet.**

# Preparing a Windows 98 / ME image for use with `sysprep.py`

- Install Windows 98 / ME in a virtual machine or emulator, just as you want it.
  I recommend using 86Box using the following configuration:

  - Machine:
    - Machine Type: Slot 1
    - Achine: [i440BX] ABIT BF6
    - CPU type: Intel Pentium II (Deschutes)
    - Memory: 64 MB
  - Display:
    - Video: [ISA] VGA (or any video card that does not have an integrated driver in Windows 98 / ME)
  - Network:
    - None, ***VERY IMPORTANT TO MAKE SURE NETWORK DRIVER SETUP STAYS INTACT!!***
  - Hard disks:
    - IDE (0:0), big enough to install the operating system.

  It is recommended that you install Windows in APM mode, because ACPI is a buggy mess (`setup /p i`).

  **WARNING:** The operating system ***must*** be installed from the HARD DISK and it must contain a folder containing the Windows 98 CAB files from the CDROM.

  **NOTE:** It is recommended that NO extra drivers are *installed* in this VM. They can be added using the `--driver` parameter

- Configure the Windows 98 / ME installation as you wish. Examples:
  - Machine name
  - User name
  - System options
  - Themes
  - Patches
  - Utilities
  - Software

- Shut down the virtual machine and take note of the image file name.

**WARNING:** Only *one* Windows directory is allowed, and only *one* Windows setup files CAB directory. If you install using 98Lite, make sure that the 98Lite setup directory is the sole carrier of these CABs.

*INFO: The script detects the Windows directory by finding `WIN.COM`*
*INFO: The CAB file directory is detected by finding `PRECOPY2.CAB`*

# Preparing & Packaging

- Copy drivers that you want slipstreamed into a directory of your choice. By default this is `_DRIVER_` in the framework directory directory.

  *NOTE: `_DRIVER_` is already filled with a curated selection of drivers. You can remove these, if you wish.*

  **If you choose a non-default directory for this, you must specify it using the `--drivers` parameter.**

- Copy extra drivers that will not be slipstreamed but added to a directory of your choice. By default this is `_EXTRA_DRIVER_` in the framework directory.

  *NOTE: These drivers will be processed in the same way as the slipstreamed ones but will not be copied to the hard drive during installation.*

  *INFO: This folder will be named `DRIVER.EX` on the ISO. You can point the Windows 98 hardware wizard to this folder and the drivers will be found and installed correctly.*

  *NOTE: `_EXTRA_DRIVER_` is already filled with a curated selection of drivers. Ycan remove these, if you wish.*

  **If you choose a non-default directory for this, you must specify it using the `--extradrivers` parameter.**

- Add any extra files you wish added to the ISO to a directory of your choice. By default this is `_EXTRA_CD_FILES` in the framework directory.

  This can include drivers that you do not wish to be processed with the QuickInstall tools, e.g. drivers that contain bundled software.

  **If you choose a non-default directory for this, you must specify it using the `--extra` parameter.**

- Run the following command to build the package:

  `sysprep.py --osroot <OS Root Folder>`

  NOTE: *You must add the other parameters if you deviate from the defaults.*

  This will build the installation package in the `_OUTPUT_` directory in the Framework directory.

  If you want to create an ISO image or a bootable USB key image, see below. 

  **WARNING: THE OS DIRECTORIES WILL BE MODIFIED IN PLACE!**

## Creating a bootable ISO image

- Use the `--iso` parameter for the `sysprep.py` script.

  e.g. adding `--iso output.iso` to the command line will yield a file named `output.iso` that can be burned to a CD/DVD/Blu-Ray or used in a virtual machine.

  Refer to the parameter descriptions above for more information.

## Creating a bootable USB key image

- Use the `--usb` parameter for the `sysprep.py` script.

  e.g. adding `--usb output.img` to the command line will yield a file named `output.img` that can be written to a USB key, SD or CF card, hard disk or other media.

  Refer to the parameter descriptions above for more information.

### How to write the bootable USB image to a USB flash drive

- On Linux, you can use `dd`
  - `dd if=<USB image file> of=/dev/sdX bs=1024k status=progress`
    
    Replace `/dev/sdX` with the USB flash drive's device path.

- On Windows, you can use the following tools:
  - `dd` For Windows: http://www.chrysocome.net/dd
  - `Win32 DiskImager`: https://sourceforge.net/projects/win32diskimager/

# Packaging multiple operating systems in one image

This is an advanced feature.

By specifying the `--osroot` parameter multiple times, you can create a multi-variant installation image. In this case a selection menu will appear during installation prompting the user which variant should be installed.

Example:

`python3 sysprep.py --osroot D:\quickinstall\Windows98SE --osroot D:\quickinstall\WindowsME --iso multi.iso`

## `win98qi.inf`

This file should be present in every *OS Root* directory. **It contains the display name of the installation in the variant selection menu shown in the installer.**

It should fit on the screen.

It can be ASCII or UTF8 encoded.

# FAQ

## Q: I'm getting a python error about non-zero return code in `msdos.exe` right after `Using SHELL32.xxx to reboot!`

Example:
```
subprocess.CalledProcessError: Command '['L:\\win98-installer\\__BIN__\\tools\\msdos.exe', 'L:\\win98-installer\\__BIN__\\registry\\regedit.exe', '/L:SYSTEM.DAT', '/R:USER.DAT', 'tmp.reg']' returned non-zero exit status 1.
```

A: This problem happens when running the script on Windows whilst the script directory is in a share hosted by a WSL session (Windows Subsystem for Linux). This causes some incompatibilities. Run the script from the WSL Linux shell instead.

## Q: I'm getting I/O and read errors, segmentation faults and other weird behavior when installing from CD on an Intel i430 / i440-based system with an Intel 82371SB south bridge (e.g. i440FX)

A: This problem has been verified by Deksor, Rigo and myself, and is a deeply rooted problem that has existed since at least version 2.4.xx. Operating the drives in PIO mode can help.

A BIOS update may help, the issue is currently under investigation as we found some BIOS versions where this problem does not occur.

For now, you can work around this problem by using a PCI SCSI or IDE adapter card that supports CD-ROM boot or has DOS drivers with the **DOS boot floppy option**.

You can also try disabling DMA when booting the CD/Floppy.

## Q: I'm trying to install on a VIA MVP3-based motherboard and I'm getting a "General Protection Fault" on the first boot. (Repoted by Rigo)

A: To work around this issue, select the "slow" hardware detection variant in the installation wizard. The problem is currently under investigation.

## Q: I'm trying to install on my 486 and I'm getting Disk I/O errors!

A: Your BIOS might have an incomplete/buggy LBA implementation. Partition the drive to use a FAT32 non-LBA partition and try again.

You can also try disabling DMA when booting the CD/Floppy.
