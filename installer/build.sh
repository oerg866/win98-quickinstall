#!/bin/sh

CC=$PWD/../i486-linux-musl-cross/bin/i486-linux-musl-gcc

set -e 1

ANBUI_FILES=$(anbui/get_build_files.sh)

$CC -DMAPPEDFILE_MULTITHREAD -Os -s -g0 --static -Wall -Wextra -pedantic -Werror -pthread $ANBUI_FILES install.c install_disk.c install_util.c util.c util_disk.c mappedfile_mt.c main.c -lpthread -olunmercy

ls -l lunmercy*
