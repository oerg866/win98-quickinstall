@ECHO OFF

:: This script builds the Win98QI ISO.
:: It expects the OS files to be in _OS_ROOT_

SET BASEDIR=%CD%
SET ISODIR=%CD%\__ISO__
SET DRIVER=%CD%\_DRIVER_
SET DRIVEREX=%CD%\_EXTRA_DRIVER_
SET DRIVERTMP=%BASEDIR%\.drvtmp
SET OUTPUT=%CD%\__OUTPUT__
SET OSROOT=%CD%\_OS_ROOT_
SET OEMINFO=%CD%\_OEMINFO_

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
del /Q "%OSWINDIR%\WIN386.SWP"
del /Q "%OSWINDIR%\SYSBKUP\*"
del /Q "%OSWINDIR%\INF\MDM*.INF"
del /Q "%OSWINDIR%\INF\WDMA_*.INF"
del /Q "%OSWINDIR%\RECENT\*"
del /Q "%OSROOT%\BOOTLOG.*"
del /Q "%OSROOT%\FRUNLOG.TXT"
del /Q "%OSROOT%\DETLOG.TXT"
del /Q "%OSROOT%\SETUPDLOG.TXT"
del /Q "%OSROOT%\SCANDISK.LOG"
del /Q "%OSROOT%\NETLOG.TXT"


:: Copy oeminfo
cp %OEMINFO%\*.* %OSWINDIR%/SYSTEM

:: Create MercyPak file for the system root
mercypak\mercypak32.exe "%OSROOT%" "%OUTPUT%\FULL.866"

:: Build driver package
md "%DRIVERTMP%"

pushd tools
drivercopy.exe "%DRIVER%" "%DRIVERTMP%"
popd

:: Important step: separate INFs and CABs
CALL set OSRELATIVECABDIR=%%OSWINCABDIR:%BASEDIR%=%%
echo %OSRELATIVECABDIR%
md "%DRIVERTMP%\%OSRELATIVECABDIR%"
md "%DRIVERTMP%\%DRIVER%"
move "%DRIVERTMP%\*.cab" "%DRIVERTMP%\%OSRELATIVECABDIR%"

if not exist "%OUTPUT%\FULL.866" (
    goto NOTFOUND
)

if not exist "%OUTPUT%\DRIVER.866" (
    goto NOTFOUND
)

mkisofs -r -V "Win98 QuickInstall" -o "%ISOFILE%" -b disk.img .

exit 0

:NOTFOUND
echo "The required .866 files do not exist, sysprep cannot continue."
exit 1