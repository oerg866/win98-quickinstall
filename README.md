*****************************************************************************
                 Windows 98 QuickInstall ISO Creator Package

                       (C) 2012-2023 Eric Voirin (oerg866)
*****************************************************************************

# Disclaimer

**Windows is a trademark that belongs to Microsoft Corporation.**

This project has ***NO ENDORSEMENT FROM ANY INVOLVED PARTIES, SUCH AS MICROSOFT.***

Please don't sue. I just like old computers :(

# Windows 98 SE QuickInstall

This is a framework that is intended for creating and preparing Windows 98 installation ISO images in a way that is optimized for extremely quick installation, similar to *nLite*, but with a completely different method and context.

It takes the root file system of an already installed Windows 98 system and packages it, whilst allowing drivers and tools to be slipstreamed at will.

For the installer, it uses Linux as a base, paired with some tools to allow hard disk partitioning and formatting, as well as  a custom installer with a custom data packing method that is optimized for streaming directly from CD to the hard disk without any seeking.

Bottom line, this means that the effort for the user to build an ISO is higher than for example nLite, but the degree of customizability is also massively increased.

## How fast is it really?

On a Pentium III class machine with ATA / ATAPI Ultra DMA available for all storage devices, Windows 98 -- using an ISO built with this framework -- can be installed from CD in roughly 60-90 seconds.

# Building the Framework, Bugs, License, etc.

See [BUILDING.md](./BUILDING.md).

# System requirements to run Windows 98 QuickInstall
  - i486-class CPU, at least a 486SX (but it will be very slow)
  - 24 MiB of memory
  - An IDE / SATA / SCSI controller supported by Linux

# How to boot Windows 98 QuickInstall

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

   **The kernel on this image does not support USB or kernel logs (`dmesg`)**

1. Removable media such as USB Flash Drives

   Using SYSLINUX it is possible to boot 

# System requirements to build Windows 98 QuickInstall __ISO images__

- Reasonably modern Windows version (7 and upwards should be okay) 
  OR reasonably modern Linux variant. This was tested with Ubuntu 20.04 on WSL.

- `wine` (Linux only)

  **I know this sounds very odd, but:**
  
  Many parts of the ISO building process are Windows specific and not able to be cleanly implemented natively on Linux, such as modifying system registry, parsing driver INF files, etc.

- 86Box (recommended) or another virtual machine capable of installing Windows 9x
- Software to extract files from a hard disk image
  
  e.g. 7zip (on Linux: `sudo apt install p7zip-full`)
- `cdrtools` OR `mkisofs` OR `genisofs`
  - Windows version is bundled
  - Linux users must install it (`sudo apt install genisofs`)

# How to create an ISO file

- Install Windows 98 in a virtual machine or emulator, just as you want it.
  I recommend using 86Box using the following configuration:
  
  - Machine:
    - Machine Type: Slot 1
    - Achine: [i440BX] ABIT BF6
    - CPU type: Intel Pentium II (Deschutes)
    - Memory: 64 MB
  - Display:
    - Video: [AGP] 3dfx Voodoo3 3000 (any video card that does not have an integrated driver in Windows 98 is fine)
  - Network:
    - None, ***VERY IMPORTANT TO MAKE SURE NETWORK DRIVER SETUP STAYS INTACT!!***
  - Hard disks:
    - IDE (0:0), ***raw image***, big enough to install the operating system.

  It is recommended that you install Windows in APM mode, because ACPI is a buggy mess (`setup /p i`).

  **WARNING:** The operating system ***must*** be installed from the HARD DISK and it must contain a folder containing the Windows 98 CAB files from the CDROM. Otherwise, drivers can not be slipstreamed.

  **NOTE:** It is recommended that NO extra drivers are installed in this VM.

- Configure the Windows 98 installation as you wish. Examples:
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
  
  Extract all files into the `_OS_ROOT_` directory.

  On Windows, you can open the image file using the 7zip File Manager or the 7zip context menu. Or WinImage, et cetera.
  
  On Linux, you can do this with by using '7z' from the p7zip-full package.

  `7z x -o_OS_ROOT_/ /path/to/image/file`
  
  **WARNING:** Only *one* Windows directory is allowed, and only *one* Windows setup files CAB directory. If you install using 98Lite, make sure that the 98Lite setup directory is the sole carrier of these CABs.

  *INFO: The script detects the Windows directory by finding `WIN.COM`*
  *INFO: The CAB file directory is detected by finding `PRECOPY2.CAB`*

  You can choose a directory other than `_OS_ROOT_`, see below for details.

- Copy drivers that you want slipstreamed to the `_DRIVER_` directory.

  *NOTE: This directory is already filled with a curated selection of drivers. Ycan remove these, if you wish.*

- Copy extra drivers that will not be slipstreamed but added to the
  "_EXTRA_DRIVER_" directory.

  *NOTE: These drivers will be processed in the same way as the slipstreamed ones but will not be copied to the hard drive during installation.*

  *INFO: This folder will be named `DRIVER.EX` on the ISO. You can point the Windows 98 hardware wizard to this folder and the drivers will be found and installed correctly.*

- Add any extra files you wish added to the ISO to the `_EXTRA_CD_FILES` directory.

  This can include drivers that you do not wish to be processed with the QuickInstall tools, e.g. drivers that contain bundled software.

- Run the following command to build an ISO:

  - Windows: `sysprep.bat [Extracted OS directory] [ISO file name]`
  - Linux:   `./sysprep.sh [Extracted OS directory] [ISO file name]`

  The two parameters are optional. 
  

  **HOWEVER,** if you copied the system root to a directory other than `_OS_ROOT_`, then they must be specified.

  **NOTE:** If you wish to specify one, you *must* also specify the other.

  Unless otherwise specified, the `__ISO__` directory will contain the output ISO image.

  **WARNING: THE OS DIRECTORY WILL BE MODIFIED IN PLACE!**

- Do not hesitate to contact me on Discord if you need help:

  `oerg#0866`

- Enjoy!

# How to create a bootable USB key

- Extra requirements:
  - `mtools` (Linux only)
  - `dosfstools` (Linux only)
  - `parted` (Linux only)
  - `syslinux` (Linux only)

  `sudo apt install mtools dosfstools parted syslinux`

  The windows script uses the integrated `diskpart`, as well as the included windows ports of `syslinux` and `dd`

- Follow the steps under **How to create an ISO file**

- Run the following command:
  - Windows: `makeusb.bat <ISO File> <Output USB image file>`
  - Linux: `./makeusb.sh <ISO File> <Output USB image file>`

***NOTE***: On Windows, the file size may not exceed 2GiB due to `cmd`'s arithmetic limitations.

## How to write the bootable USB image to a USB flash drive

- On Linux, you can use `dd`
  - `dd if=<USB image file> of=/dev/sdX bs=1024k status=progress`
    
    Replace `/dev/sdX` with the USB flash drive's device path.

- On Windows, you can use the following tools:
  - `dd` For Windows: http://www.chrysocome.net/dd
  - `Win32 DiskImager`: https://sourceforge.net/projects/win32diskimager/