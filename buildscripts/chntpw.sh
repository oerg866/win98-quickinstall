#!/bin/sh

CC=$PWD/../i486-linux-musl-cross/bin/i486-linux-musl-gcc

set -e 1

echo $CC

make clean reged.static
