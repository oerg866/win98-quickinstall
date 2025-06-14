*****************************************************************************
                 Windows 9x QuickInstall ISO Creator Package

                       (C) 2012-2024 Eric Voirin (oerg866)
*****************************************************************************

# Disclaimer

**Windows is a trademark that belongs to Microsoft Corporation.**

This project has ***NO ENDORSEMENT FROM ANY INVOLVED PARTIES, SUCH AS MICROSOFT.***

Please don't sue. I just like old computers :(


# Windows 9x QuickInstall

| | | |
|--------------------------|--------------------------|--------------------------|
| ![](assets/install1.png) | ![](assets/install2.png) | ![](assets/install3.png) |
| ![](assets/install4.png) | ![](assets/install5.png) | ![](assets/install6.png) |
| ![](assets/windows1.png) | ![](assets/windows2.png) |                          |

## Description

This is a framework that is intended for creating and preparing Windows 98 installation ISO images in a way that is optimized for extremely quick installation, similar to *nLite*, but with a completely different method and context.

It takes the root file system of an already installed Windows 98 system and packages it, whilst allowing drivers and tools to be slipstreamed at will.

For the installer, it uses Linux as a base, paired with some tools to allow hard disk partitioning and formatting, as well as  a custom installer with a custom data packing method that is optimized for streaming directly from CD to the hard disk without any seeking.

Bottom line, this means that the effort for the user to build an ISO is higher than for example nLite, but the degree of customizability is also massively increased.

## How fast is it really?

On a Pentium III class machine with ATA / ATAPI Ultra DMA available for all storage devices, Windows 98 -- using an ISO built with this framework -- can be installed from CD in roughly 60-90 seconds.

# Building the Framework, Bugs, License, etc.

See [BUILDING.md](./BUILDING.md).

# I don't want to read and/or do all of this.

Okay. Go to the releases tab :-)

# Supported Target Operating Systems

* Microsoft Windows **98** (Build 4.10.1998)
* Microsoft Windows **98 Second Edition** (Build 4.10.2222)
* Microsoft Windows **Millenium Edition** (Build 4.90.3000)

Support for international versions is not properly tested. It should work and in my testing it does, but YMMV. Please report bugs!

**NO versions of Windows 95 are supported due to non-PNP device detection being part of the DOS-based installer stage.**

# System requirements to use QuickInstall
  - i486-class CPU, at least a 486SX (but it will be very slow)
  - 24 MiB of memory
  - An IDE / SATA / SCSI controller supported by Linux

# How to boot a QuickInstall image

There are several provided methods to boot into Windows 98 QuickInstall:

1. CD/DVD-ROM boot

   The Windows 98 QuickInstall ISO image can be booted on any computer that supports floppy-emulation CD-ROM boot.

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

  * **Windows 10** 21H2, Build 19044.2846
  * Windows Subsystem For Linux (Ubuntu 20.04.5 LTS)
  * Ubuntu 20.04.4 LTS (native)

- `python` (3.5 or newer)

  * On **Windows 7 and 8.1**:

    Download a python installation package from https://www.python.org/downloads/

  * On **Windows 10 and 11**:
  
    Use the Microsoft Store to download an appropriate variant.
  
  * On **Linux (Debian, Ubuntu)**:

    `sudo apt install python3.8`

- **Script requirements**

  Run the following command in the framework directory:

  `pip3 install -r requirements.txt`

- `mkisofs` from cdrtools
  - On **Windows**:
  
    Tool is bundled with the framework, no action required.

  - On **Linux (Debian, Ubuntu):**

    `sudo apt install genisofs`

- `wine` (**Linux Only**)

  `sudo apt install wine`

  *Many parts of the ISO building process are Windows specific and not able to be cleanly implemented natively on Linux, such as modifying system registry, parsing driver INF files, etc.*

- 86Box (recommended) or another virtual machine capable of installing Windows 9x

- Software to extract files from a hard disk image
  
  e.g. 7zip (on Linux: `sudo apt install p7zip-full`)

# The system preparation script (`sysprep.py`) 

  This script serves the purpose of preparing an installation for packaging into an ISO and/or USB image file.

  It takes the following parameters:

  * `--iso <ISO>`
    
    Instructs the script to create an ISO image with the given file name
  
  * `--usb <USB>`

    Instructs the script to create an USB key image with the given file name
  
  * `--osroot <OSROOT>`

    Specifies a Windows 98 / ME system root directory ("*OS Root*") to be used.

    **This can be specified multiple times, in which case the installation wizard will show a selection menu.**

  * `--extra <EXTRA>`

    Instructs the script to add the files in this directory to the final ISO/USB output.

    Default: `_EXTRA_CD_FILES_` in the framework directory

    **This can be specified multiple times, all the files in all the directories will be added in this case**

  * `--drivers <DRIVERS>`

    Instructs the script to process *slipstream* all drivers in this directory.

    *Slipstream* means that these drivers will be installed automatically when the hardware for them is detected at any point of the installation's lifetime, even when the hardware is not yet present at the time of installation.

    Default: `_DRIVER_` in the framework directory. It already contains a curated selection of drivers.

    **This parameter can only be specified once.**

  * `--extradrivers <EXTRADRIVERS>`

    Instructs the script to process all drivers in this directory and add them into the extra drivers directory.

    **These drivers are NOT slipstreamed** and thus not automatically installed. They are however made available on the resulting installation media and can be installed by pointing the Windows 98 / ME `Add New Hardware` wizard to the `DRIVER.EX` directory on the media.

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

