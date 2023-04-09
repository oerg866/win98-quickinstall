:: @ECHO OFF

:: This script builds the Win98QI ISO.

SET BASEDIR=%CD%
SET ISODIR=%CD%\__ISO__
SET DRIVER=%CD%\_DRIVER_
SET DRIVEREX=%CD%\_EXTRA_DRIVER_
SET DRIVERTMP=%BASEDIR%\.drvtmp
SET DRIVERINFOUTPUT=%DRIVERTMP%\DRIVER
SET EXTRA=%CD%\_EXTRA_CD_FILES_
SET OUTPUT=%CD%\__OUTPUT__
SET DRIVEREXOUTPUT=%OUTPUT%\DRIVER.EX
SET REGTMP=%CD%\.regtmp
SET OEMINFO=%CD%\_OEMINFO_
SET CDROOTSOURCE=%CD%\cdromroot

:: If the OS ROOT and ISO FILE parameters are given, set variables to them
CALL :realpath "%1"
SET OSROOT=%RET%
CALL :realpath "%2"
SET ISOFILE=%RET%

if [%OSROOT%] == [] SET OSROOT=%CD%\_OS_ROOT_

if [%ISOFILE%] == [] SET ISOFILE=%ISODIR%\win98qi.iso

echo OS Root: %OSROOT%

rmdir /S /Q "%OUTPUT%"
rmdir /S /Q "%ISODIR%"
md "%OUTPUT%"
md "%ISODIR%"

:: find windows CAB dir

pushd "%OSROOT%"
for /R %%f in (PRECOPY2.CAB) do @IF EXIST %%f set OSWINCABDIR=%%~dpf
for /R %%f in (WIN.COM) do @IF EXIST %%f set OSWINDIR=%%~dpf
popd

CALL set OSRELATIVECABDIR=%%OSWINCABDIR:%OSROOT%=%%
CALL set OSRELATIVEWINDIR=%%OSWINDIR:%OSROOT%=%%

if ("%OSWINDIR%"=="") (
    echo "Windows diectory not found."    
    exit /b -1
)

if ("%OSWINCABDIR%"=="") (
    echo "Windows CAB / CD directory not found."    
    exit /b -1
)

echo Relative CAB dir: %OSRELATIVECABDIR%
echo Relative WIN dir: %OSRELATIVEWINDIR%

:: Prepare registry
:: Find SYSTEM.DAT and USER.DAT files

set SYSTEMDAT=%OSWINDIR%\SYSTEM.DAT
set USERDAT=%OSWINDIR%\USER.DAT

if not exist "%SYSTEMDAT%" (
    echo "Registry not found."
    exit /b -1
)

:: Process registry

pushd registry
    call :make_registry "%SYSTEMDAT%" "%USERDAT%" slowpnp.reg "%OUTPUT%\SLOWPNP.866"
    call :make_registry "%SYSTEMDAT%" "%USERDAT%" fastpnp.reg "%OUTPUT%\FASTPNP.866"
popd

:: Filter some garbage data first
del /Q /F /A "%OSWINDIR%\WIN386.SWP"
del /Q /F /A "%OSWINDIR%"\SYSBCKUP\*
del /Q /F /A "%OSWINDIR%"\INF\MDM*.INF
del /Q /F /A "%OSWINDIR%"\INF\WDMA_*.INF
del /Q /F /A "%OSWINDIR%"\RECENT\*
del /Q /F /A "%OSROOT%\WIN386.SWP"
del /Q /F /A "%OSROOT%"\BOOTLOG.*
del /Q /F /A "%OSROOT%\FRUNLOG.TXT"
del /Q /F /A "%OSROOT%\DETLOG.TXT"
del /Q /F /A "%OSROOT%\SETUPLOG.TXT"
del /Q /F /A "%OSROOT%\SCANDISK.LOG"
del /Q /F /A "%OSROOT%\NETLOG.TXT"

:: Copy oeminfo
copy "%OEMINFO%"\*.* "%OSWINDIR%"\SYSTEM

:: Create MercyPak file for the system root
mercypak\mercypak32.exe "%OSROOT%" "%OUTPUT%\FULL.866"

