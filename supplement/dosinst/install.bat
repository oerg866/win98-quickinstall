@ECHO OFF
REM This file is on the root of the CD image and can be called from DOS.

ECHO.
ECHO Welcome to Win9x QuickInstall!
ECHO.
ECHO Press one of the following keys to start QuickInstall:
ECHO.
ECHO   A   Start QuickInstall Normally
ECHO   B   Disable all PATA and SATA DMA
ECHO   C   PATA and SATA Disk DMA only
ECHO   D   ATAPI (CDROM) DMA only
ECHO   E   Compact Flash Card DMA only
ECHO.
ECHO Selecting A (Start QuickInstall Normally) in 5 seconds...
ECHO.

REM /C defines valid keys
REM /N hides the [A,B,C,D,E]? prompt
REM /T:A,5 makes ENTER or timeout (5 sec) default to A (optional)

CHOICE /C:ABCD /N /T:A,5 "Select option: "

IF ERRORLEVEL 5 GOTO OPTION_E
IF ERRORLEVEL 4 GOTO OPTION_D
IF ERRORLEVEL 3 GOTO OPTION_C
IF ERRORLEVEL 2 GOTO OPTION_B
IF ERRORLEVEL 1 GOTO OPTION_A

:OPTION_A 
ECHO Starting QuickInstall normally...
set CMDLINE=tsc=unstable noapic acpi=off
GOTO END

:OPTION_B
ECHO Disabling all PATA and SATA DMA...
set CMDLINE=tsc=unstable noapic acpi=off libata.dma=0
GOTO END

:OPTION_C
ECHO Enabling PATA and SATA disk DMA only...
set CMDLINE=tsc=unstable noapic acpi=off libata.dma=1
GOTO END

:OPTION_D
ECHO Enabling ATAPI (CDROM) DMA only...
set CMDLINE=tsc=unstable noapic acpi=off libata.dma=2
GOTO END

:OPTION_D
ECHO Enabling CF-Card DMA only...
set CMDLINE=tsc=unstable noapic acpi=off libata.dma=4
GOTO END

:END
ECHO QuickInstall Command Line: %CMDLINE%

LOADLIN BZIMAGE.CD %CMDLINE%