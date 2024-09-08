#!/bin/sh
# Win9x QuickInstall environment setup
export TERMINFO=/usr/lib/terminfo
export LANG=C.UTF-8


echo 2 >/proc/sys/vm/overcommit_memory
echo 95 >/proc/sys/vm/overcommit_ratio