# Preparing a Windows 98 / ME installation for packaging

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
    - IDE (0:0), ***raw image***, big enough to install the operating system.

  It is recommended that you install Windows in APM mode, because ACPI is a buggy mess (`setup /p i`).

  **WARNING:** The operating system ***must*** be installed from the HARD DISK and it must contain a folder containing the Windows 98 CAB files from the CDROM. Otherwise, drivers can not be slipstreamed.

  **NOTE:** It is recommended that NO extra drivers are installed in this VM.

- Configure the Windows 98 / ME installation as you wish. Examples:
  - Machine name
  - User name
  - System options
  - Themes
  - Patches
  - Utilities
  - Software

- Shut down the virtual machine and **DO NOT TURN IT BACK ON**

- Use 7zip or an imaging software and extract the entire root of the 
  partition you installed Windows 98 to.
  
  Extract all files into a directory. We will call this the *OS Root*. The default for this is `_OS_ROOT_` in the framework directory.

  On Windows, you can open the image file using the 7zip File Manager or the 7zip context menu. Or WinImage, et cetera.
  
  On Linux, you can do this with by using '7z' from the p7zip-full package.

  `7z x -o_OS_ROOT_/ /path/to/image/file`
  
  **WARNING:** Only *one* Windows directory is allowed, and only *one* Windows setup files CAB directory. If you install using 98Lite, make sure that the 98Lite setup directory is the sole carrier of these CABs.

  *INFO: The script detects the Windows directory by finding `WIN.COM`*
  *INFO: The CAB file directory is detected by finding `PRECOPY2.CAB`*

  **This location must be specified when running the script by using the `--osroot` parameter.**

# Preparing & Packaging

- Copy drivers that you want slipstreamed into a directory of your choice. By default this is `_DRIVER_` in the framework directory directory.

  *NOTE: `_DRIVER_` is already filled with a curated selection of drivers. Ycan remove these, if you wish.*

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

## Q: Windows 98 / ME complains about system file integrity when I create an image after a Daylight Savings Time swap-over

A: This is a weird glitch that happens on Windows hosts where files created after DST are suddenly are offset by one hour.

In the future, the sysprep script will work with Hard Disk image files, which will make this a non-issue.

**Workaround 1:** Re-extract the files from the hard disk image you used as the base to create your image

**Workaround 2:** Create images on Linux

**Workaround 3:** Wait until summer :-)

## Q: Windows 98 / ME complains about missing CAT files when installing a driver from the extra drivers

A: 98Lite deletes the catalog root directory to save installation space. Catalog files can safely be skipped, but if the error annoys you, you can unpack them from the Win98 CAB files to prevent it.

## Q: I'm getting a python error about non-zero return code in `msdos.exe` right after `Using SHELL32.xxx to reboot!`

Example:
```
subprocess.CalledProcessError: Command '['L:\\win98-installer\\__BIN__\\tools\\msdos.exe', 'L:\\win98-installer\\__BIN__\\registry\\regedit.exe', '/L:SYSTEM.DAT', '/R:USER.DAT', 'tmp.reg']' returned non-zero exit status 1.
```

A: This problem happens when running the script on Windows whilst the script directory is in a share hosted by a WSL session (Windows Subsystem for Linux). This causes some incompatibilities. Run the script from the WSL Linux shell instead.

## Q: All the operating system files are un-hidden after installation! Why?

A: You've probably used Linux to do the image creation. Linux has no concept of hidden files, therefore this file property cannot be replicated in the images.

## Q: I'm getting I/O and read errors, segmentation faults and other weird behavior when installing from CD on an Intel i430 / i440-based system with an Intel 82371SB south bridge (e.g. i440FX)

A: This problem has been verified by Deksor, Rigo and myself, and is a deeply rooted problem that has existed since at least version 2.4.xx. Operating the drives in PIO mode can help.

A BIOS update may help, the issue is currently under investigation as we found some BIOS versions where this problem does not occur.

For now, you can work around this problem by using a PCI SCSI or IDE adapter card that supports CD-ROM boot or has DOS drivers with the **DOS boot floppy option**.

## Q: I'm trying to install on a VIA MVP3-based motherboard and I'm getting a "General Protection Fault" on the first boot. (Repoted by Rigo)

A: To work around this issue, select the "slow" hardware detection variant in the installation wizard. The problem is currently under investigation.

## Q: I'm trying to install on my 486 and I'm getting Disk I/O errors!

A: Your BIOS might have an incomplete/buggy LBA implementation. Partition the drive to use a FAT32 non-LBA partition and try again.

## Q: I'm getting a `While initializing device VCACHE: Windows protection error` when running on my modern PC (Ryzen, Intel 13th gen, etc.)

Install the CREGFIX patch (the reference ISOs contain it in the extras folder):

https://github.com/mintsuki/cregfix

The patch can be automated by adding it to the `--extra` parameter of the `sysprep.py` script or dropping it into `_EXTRA_CD_FILES_` before sysprep and installation.
