#!/bin/bash

export CC=$PWD/../i486-linux-musl-cross/bin/i486-linux-musl-gcc
export CXX=$PWD/../i486-linux-musl-cross/bin/i486-linux-musl-g++

make -j8 CFLAGS="--static" LDFLAGS="--static"

make install-usrbin_execPROGRAMS prefix=$PWD/OUTPUT
make install-usrsbin_execPROGRAMS prefix=$PWD/OUTPUT