:: Build driver package
md "%DRIVERTMP%"

pushd "%DRIVERTMP%"
:: working around a bug in drivercopy, which for reasons I cannot fix at the moment, so only relative paths work
:: because makecab CabinetNameTemplate can't be a directory, only a filename... the directory has to go in DestinationDir

copy "%BASEDIR%"\tools\makecab.exe .
"%BASEDIR%"\tools\drivercopy.exe "%DRIVER%" .
del makecab.exe
popd

:: Important step: separate INFs and CABs
md "%DRIVERTMP%\%OSRELATIVECABDIR%"
md "%DRIVERINFOUTPUT%"
move "%DRIVERTMP%\*.cab" "%DRIVERTMP%\%OSRELATIVECABDIR%"
move "%DRIVERTMP%\*.inf" "%DRIVERINFOUTPUT%"

:: Process extra drivers
md %DRIVEREXOUTPUT%
pushd "%DRIVEREXOUTPUT%"
copy "%BASEDIR%"\tools\makecab.exe .
"%BASEDIR%"\tools\drivercopy.exe "%DRIVEREX%" .
del makecab.exe
popd

mercypak\mercypak32.exe "%DRIVERTMP%" "%OUTPUT%\DRIVER.866"

:: Copy extra CD files
xcopy /R /E /S /H /C /I "%EXTRA%" "%OUTPUT%\extras"
xcopy /R /E /S /H /C /I "%CDROOTSOURCE%" "%OUTPUT%"

if not exist "%OUTPUT%\FULL.866" (
    goto NOTFOUND
)

if not exist "%OUTPUT%\DRIVER.866" (
    goto NOTFOUND
)

:: Create ISO
pushd "%OUTPUT%"
del /Q /F /A %ISOFILE%
..\tools\mkisofs -J -r -V "Win98 QuickInstall" -o "%ISOFILE%" -b cdrom.img .
popd

echo Done.
echo The ISO file was created at %ISOFILE%.
exit /b

:NOTFOUND
echo The required .866 files do not exist, sysprep cannot continue.
exit /b

:: Realpath equivalent for Windows, quite hacky...
:realpath
    set RET=%~f1
    exit /b

:: Make registry adjustments
:: %1 = SYSTEM.DAT path
:: %2 = USER.DAT path
:: %3 = registry file
:: %4 = output 866 file
:make_registry
    echo Adding %3 to the registry.
    rmdir /S /Q %REGTMP%
    mkdir "%REGTMP%\%OSRELATIVEWINDIR%"
    del /Q /F /A SYSTEM.DAT
    del /Q /F /A USER.DAT
    xcopy /H /Y %1 .
    xcopy /H /Y %2 .
    copy %3 tmp.reg

    if exist "%OSWINDIR%\SYSTEM\SHELL32.W98" (
        echo 98Lite on Win98 detected, using SHELL32.W98 reboot method
        echo "Reboot"="RUNDLL32.EXE SHELL32.W98,SHExitWindowsEx 2" >>tmp.reg
    ) else (
        :: batch doesn't have else if ............
        if exist "%OSWINDIR%\SYSTEM\SHELL32.WME" (
            echo 98Lite on WinME detected, using SHELL32.WME reboot method
            echo "Reboot"="RUNDLL32.EXE SHELL32.WME,SHExitWindowsEx 2" >>tmp.reg
        ) else (
            echo Stock Win98 detected, using SHELL32.DLL reboot method
            echo "Reboot"="RUNDLL32.EXE SHELL32.DLL,SHExitWindowsEx 2" >>tmp.reg
        )
    )

    %BASEDIR%\tools\msdos.exe regedit.exe /L:SYSTEM.DAT /R:USER.DAT tmp.reg

    del /Q /F /A tmp.reg 

    xcopy /H /Y SYSTEM.DAT "%REGTMP%\%OSRELATIVEWINDIR%"
    xcopy /H /Y USER.DAT "%REGTMP%\%OSRELATIVEWINDIR%"
    %BASEDIR%\mercypak\mercypak32.exe "%REGTMP%" %4
    exit /b