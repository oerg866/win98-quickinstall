#!/bin/sh

CC=$PWD/../i486-linux-musl-cross/bin/i486-linux-musl-gcc

$CC -O2 --static -Wall -pthread main.c ringbuf.c util.c disk.c install_ui.c install.c -lpthread -ldialog -lncurses -ltinfo -olunmercy
