@echo off

SHSUCDX.COM /D:WIN98QI

:do_findcd
call findcd.bat

%CDROM%
install.bat

:fail
cls
echo LOAD ERROR OR CD-ROM DRIVE NOT FOUND. CANNOT LOAD KERNEL.