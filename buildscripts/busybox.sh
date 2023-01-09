#!/bin/bash

make -j16 ARCH=x86 CROSS_COMPILE=$BASE/i486-linux-musl-cross/bin/i486-linux-musl-
make -j16 ARCH=x86 CROSS_COMPILE=$BASE/i486-linux-musl-cross/bin/i486-linux-musl- install