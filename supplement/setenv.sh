#!/bin/sh
# Win9x QuickInstall environment setup
export TERMINFO=/usr/lib/terminfo
export LANG=C.UTF-8


echo 1 >/proc/sys/vm/overcommit_memory
echo 90 >/proc/sys/vm/overcommit_ratio