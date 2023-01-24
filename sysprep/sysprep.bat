:: @ECHO OFF

:: This script builds the Win98QI ISO.
:: It expects the OS files to be in _OS_ROOT_

SET BASEDIR=%CD%
SET ISODIR=%CD%\__ISO__
SET DRIVER=%CD%\_DRIVER_
SET DRIVEREX=%CD%\_EXTRA_DRIVER_
SET DRIVERTMP=%BASEDIR%\.drvtmp
SET DRIVERINFOUTPUT=%DRIVERTMP%\DRIVER
SET EXTRA=%CD%\_EXTRA_CD_FILES_
SET OUTPUT=%CD%\__OUTPUT__
SET DRIVEREXOUTPUT=%OUTPUT%\DRIVER.EX
SET OSROOT=%CD%\_OS_ROOT_
SET OEMINFO=%CD%\_OEMINFO_
SET CDROOTSOURCE=%CD%\cdromroot

:: no timestamps, sorry
SET ISOFILE=%ISODIR%\win98qi.iso

rmdir /S /Q "%OUTPUT%"
rmdir /S /Q "%ISODIR%"
md "%OUTPUT%"
md "%ISODIR%"

:: find windows CAB dir

pushd "%OSROOT%"
for /R %%f in (PRECOPY2.CAB) do @IF EXIST %%f set OSWINCABDIR=%%~dpf
for /R %%f in (WIN.COM) do @IF EXIST %%f set OSWINDIR=%%~dpf
popd

if ("%OSWINDIR%"=="") (
    echo "Windows diectory not found."    
    exit -1
)

if ("%OSWINCABDIR%"=="") (
    echo "Windows CAB / CD directory not found."    
    exit -1
)

:: Filter some garbage data first
del /Q /F /A "%OSWINDIR%\WIN386.SWP"
del /Q /F /A "%OSWINDIR%"\SYSBCKUP\*
del /Q /F /A "%OSWINDIR%"\INF\MDM*.INF
del /Q /F /A "%OSWINDIR%"\INF\WDMA_*.INF
del /Q /F /A "%OSWINDIR%"\RECENT\*
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
CALL set OSRELATIVECABDIR=%%OSWINCABDIR:%OSROOT%=%%
echo %OSRELATIVECABDIR%
md "%DRIVERTMP%\%OSRELATIVECABDIR%"
md "%DRIVERTMP%\%DRIVER%"
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
..\tools\mkisofs -J -r -V "Win98 QuickInstall" -o "%ISOFILE%" -b disk.img .
popd

echo Done.
echo The ISO file was created at %ISOFILE%.

goto END

:NOTFOUND
echo The required .866 files do not exist, sysprep cannot continue.

:END