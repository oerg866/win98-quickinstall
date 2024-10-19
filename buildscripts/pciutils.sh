#!/bin/bash

export CC=$PWD/../i486-linux-musl-cross/bin/i486-linux-musl-gcc
export CXX=$PWD/../i486-linux-musl-cross/bin/i486-linux-musl-g++

make -j8 CROSS_COMPILE=$PWD/../i486-linux-musl-cross/bin/i486-linux-musl- LDFLAGS="--static"
