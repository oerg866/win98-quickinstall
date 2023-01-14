@ECHO OFF
:: File for mingw builds, but we do include binaries in the repo because windows folks don't do 
gcc --static -Wall -Werror -omercypak32.exe mercypak.c mpak_lib.c