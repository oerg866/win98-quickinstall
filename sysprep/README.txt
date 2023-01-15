*****************************************************************************
                 Windows 98 QuickInstall ISO Creator Package

                       (C) 2012-2023 Eric Voirin (oerg866)
*****************************************************************************

Here are the steps to create an ISO:

- If you are on linux, you must install the following prerequisites:

- Install Windows 98 in a virtual machine or emulator, just as you want it.
  I recommend using 86Box using the following configuration:
  
  * Machine
    * Machine Type: Slot 1
    * Achine: [i440BX] ABIT BF6
    * CPU type: Intel Pentium II (Deschutes)
    * Memory: 64 MB
  * Display:
    * Video: [AGP] 3dfx Voodoo3 3000 (so that there is no driver contention)
  * Network:
    * NONE, VERY IMPORTANT TO MAKE SURE NETWORK DRIVER SETUP STAYS INTACT
  * Hard disks:
    * IDE (0:0), raw image, big enough to install the operating system

  *** NOTE: The operating system must be installed from the HARD DISK
  ***       and it must contain a folder containing the Windows 98
  ***       CAB files from the CDROM. Otherwise, drivers can not be
  ***       slipstreamed (yet).

  *** NOTE: It is recommended that NO extra drivers are installed in this
  ***       VM.

- Run the Pre-Setup registry file.

  This is found in the "_RUN_BEFORE_SYSPREP_" folder.

  Use "presetup.reg" if you wish to have the full Non-PNP device detection
  phase during installation. (Recommended if you intend to install on 486
  and early Pentium systems)

  Use "presetup_skip_pnp_init.reg" if you wish to skip the non-PNP device
  detection phase. This saves a lot of time during installation.
  Recommended on Pentium and above.

- Shut down the virtual machine and *DO NOT TURN IT BACK ON*

- Use 7zip or an imaging software and extract the entire root of the 
  partition you installed Windows 98 to.
  
  Extract all files into the "_OS_ROOT_" directory.

  On Windows, you can open the image file using the 7zip File Manager.
  On Linux, you can do this with by using '7z' from the p7zip-full package.

  '7z x -o_OS_ROOT_/ /path/to/image/file'
  
  *** NOTE: Only *one* Windows directory is allowed, and only *one*
  *** CAB directory.
  *** Windows directory is detected by finding "WIN.COM"
  *** CAB directory is detected by finding "PRECOPY2.CAB"

- Copy drivers that you want slipstreamed to the "_DRIVER_" directory.

  *** NOTE: This directory is already filled with a curated selection 
  *** of drivers.
  *** You can remove these, if you wish.

- Copy extra drivers that will not be slipstreamed but added to the
  "_EXTRA_DRIVER_" directory.

  *** NOTE: These drivers will be processed in the same way as the
  *** slipstreamed ones but will not be copied to the hard drive
  *** during installation.
  *** This folder will be named "DRIVER.EX" on the ISO. You can
  *** point the Windows 98 hardware wizard to this folder and
  *** the drivers will be found and installed correctly.

- Add any extra files you wish added to the ISO to the "_EXTRA_CD_FILES"
  directory.

  This can include drivers that you do not wish to be processed with
  the QuickInstall tools, e.g. drivers that contain extra software.

- Run the following command to build an ISO:

  Windows: sysprep.bat
  Linux:   ./sysprep.sh

- The "__ISO__" directory will contain the output iso.

- Do not hesitate to contact me on Discord if you need help:
  oerg#0866

- Enjoy!