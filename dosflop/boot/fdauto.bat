@echo off

SHSUCDX.COM /D:WIN98QI

:do_findcd
call findcd.bat

loadlin %CDROM%\bzImage.cd

:fail
cls
echo LOAD ERROR OR CD-ROM DRIVE NOT FOUND. CANNOT LOAD KERNEL.