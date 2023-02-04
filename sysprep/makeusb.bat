
@echo off

:: First, find the size of all files and give it some generous padding
set ISOSIZE=0
pushd __OUTPUT__
for /r %%x in (*) do set /a ISOSIZE+=%%~zx
echo %ISOSIZE% Bytes
popd

set /a PADDING=50 * 1048576
set /a FATOFFSET=2048 * 512 / 1024
set /a FATSIZE=%ISOSIZE% + %PADDING%

:: Calc total disk size, align this to like a 1MB boundary
set /a DISKSIZE=(%FATSIZE% + %FATOFFSET%)
set /a DISKSIZE=%DISKSIZE% / 1048576 * 1048576 + 1048576
echo "DiskSize = %DISKSIZE%"

set /a DISKMB=%DISKSIZE% / 1048576
set /a FATOFFSETKB=%FATOFFSET% / 1024

:: we create the diskpart script and image in the TEMP folder
:: because it requires admin permissions and that means
:: if we're on a share it might not be accessible

:: Before we begin, we must check if we have a free drive letter to temporarily mount the image
for %%i in (Z Y X W V U T S R Q P O N M L K J I H G F E D C B A) do if not exist %%i:\ (
    echo %%i: is unused
    set TEMPLETTER=%%i
    goto found
)

echo No unused drive letters found
exit /b 127
:found

:: first, unmount old reminants

echo automount disable >diskpart.txt
echo select vdisk FILE='%TEMP%\img.vhd' NOERR >>diskpart.txt
echo detach vdisk NOERR >>diskpart.txt
copy /Y diskpart.txt "%TEMP%"
diskpart /s "%TEMP%\diskpart.txt"

del /F %TEMP%\img.vhd


:: create partition table and single fat32 partition

echo automount disable >diskpart.txt
echo create vdisk FILE='%TEMP%\img.vhd' TYPE=FIXED MAXIMUM=%DISKMB% >>diskpart.txt
echo attach vdisk >>diskpart.txt
echo clean >>diskpart.txt
echo convert mbr >>diskpart.txt
echo create partition primary offset=%FATOFFSET% ID=0C >>diskpart.txt
echo select partition 1 >>diskpart.txt
echo assign letter='%TEMPLETTER%' >>diskpart.txt
echo format fs=fat32 unit=512 quick >>diskpart.txt
echo active >>diskpart.txt
copy /Y diskpart.txt "%TEMP%"
diskpart /s "%TEMP%\diskpart.txt"

:: Copy all files to the mountpoint
xcopy /E /S __OUTPUT__\* "%TEMPLETTER%:\"

:: Write SYSLINUX and its files to image
tools\syslinux.exe -i -f "%TEMPLETTER%:"

:: unmount the disk once we're done
echo automount disable >diskpart.txt
echo select vdisk FILE='%TEMP%\img.vhd' NOERR >>diskpart.txt
echo detach vdisk NOERR >>diskpart.txt
copy /Y diskpart.txt "%TEMP%"
diskpart /s "%TEMP%\diskpart.txt"

:: Write MBR to disk
tools\ddrelease64.exe if="tools\syslinux_mbr.bin" of="%TEMP%\img.vhd" bs=440 count=1 conv=notrunc

del /Q "%1"
move "%TEMP%\img.vhd" "%1"

exit /b

:realpath
    set RET=%~f1
    exit /b

