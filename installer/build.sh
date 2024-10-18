#!/bin/sh

CC=$PWD/../i486-linux-musl-cross/bin/i486-linux-musl-gcc

set -e 1

ANBUI_FILES=$(anbui/get_build_files.sh)

$CC -DMAPPEDFILE_MULTITHREAD -Os -s -g0 --static -Wall -Wextra -pedantic -Werror -pthread $ANBUI_FILES disk.c install.c util.c mappedfile_mt.c main.c -lpthread -olunmercy
$CC -DMAPPEDFILE_MULTITHREAD -Os -s -g0 --static -Wall -Wextra -pedantic -Werror $ANBUI_FILES disk.c install.c util.c mappedfile.c main.c -olunmercy_singlethread

ls -l lunmercy*
