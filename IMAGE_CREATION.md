# Creating your own Windows 9x QuickInstall images

So you want to create your own QuickInstall-based installation images? Keep reading!

## System Requirements to **create** QuickInstall Images from Scratch

- A Hard Disk image (IMG/RAW, VHD, VMDK, VDI) containing a fully set up Windows 9x installation according to the guidelines described in the chapter ***Preparing a Windows 98 / ME image for use with `sysprep.py`***

- **Windows 10 or 11** (7 / 8.x may also be supported, but are untested)
  or **Linux**  (Tested with Ubuntu 22.04)
  or **WSL**  (Tested with Ubuntu 22.04)

- `python` (3.8 or newer, 3.11 recommended)

  * **Windows 7 and 8.1**: Download a python installation package from https://www.python.org/downloads/
  * **Windows 10 and 11**: Use the Microsoft Store to download an appropriate version
  * On **Linux (Debian, Ubuntu)**: `sudo apt install python3`

- **Python Requirements**

  Run the following command in the framework directory:

  `pip3 install -r requirements.txt`

  ***WARNING:*** *There are no third-party tools to modify Windows 9x-format registry DAT files, but doing so is an essential part of the image building process. This uses an MS-DOS emulation layer and the 16-Bit part of the Windows 9x registry editor (`regedit.exe`). This means that currently, on platforms other than Windows, `wine` is needed to be installed:*

  - `sudo apt install wine` (Debian, Ubuntu, ...)
  - `sudo pacman -S wine` (Arch, ...)

  To enable 32-bit Wine to run correctly, multiarch needs to be enabled first:
  - `sudo dpkg --add-architecture i386 && apt-get update`

## Preparing a Windows 9x Image to create a QuickInstall Image from Scratch

- Install Windows 98 / ME in a virtual machine or emulator, just as you want it.
  I recommend using *86Box* or *PCBox* using the following configuration:

  - **Machine:** Slot 1, [i440BX] Abit BF6, Intel Pentium II, 64MB RAM
  - **Display:** [ISA] VGA (or any video card that does not have an integrated driver in Windows 98 / ME)
  - **Network:** NONE - ***VERY IMPORTANT TO MAKE SURE NETWORK DRIVER SETUP STAYS INTACT!!***
  - **Hard disks:** IDE (0:0), big enough to install the operating system.

  It is recommended that you install Windows in APM mode, because ACPI is a buggy mess (`setup /p i`).

  **NOTE:** The operating system ***must*** be installed from the HARD DISK and it must contain ***exactly one*** folder containing the Windows 98 CAB files from the CDROM. Only *one* Windows directory is allowed, and only *one* Windows setup files CAB directory. If you install using 98Lite, make sure that the 98Lite setup directory is the sole carrier of these CABs.

  **NOTE:** It is recommended that NO extra drivers are *installed* in this VM. They can be added using the `--driver` parameter

  **Note:** It is not recommended to use a modern hypervisor such as Hyper-V, VirtualBox or VMWare to perform these steps, as an unpatched Windows 98 installation is highly unstable on modern CPUs (even under virtualization), making the installation steps before these bugs are patched very hard to execute.

- Configure the Windows 98 / ME installation as you wish:

  - System parameters (Computer Name, User Name, System / Explorer Options, Theme, etc.)
  - System patches (Official updates, third-party patches, etc.)
  - Utilities (Software, Benchmarks, Utilities, etc.)

- Shut down the virtual machine and take note of the hard disk image file name.

## The `sysprep.py` Script to create QuickInstall Images

  This script serves the purpose of preparing an installation for packaging into an ISO and/or USB image file.

  It takes the following parameters:

  * `--iso <ISO>`  
    Instructs the script to create an ISO image with the given file name
  
  * `--usb <USB>`  
    Instructs the script to create an USB key image with the given file name
  
  * `--osroot <Image File> "Display Name"`  
    Specifies a Windows 98 / ME hard disk image as the source.
    
    `Image File` can be any of the following file formats: IMG/RAW, VHD, VMDK, VDI. It can be dynamically sized or a differential image (e.g. from a hypervisor snapshot)

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
  This parameter controls console output verbosity of the script. `VERBOSE` is either `True` or `False` (default).  
  **This parameter is currently broken, sorry. It's always quiet.**

## Steps for preparing & packaging a QuickInstall Image after VM Creation

- Copy drivers that you want slipstreamed into a directory of your choice. By default this is `_DRIVER_` in the framework directory directory.

  *NOTE: `_DRIVER_` is already filled with a curated selection of drivers. You can remove these, if you wish.*

  **If you choose a non-default directory for this, you must specify it using the `--drivers` parameter.**

- Copy extra drivers that will not be slipstreamed but added to a directory of your choice. By default this is `_EXTRA_DRIVER_` in the framework directory.

  *NOTE: These drivers will be processed in the same way as the slipstreamed ones but will not be copied to the hard drive during installation.*

  *INFO: This folder will be named `DRIVER.EX` on the resulting image.  
  You can point the Windows 98 hardware wizard to this folder and the drivers will be found and installed correctly.*

  *NOTE: `_EXTRA_DRIVER_` is already filled with a curated selection of drivers. Ycan remove these, if you wish.*

  **If you choose a non-default directory for this, you must specify it using the `--extradrivers` parameter.**

- Add any extra files you wish added to the ISO to a directory of your choice. By default this is `_EXTRA_CD_FILES` in the framework directory.

  This can include drivers that you do not wish to be processed with the QuickInstall tools, e.g. drivers that contain bundled software.

  **If you choose a non-default directory for this, you must specify it using the `--extra` parameter.**

- Run the following command to build the package:

  `sysprep.py --osroot <Windows 98 Image File> "Human-Readable Name For The Image" --iso <Output ISO File Name>`  
  Or if you prefer to create a USB key image instead:  
  `sysprep.py --osroot <Windows 98 Image File> "Human-Readable Name For The Image" --usb <Output USB File Name>`
  
  You can absolutely create both a ISO and USB image at the same time. Here is an example:

  `sysprep.py --osroot myImage.vhd "Windows 98 SE" --iso win98qi.iso --usb win98qi.img`

  NOTE: *You must add the other parameters if you deviate from the defaults.*

## Packaging multiple Windows 9x Versions in one QuickInstall Image

This is an advanced feature.

By specifying the `--osroot` parameter multiple times, you can create a multi-variant installation image. In this case a selection menu will appear during installation prompting the user which variant should be installed.

Example:

`python3 sysprep.py --osroot D:\QI\Stock.VHD "98SE Stock" --osroot D:\QI\Micro.VHD "98SE Micro" --iso multi.iso`
