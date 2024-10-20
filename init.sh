#!/bin/bash

set -e

#set -x
#trap read debug

git submodule update --init --recursive --depth 1 || true

echo "--------------------------------"
echo "Ignore the above message about not being able to find a certain commit hash,"
echo "it is the EFI submodule of SYSLINUX. We're not building for EFI, so we can"
echo "ignore it."
echo "--------------------------------"

# pre clean so the init script works even when init has been done already
rm -rf ncurses*
rm -rf termtypes*

DOWNLOAD="wget -nc -c"

# Download & unpack compiler

if [ ! -d "i486-linux-musl-cross" ] && [ ! -f "i486-linux-musl-cross.tgz" ]; then
	echo Downloading i486-linux-musl-cross.tgz
	$DOWNLOAD http://musl.cc/i486-linux-musl-cross.tgz
fi;
if [ ! -d "i486-linux-musl-cross.tgz" ]; then
	tar -xvf i486-linux-musl-cross.tgz
fi;

# Download & unpack ncurses

if [ ! -d "ncurses" ] && [ ! -f "ncurses-6.3.tar.gz" ]; then
	echo Downloading ncurses
	$DOWNLOAD https://ftp.gnu.org/pub/gnu/ncurses/ncurses-6.3.tar.gz
fi;
if [ ! -d "ncurses" ]; then
	tar -xvf ncurses-6.3.tar.gz
	mv ncurses-6.3 ncurses
fi;

# Download and build termtype data

mkdir -p termtypes
pushd termtypes

if [ ! -f "termtypes.ti.gz" ]; then
	echo Downloading termtype data
	$DOWNLOAD http://catb.org/terminfo/termtypes.ti.gz
fi;

gunzip termtypes.ti.gz
tic -o . termtypes.ti
popd

# Build config for linux and busybox

cp buildscripts/linux_config linux/.config
cp buildscripts/busybox_config busybox/.config

# Patch the dependencies

pushd linux
	git reset --hard
	git apply ../buildscripts/linux_patches/*
popd

pushd syslinux
	git reset --hard
	git apply ../buildscripts/syslinux_patches/*
popd

# Build the dependencies. 

pushd syslinux
	make -j8 bios
popd


BUILDSCRIPTS="ncurses dosfstools util-linux busybox syslinux pciutils"

for dep in $BUILDSCRIPTS
do
	cp buildscripts/$dep.sh $dep/build.sh
done

# Prepare things to be built properly

export CC=$PWD/i486-linux-musl-cross/bin/i486-linux-musl-gcc
export CXX=$PWD/i486-linux-musl-cross/bin/i486-linux-musl-g++
export PREFIX=$PWD/i486-linux-musl-cross/i486-linux-musl
export LIBDIR=$PREFIX/lib
export INCLUDEDIR=$PREFIX/include

pushd ncurses
	./configure --prefix=$PREFIX --host=i486-linux-musl --enable-widec
	./build.sh
popd

pushd util-linux
	./autogen.sh
	./configure --host=i486-linux-musl --prefix=$PWD/OUTPUT --disable-use-tty-group --disable-bash-completion --disable-shared --enable-static --without-tinfo NCURSESW6_CONFIG=$PREFIX/bin/ncursesw6-config
	./build.sh
popd

pushd dosfstools
        ./autogen.sh
        ./configure --host=i486-linux-musl --prefix=$PWD/OUTPUT --bindir=$PWD/OUTPUT/bin --sbindir=$PWD/OUTPUT/sbin
        ./build.sh
popd

echo "Assuming nothing went wrong, this should be ready to go! :)"
echo "Run ./build.sh to generate the framework!"
