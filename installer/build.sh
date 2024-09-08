#!/bin/sh

CC=$PWD/../i486-linux-musl-cross/bin/i486-linux-musl-gcc

set -e 1

$CC -DMAPPEDFILE_MULTITHREAD -O3 -s -g0 --static -Wall -Wextra -pedantic -Werror -pthread anbui/*.c disk.c install.c util.c mappedfile_mt.c main.c -lpthread -olunmercy
$CC -DMAPPEDFILE_MULTITHREAD -O3 -s -g0 --static -Wall -Wextra -pedantic -Werror anbui/*.c disk.c install.c util.c mappedfile.c main.c -olunmercy_singlethread

ls -l lunmercy*
