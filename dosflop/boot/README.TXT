
            XMGR, UIDE, UHDD/UDVD2, and RDISK -- DOS Device Drivers
          ===========================================================

1. Description
   -----------

   XMGR, UIDE, UHDD/UDVD2 and RDISK are Open Source DOS device drivers for
   a PC system with an 80386+ CPU and using FreeDOS.

   XMGR is an "XMS manager" for up to 4-GB of XMS memory, with support for
   V3.70+ UMBPCI by Uwe Sieber.   XMGR can load directly to UMBPCI "Shadow
   RAM" upper memory, and it can "boot" into regular upper memory if using
   JEMM386.   See the example FDCONFIG.SYS files in Section 5 below.

   UIDE is a disk caching driver.   It traps BIOS "Int 13h" I-O and caches
   data for up to 22 BIOS disks of any size on up to 9 "Legacy" and Native
   PCI SATA/IDE controllers.   It calls the BIOS to run A: or B: diskettes
   and caches data for other Int 13h drivers which load first.   UIDE also
   runs up to 4 CD/DVD drives, including old PIO-mode types (no UltraDMA),
   and caches their data files and directories for FAR faster speed (audio
   or trackwriting is uncached).   UIDE's cache is in XMS memory and holds
   up to 4 Gigabytes of data.   Its /B switch also sets a "basic" UltraDMA
   disk driver (no cache) which can use 128K of XMS to buffer I-O unsuited
   to UltraDMA.   The basic UIDE is for tests and other non-cached work.

   UHDD is an improved disk-only caching driver.    It works like UIDE and
   handles up to 22 BIOS disks of all sizes, including A: or B: diskettes,
   on up to 9 "Legacy" and Native-PCI SATA/IDE controllers.    When loaded
   after UHDD, the UDVD2 driver (see below) will call UHDD to cache CD/DVD
   files and directories.    With a 10-MB+ cache, UHDD uses Read-Ahead for
   its UltraDMA disks; and with a /O switch, disk UltraDMA and cache tasks
   will overlap.   Using both gives UHDD up to 20% more caching speed than
   UIDE.   UHDD's cache is in XMS memory and can hold up to 4 Gigabytes of
   data.   Its /B switch also sets a basic UltraDMA disk driver (no cache)
   requiring 128K of XMS as a buffer for I-O not suited to UltraDMA.   The
   basic UHDD is far smaller than UIDE's basic driver and can handle tests
   or other non-cached work.

   UDVD2 is a CD/DVD driver for up to 4 SATA, IDE, or old PIO-mode drives.
   It reads CD/DVD data files, plays audio CDs, and can read "raw" (track-
   writer) data.   UDVD2 can work alone; or if the caching UHDD is in use,
   UDVD2 calls it to cache CD/DVD data files and directories (audio/track-
   writing is uncached).    UDVD2 uses 128K of XMD as its buffer for input
   not suited to UltraDMA.   It "shares" UHDD's buffer, or it sets its own
   buffer if UHDD is unused.

   RDISK is a RAM-disk driver.   It creates a "fast" disk using up to 2-GB
   of XMS memory.   Files can be uploaded using FDAUTO to the RAM-disk and
   accessed at memory speeds.    RDISK is a simple RAM-disk driver with no
   resizing or other options.

   The small RDISKON.COM program will re-enable an RDISK drive if a format
   command is issued to it by mistake!   Entering  RDISKON L  at a command
   prompt, where L is the drive letter (A to Z), re-enables the drive.

   The small CC.COM "Clear Cache" program can help verify files written by
   UIDE/UHDD.   Entering  CC  at a command prompt makes the caching driver
   "flush" its cache.     Disk data (NOT data still in cache!) can then be
   compared v.s. the original output.    Note that some CompactFlash cards
   cause CC to execute very slowly.


2. NO Warranties
   -------------

   XMGR, UIDE, UHDD/UDVD2 and RDISK are offered at no cost, as-is, "Use at
   your own risk", and with NO warranties, not even an implied warranty of
   FITNESS for any purposes, nor an implied warranty of MERCHANTABILITY!


