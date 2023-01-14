#!/bin/sh

CC=$PWD/../i486-linux-musl-cross/bin/i486-linux-musl-gcc

gcc --static -Wall -Werror -omercypak mercypak.c mpak_lib.c
