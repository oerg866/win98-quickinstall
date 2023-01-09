#!/bin/bash

export CC=$PWD/../i486-linux-musl-cross/bin/i486-linux-musl-gcc
export CXX=$PWD/../i486-linux-musl-cross/bin/i486-linux-musl-g++

OUTPUT=$PWD/OUTPUT
mkdir $OUTPUT

./autogen.sh
./configure --host=i486-linux-musl --prefix=$OUTPUT --bindir=$OUTPUT/bin --sbindir=$OUTPUT/sbin

make -j8 CFLAGS="--static" LDFLAGS="--static"
make install