3. Revision Summary
   ----------------

   30-Oct-21   UHDD uses mostly caching logic  even for noncached work,
                 saving 650+ bytes on disk! UHDD.SYS is now less than 5K,
                 suitable even for crowded "boot diskettes" etc. All UHDD
                 features, switches and cache speeds do remain UNCHANGED!
                 [The /B noncached UHDD,  not often used,  now uses ~ 300
                 bytes of additional RAM compared to the previous version].

   14-Jul-20   UHDD's /O switch now enables DMA/Caching Overlap!    It and
                 Read-Ahead allow UHDD up to 20% faster caching speed than
                 UIDE on V6.22 MS-DOS and similar speed gains for FreeDOS.
                 See the comments about UHDD /O in Section 5, below.   Due
                 to Overlap, UHDD now runs up to 20 BIOS disks (was 22).

    7-Jul-20   UIDE now updated with all UHDD updates and "bug fixes" from
                 15-Feb-2019 onward.    It now offers 10-MB and all 20-MB+
                 cache sizes for old small-memory systems.   UHDD fixed to
                 run 9 controllers and "LBA48" disks (> 128-GB) correctly.
                 It now does Read-Ahead with all 10-MB+ caches.    See the
                 driver .ASM source files for full notes on these changes.

   15-Apr-20   XMGR updated from the 5-Mar-2015 version as follows:

        The "I-O Catcher" now handles a maximum of 16 BIOS hard disks.

        Real-mode and /Z protected-mode XMS move sections are 4K bytes
          for faster speed (was 2K).

        The /B "boot" option now permits /N32 to set 32 XMS "Handles",
          saving 160 low-memory bytes versus the 48 "Handles" default.

        The 80386 "errata" commands recommended by Ninho are included.

        The HIMEM de-facto standard driver name of "XMSXXXX0" is used.

   31-Mar-19   UHDD now uses Read-Ahead for its UltraDMA disks with 20-MB+
                 caches (10% more speed than UIDE).

   16-Mar-19   README file updated.    UHDD.ASM changed to note UDVD2 must
                  load AFTER the UHDD driver (not before!) to cache CD/DVD
                  data.   No code changes, UHDD still dated 15-Feb-2019.

   15-Feb-19   UHDD updated from the 5-Mar-2015 version as follows:

        10-MB and all 20-MB+ cache sizes added, for old systems with small
        memory.   Default is still 80-MB if UHDD's /S switch is omitted or
        invalid.

        The 5-MB cache has 16K cache blocks, the 10- and 15-MB caches have
        32K blocks, and the 20-MB+ caches use 64K blocks for faster speed.

        Maximum cache size is still 4 Gigabytes (really 4093-MB; 3-MB held
        for UHDD's cache tables [max. 1-MB] and mainboard control regs.).

        The 5-Mar-2015 UDVD2 can still call this UHDD to cache CD/DVD data
        files; no UDVD2 changes needed.

        If LBACache is wanted, the non-cached UHDD /B can still load first
        to provide disk UltraDMA.

        The "80386 errata" commands used by Ninho on his 386/SX are added.

        SSDs and CompactFlash cards with an ATAPI flag are run as UltraDMA
        disks, not as slower "Call the BIOS" drives like in UIDE.

        Disks over 128-GB now use only LBA48 commands, to stop disk ERRORS
        at exactly 128-GB when an LBA28 command "crosses OVER" that limit.
        All disks less than 128-GB still use LBA28 commands.

   For earlier revision notes, see the 5-Mar-2015 README file.


4. Switch Options
   --------------

   XMGR usually needs only its /B switch, if "booting" with an EMM driver.
   All XMGR switch options are as follows:

      /B     Specifies "boot" mode.   XMGR loads in temporary memory until
                JEMM386 enables upper memory.    Without /B, XMGR loads in
                UMBPCI "Shadow RAM" or in low memory if UMBPCI is unused.

      /Mn    Specifies a temporary area for loading XMGR in "boot" mode or
                for UMBPCI "Shadow RAM" DMA before a workspace buffer gets
                posted.   Values for /M are:

                    /M1 = 64K.    /M3 = 192K.   /M5 = 320K.   /M7 = 448K.
                    /M2 = 128K.   /M4 = 256K.   /M6 = 384K.   /M8 = 512K.

                Without /M, /M5 is assumed and the 320K area will be used.
                Temporary system data may be put anywhere in memory!   /Mn
                helps to find a safe area for XMGR to use.

      /Nnn   Sets how many XMS "Handles" are available.   The value nn may
                be 48, 80, or 128.   The /B "boot" option now accepts /N32
                to save low memory, but 32 "Handles" may be not-enough and
                should be tested.   Without /N, 48 "Handles" are set.

      /PA    Specifies use or non-use of PS/2 Port 92h logic to handle the
      /PN       system's "A20" line.   /PA indicates "Always" use Port 92h
                logic.   /PN indicates "Never" use it and handle "A20" via
                standard keyboard-port logic.   Without /P, XMGR "asks the
                BIOS" if the system supports Port 92h.   If not, XMGR uses
                keyboard-port logic.    If "A20" is found to be on as XMGR
                loads, XMGR does not handle it at all.

      /Tn    Specifies the BIOS calls to use in getting extended memory:

                   /T0   No "E820h" or "E801h" requests.
                   /T1   Memory-list requests only (Int 15h, AX=E820h).
                   /T2   A dual-area request only  (Int 15h, AX=E801h).
                   /T3   "E820h" requests first, then an "E801h" request.

                /T can usually be omitted, causing a /T3 default.   An old
                64-MB request is also used to get /T0 memory or if the /T1
                to /T3 requests are unsuccessful.   Users may need to test
                /T1 and /T2, as a pre-1996 BIOS might not handle them O.K.
                If so, /T0 will be required.

      /W     Requests using the kernel workspace buffer for UMBPCI "Shadow
                RAM" DMA.   If /W is omitted, XMGR sets its own low memory
                buffer.   /W is ignored if UMBPCI is not used.

      /Z     Moves protected-mode XMS data in 4K blocks (not 64K).    With
                JEMM386, /Z is unneeded.    If other EMM/VCPI/DPMI drivers
                are used, PCs must be tested to see if /Z is needed.   BAD
                schemes, that allow too few interrupts during an XMS move,
                may still be in use!

             --------------------

   UIDE usually needs only a /H switch to use HMA space and a /S switch to
   specify its cache size.   All UIDE switches are as follows:

      /A     Requests "alternate" addressing for "legacy" IDE controllers,
                01E8h/0168h for the first one, 01F0h/0170h for the second.
                /A is rarely needed.   Without /A, the first "legacy" con-
                troller will use 01F0h/0170h and a second uses 01E8h/0168h
                as is usual for PC mainboards.

      /B     Requests a "basic" UltraDMA driver for disks and CDs/DVDs, no
                caching nor diskette handling.   The /B driver can request
                128K of XMS for an UltraDMA I-O buffer, and it can load in
                the HMA.

      /D:    Specifies the "device name" used by the CD/DVD Redirector to
                access CD/DVD drives, e.g.   /D:CDROM1   /D:SANYO1   etc.
                If /D: is omitted, or the name following a /D: is missing
                or invalid,  UDVD1  is set by default.

      /E     Makes the driver call the BIOS for any hard disk I-O request.
                /E avoids setup trouble on some DOS emulators (VirtualBox,
                etc.) that do not emulate all PC hardware logic!   /E also
                allows using hard disks on 1994 or older PCs which have no
                UltraDMA or no PCI/EDD BIOS.    /E still caches disk data,
                unlike /N1 that removes ALL disk logic!    If /B is given,
                /E is ignored.

                ***** NOTE *****

                Using /E on protected-mode systems (JEMM386 etc.) may LOSE
                much speed, as a newer BIOS may omit "Virtual DMA" support
                and force disks to run in PIO mode!   If /E is required, a
                DOS system should run in real-mode (UMBPCI, etc.) whenever
                possible.

      /H     Puts most of the driver in "free HMA" space.   To use /H, the
                driver must load from FDAUTO.BAT (not FDCONFIG.SYS), since
                FreeDOS provides no "free HMA" until FDAUTO is run.

      /N1    Requests NO hard-disk handling by the driver.

      /N2    Requests NO CD/DVD handling by the driver.

      /N3    Requests no XMS memory.    /N3 sets UIDE's /B "basic" driver.
                /N3 requires loading in LOW memory, or UIDE aborts!    /N3
                can LOSE much speed, as misaligned or other I-O unsuitable
                for UltraDMA requires calling the BIOS for a disk or using
                PIO-mode for CD/DVD drives!

      /N4    See /Z below.

      /Q     Awaits "data request" before starting UltraDMA I-O transfers.
                /Q is rarely needed, only for old systems where the driver
                loads O.K. but seems unable to transfer data.    /Q is NOT
                for use with Sabrent or other SATA-to-IDE adapters that do
                not emulate "data request"!

      /R15   "Reserves" 15-MB or 63-MB of XMS, for old DOS "game" programs
      /R63      which require XMS memory below 16- or 64-MB.   The drivers
                must be able to reserve this memory, reserve their own XMS
                memory beyond that, and then "free" the 15/63-MB XMS.   If
                not, the drivers display "XMS init error" and abort!    /R
                for UDVD2 is ignored if it loads after UHDD, as UDVD2 will
                then "share" UHDD's XMS memory.

      /Snn   Specifies the desired cache size, in megabytes of XMS memory.
                Values for /S can be 5, 10, 15  or any number from 20 thru
                4093 (4 Gigabytes).    When /S is omitted/invalid, a 20-MB
                cache is set.   UIDE displays "XMS init error" and aborts,
                if not-enough XMS memory is free!   If so, a smaller cache
                must be requested.

      /UX    Disables all CD/DVD UltraDMA, even for units that can do it.
                All CD/DVD data input then uses PIO-mode.   /UX is rarely
                needed, only for odd drives that do not obey ATAPI rules.
                /UX does not affect hard disks.

      /Z     Moves protected-mode XMS data in 8K blocks (not 64K) for 486+
                CPUs and 4K blocks for slower 386 CPUs.   With JEMM386, /Z
                is unneeded.    When other EMM/VCPI/DPMI drivers are used,
                PCs must be tested to find if /Z is needed.   BAD schemes,
                that allow not-enough interrupts during an XMS move, could
                still be in use!

             --------------------

   UHDD usually needs only a /H switch to load in the HMA, a /S switch to
   set a cache size, and a /O switch to enable DMA/Caching Overlap (best,
   for most systems!).   A summary of all UHDD switches is as follows:

      /A     Sets ALTERNATE addressing for "Legacy" IDE controllers, same
                as UIDE /A above.   Rarely necessary.

      /B     Requests a "basic" UltraDMA disk driver (no cache) requiring
                128K of XMS as a buffer for I-O unsuited to UltraDMA.

      /E     Makes the driver "call the BIOS" for hard disk I-O requests,
                same as UIDE /E above.

      /H     Puts most of the driver in "free HMA" space.   See the notes
                for UIDE /H above.

      /O     Overlaps disk UltraDMA with caching tasks, for faster speed.
                Although preferred, /O may NOT work on old/odd/"cheap" PC
                mainboards unable to do UltraDMA and access XMS memory at
                the same time!   Systems should be tested, using /O.   If
                disk errors occur, but CD/DVD input via UDVD2 works fine,
                /O must NOT be used!

      /Q     Awaits "data request" before beginning UltraDMA I-O with old
                controllers, same as UIDE /Q above.   Rarely necessary.

      /R15   Reserves 15-MB or 63-MB of XMS, for old DOS "game" programs,
      /R63      same as UIDE /R above.

      /Snn   Specifies the desired cache size in megabytes of XMS memory.
                Values for /S may be 5 or any number from 10 thru 4093 (4
                Gigabytes).    If /S is omitted or invalid, a 20-MB cache
                is set.   UHDD displays "XMS init error" and aborts, when
                not-enough XMS is free!   If so, request a smaller cache.

      /Z     See /Z for UIDE, above.

             --------------------

   UDVD2 normally needs only a /H switch to use HMA space and a /D: switch
   to specify a driver "device name".   A summary of all UDVD2 switches is
   as follows:

      /A     Sets ALTERNATE addressing for "Legacy" IDE controllers, same
                as UIDE /A above.   Rarely necessary.

      /D:    Sets a "device name" used by the CD/DVD Redirector to access
                CD/DVD drives, same as UIDE /D: above.

      /H     Puts most of the driver in "free HMA" space.   See the notes
                for UIDE /H above.

      /Rnn   Reserves 15-MB or 63-MB of XMS, for old DOS "game" programs,
                same as UIDE /R above.   Rarely necessary.

      /UX    Disables CD/DVD UltraDMA, same as UIDE /UX above.     Rarely
                necessary.

             --------------------

   RDISK uses only /S size and /: drive-letter switches:

      /Sn    Specifies a desired RAM-disk size in megabytes of XMS memory.
                Values are any number from 2 to 2047.   When /S is omitted
                or invalid, a 25-MB RAM-disk is set by default.

      /:L    Specifies the DOS drive letter desired to access RDISK files.
                L may be any available drive letter from A to Z, e.g.  /:N
                assigns drive N: to all RDISK files.   If the drive letter
                is too high or already in use, RDISK will abort, and users
                will need "LASTDRIVE=" in FDCONFIG.SYS to set more drives.
                If RDISK loads from FDCONFIG.SYS, or if /: is omitted, the
                next free drive letter will be used.

             --------------------

   For all switches in each driver, a dash may replace the slash and lower
   case letters may be used if desired.


5. Example Configuration Files
   ---------------------------

   A) Small real-mode systems that need only XMS may use this FDCONFIG.SYS
      example file:

          ..
          ..
      DOS=HIGH
      DEVICE=C:\BIN\XMGR.SYS
          ..
          ..  Etc.
          ..

   B) Real-mode systems with V3.70+ UMBPCI and XMGR do not need the LOWDMA
      driver, as XMGR sets an "I-O Catcher" for UMBPCI.   This scheme uses
      NO low memory, if /W is given; XMGR and other drivers go directly to
      UMBPCI "Shadow RAM" upper memory!   An example FDCONFIG.SYS file is:

          ..
          ..
      DOS=HIGH,UMB
      DOSDATA=UMB
      DEVICE=C:\BIN\UMBPCI.SYS
      DEVICE=C:\BIN\XMGR.SYS /W
          ..
          ..  Etc.
          ..

   C) A protected-mode system with XMGR and JEMM386 can use XMGR's "boot",
      taking only 384 low memory bytes as its 32-entry "XMS Handles" table
      (used early by JEMM386).   See Section 6 below for other notes about
      protected-mode!   An example FDCONFIG.SYS file is:

          ..
          ..
      DOS=HIGH,UMB
      DOSDATA-UMB
      DEVICE=C:\BIN\XMGR.SYS /B /N32             ;32 Handle XMGR "boot"
      DEVICE=C:\BIN\JEMM386.EXE I=B000-B7FF ...
      DEVICEHIGH=C:\BIN\XMGR.SYS                 ;Loads the runtime XMGR
          ..
          .. Etc.
          ..

   When all above drivers are loaded, other needed FDCONFIG.SYS files such
   as SETVER, ANSI.SYS, etc. can then load in any desired order.

   For each of the above examples, FDAUTO.BAT can then load UHDD and UDVD2
   (or just UIDE) using the DEVLOAD program.   This permits the drivers to
   use "free HMA" and save low memory.   UDVD2 must load after UHDD, so it
   will find UHDD in memory and "link" to it for CD/DVD data file caching.
   Example FDAUTO command lines are:

          ..
          ..  Int 13h drivers cached thru UHDD/UIDE
          ..  load now and can load in upper memory.
          ..
      DEVlOAD /H C:\BIN\UHDD.SYS /S500 /H /O     ;Or UIDE alone
      DEVLOAD /H C:\BIN\UDVD2.SYS /D:BLURAY1 /H
      LH C:\BIN\RDISK.COM /S250                  ;Optional
          ..
          ..  Etc.
          ..


