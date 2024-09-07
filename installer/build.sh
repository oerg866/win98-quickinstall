#!/bin/sh

CC=$PWD/../i486-linux-musl-cross/bin/i486-linux-musl-gcc

$CC -s -O3 -g0 --static -Wall -Wextra -pedantic -Werror -pthread *.c anbui/*.c -lpthread -olunmercy
ls -l lunmercy
