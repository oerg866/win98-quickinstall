# Windows 9x QuickInstall Reference Images

The QuickInstall reference images have a patch selection curated by the author (oerg866) and SweetLow to give them the best possible chance of working well on both old and new systems.

They are available in three flavors:

- Windows 98 SE: **Stock installation with DirectX 6.1**
- Windows 98 SE: **98Lite De-Bloated installation with DirectX 8.1**
- Windows ME: **98Lite De-Bloated installation with DirectX 8.1**

The 98Lite-based images come without most extra Windows features and a minimal Internet-Explorer-free user interface to provide the fastest, slickest and bloat-free Windows 9x experience *ever*.

The *Stock Installation* image is provided for non-risk-takers :)

Check the **Releases** tab for download options.

### Reference Image Feature and Patch Level (Windows 98 SE)
* Microsoft Patches (QFEs), Windows 98 SE:
  - Q239696
  - Q239887
  - Q240075
  - Q245682
  - Q249824
  - Q253697
  - Q253711
  - Q254660
  - Q262232
  - Q269601
  - Q270063
  - Q273017
  - Q273727
  - Q276602
  - Q280448
  - Q281533
  - Q288430
  - Q291362
  - Q293793
  - Q293197
  - Q306453
  - Q381307
  
* Third party patches:
  * RAM Patch (by R. Loew)
  * VMM >4GB Resource patch (by SweetLow)
  * CDFS 2GB disk size patch (by R.Loew)
  * DOS Mouse Acceleration patch (oerg866)
  * PS/2 200Hz Report Rate patch (by SweetLow)
  * SYSDM.CPL Hardware Detection Delay patch (by oerg866)
  * Terabyte Plus Package 3.0 (by R. Loew)
  * AHCI & SSD TRIM tools (by R. Loew)
  * LBA48-patched Windows tools (scandisk, defrag, fdisk, etc.) (by MDGx)
  * VIA PCI patch (by R. Loew)
  * IO.SYS Non-DOS Partition Corruption Patch (by R. Loew)
  * IO.SYS Drive Enumeration Order Patch (by maxud)
  * Modern CPU TLB Invalidation Bug Patch (by JHRobotics)
  * SMART access driver (SMARTVSD) directory fix
  * SweetLow patch set for existing patches:
    * R.Loew AHCI.PDR - disabled nonfunctional handler of IDE_PASS_THROUGH to prevent BSOD on SMART access
    * APIX.VXD - disabled special handling for devices on Port Driver ESDI_506.PDR
    * SMARTVSD.VXD - works for any combination of ATA channels and devices on these channels and more
    * R. Loew SATA Patch - Corrected .INF for PCI Bus Master ATA Controllers including those in PCI Native Mode

### Reference Image Feature and Patch Level (Windows ME)
* Microsoft Patches (QFEs), Windows ME:
  - Q268452
  - Q274175
  - Q276602
  - Q277784
  - Q278289
  - Q280127
  - Q280800
  - Q289635
  - Q290831
  - Q296773
  - Q301453
  - Q301540
  - Q304708
  - Q305826
  - Q308236
  - Q311561
  - Q314417
  - Q316795
  - Q381307

* Third-party patches:
  * RAM Patch (by R. Loew)
  * VMM >4GB Resource patch (by SweetLow)
  * CDFS 2GB disk size patch (by R.Loew)
  * DOS Mouse Acceleration patch (oerg866)
  * PS/2 200Hz Report Rate patch (by SweetLow)
  * SYSDM.CPL Hardware Detection Delay patch (by oerg866)
  * AHCI & SSD TRIM tools (by R. Loew)
  * LBA48 Drive Size patch (by LLXX)
  * 1TB+ Disk Size Patch (by R. Loew)
  * SATA Patch (by R. Loew)
  * VIA PCI patch (by R. Loew)
  * IO.SYS Non-DOS Partition Corruption Patch (by R. Loew)
  * Modern CPU TLB Invalidation Bug Patch (by JHRobotics)
  * SMART access driver (SMARTVSD) directory fix
  * SweetLow patch set for existing patches:
    * R.Loew AHCI.PDR - disabled nonfunctional handler of IDE_PASS_THROUGH to prevent BSOD on SMART access
    * APIX.VXD - disabled special handling for devices on Port Driver ESDI_506.PDR
    * SMARTVSD.VXD - works for any combination of ATA channels and devices on these channels and more
    * R. Loew SATA Patch - Corrected .INF for PCI Bus Master ATA Controllers including those in PCI Native Mode
    * R. Loew SATA Patch - Fixed extraction of default parameters of drive from IDENTIFY DEVICE data

### Common Updates and Software
- Microsoft Installer 2.0
- Microsoft Layer for Unicode
- Microsoft DirectX 6.1a (stock images) or 8.1b (Lite images)
- 7-Zip 9.20
- Unofficial 2020 Timezone Update (by PROBLEMCHYLD)
  
### Extras inside `extras` folder:
  - **Microsoft Updates**: DirectX 9.0C (Aug. 2007), .NET 2.0, IE6 SP1, VB6 / VC6 / VC2005 Runtime, Directory Services Client
  - **KernelEx**:  v4.5.2, v4.5.2016.18 Update, 4.22.26pre2 Update, 4.22.25.2-TMT Cumulative Installer

    **NOTE: This can brick your system, especially on the 98Lite versions this should not be used**
  - Revolutions Pack 9.7 + Updates + Extra Fonts / Themes
  - **Benchmarks**: 3DMark 99 + 2000 + 2001 SE, Super Pi Mod, Roadkill Disk Speed, Atto Disk Benchmark
  - **CPU/Hardware Tools**: CPU-Z Vintage Edition, CPUFSB, HWiNFO32, HDAT2, K6INIT, WPCREDIT, PCIEDIT, Video Memory Tester (VMT / VMTCE)
  - **Utilities**: Total Commander, Paragon NTFS, IrfanView, TCP Optimizer
  - **Drivers**:
    - ALi AGP (1.90, 1.82) + Utility (1.40), ALi Integrated (2.092)
    - AMD: AMD75x/76x Driver Packs (1.30)
    - VIA 4in1 (4.35, 4.43), VIA IDE Driver (3.20B with RLOEW fix), VIA Latency Patch
    - SiS: 5600/600 AGP, SiS 961/964 IDE, SiS 964 RAID, SiS IDE 2.13, UIDE1.02, AGP/USB/ATA133 1.21
    - Intel: INF Installer (6.3.0.1007)
    - nVidia: GeForce (45.23, 81.98), nForce (4.20)