6. Technical Notes
   ---------------

   Protected-mode users must pretest "I=nnnn-nnnn" or "X=nnnn-nnnn" values
   for JEMM386, as its "I=TEST" or "X=TEST" options may fail on newer PCs.
   Protected-mode is unrecommended with old DOS games needing reserved XMS
   as JEMM386 takes some XMS memory first.   Newer "cheap BIOS" mainboards
   now OMIT logic used only by DOS and need an UltraDMA disk driver loaded
   before JEMM386 sets V86 protected-mode!   For such boards, FDCONFIG.SYS
   example C above requires drivers cached thru UHDD, then UHDD itself, to
   load only in LOW memory directly after XMGR sets up XMS for the system.
   UIDE is unrecommended in this case as it will take FAR more low memory!
   JEMM386 can then load and set V86 protected-mode, and UDVD2 (if wanted)
   can be loaded later from FDAUTO.   2015 or older PCs with a normal BIOS
   usually do not need this and can use example C as shown above.   Use of
   JEMMEX or other "EMM" drivers but JEMM386 must now be avoided.    Other
   "EMM" drivers got dropped long ago, some with leftover ERRORS!   JEMMEX
   has caused crashes on newer PCs due to OBSOLETE XMS detection code, and
   it cannot work with UIDE/UHDD on a "cheap BIOS" board as neither driver
   can load O.K. if the other is absent!   To run protected-mode on "cheap
   BIOS" boards, use FDCONFIG.SYS example C with UHDD/UDVD2 (not UIDE) and
   the changes noted herein, and be aware that JEMM386 will be REQUIRED!

   XMGR and UIDE/UHDD/UDVD2 will return normal XMS and CD/DVD error codes,
   as needed.   They are listed in the "V3.0 XMS Specification" and in the
   Microsoft "MS-DOS CD-ROM Extensions 2.1" document.   Both are available
   from Microsoft or from other Internet sources.

   UIDE and UHDD work as "BIOS drivers" and return whichever codes are set
   for diskettes and hard-disks handled by the BIOS.    For their SATA and
   IDE hard-disks, UIDE/UHDD can post the following error codes:

       Code 0Fh - DMA error.           CCh - Disk is FAULTED.
            20h - Controller busy.     E0h - Hard I-O error.
            AAh - Disk not ready.      FFh - XMS memory error.

   Many DOS programs display only "Disk Error" messages with NO code, thus
   disk errors may require running a diagnostic to get better information!

   UIDE/UHDD handle only "Legacy" or "Native PCI" IDE controllers.   RAID-
   only chipsets, port multiplier chips, or ADMA chipsets are unsupported.
   UIDE/UHDD must use "Legacy", "Compatibility", or "Native IDE" mode with
   AHCI controllers.    For mainboards with no such controller settings, a
   Sabrent or similar SATA-to-IDE card can let UIDE/UHDD/UDVD2 handle SATA
   disks/CDs/DVDs from parallel-port IDE controllers (80-pin cable) at DMA
   speeds.   "Add on" PCI-bus adapter cards that can be detected by normal
   PCI-bus routines may also be used for disks/CDs/DVDs.

   XMGR loads in UMBPCI upper memory that is not "provided" to the system.
   With UMBPCI, a "MEM" list may begin with a block having a 00A5h offset,
   or greater with 80 or 128 XMS "Handles".    The upper memory skipped by
   this offset contains XMGR.

   The UMBPCI upper memory manager uses system "Shadow RAM" that cannot do
   DMA.    A newer BIOS may use UltraDMA to load programs in upper memory.
   If this is "Shadow RAM", a HANG can occur!    These two rules apply, if
   using UMBPCI with XMGR and UIDE/UHDD:

     A) V3.70+ UMBPCI must load before XMGR, so XMGR finds UMBPCI and sets
        its "I-O Catcher", to handle diskette "Shadow RAM" DMA forever and
        to do disk "Shadow RAM" UltraDMA until UIDE/UHDD can load.   Older
        UMBPCI drivers, or other UMBPCI load schemes, are not recommended!

     B) When CHS I-O is done, each disk must have valid CHS parameters set
        in the BIOS.   If not, the "I-O Catcher" or UIDE/UHDD let the BIOS
        handle CHS I-O.   If BIOS UltraDMA is left enabled, a "Shadow RAM"
        HANG can occur!

   Old BIOS programs may not configure a mainboard controller when no user
   drives are on it!   The drivers then display "BAD controller" and go on
   looking for others to use.   If this message is given, users must check
   that all SATA/IDE drives are set "active" with the BIOS setup routines.
   If so, "BAD controller" means a chip is not using "Bus Master" and "I-O
   Space" modes, and a BIOS update is needed!

