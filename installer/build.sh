#!/bin/sh

CC=$PWD/../i486-linux-musl-cross/bin/i486-linux-musl-gcc

$CC --static -Wall -pthread main.c ringbuf.c util.c disk.c install.c -lpthread -olunmercy
