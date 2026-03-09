@echo off
del *.obj 2>nul
del *.exe 2>nul
wcl386 main.c -bt=nt -l=nt_win -4 -q -fe=qisetup
del *.obj 2>nul