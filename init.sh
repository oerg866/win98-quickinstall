#!/bin/bash

set -e

#set -x
#trap read debug

# required packages
# build-essential gcc-7 tic python3 gunzip libuuid wine dd nasm mkisofs zip mtools syslinux

# Find GCC version
gcc_version=$(gcc -dumpversion)

if [ "$gcc_version" -gt 7 ]; then
# TODO File a bug maybe? I'm too incompetent to debug it..
	echo "A GCC version higher than 7 was found on the host system."
	echo "This may produce broken linux kernel code for i486 and i586."
	echo "It will probably only work on i686 and higher class machines."
	read -n 1 -s -r -p "Press any key to continue."
fi

git submodule update --init --recursive --depth 1 || true

echo "--------------------------------"
echo "Ignore the above message about not being able to find a certain commit hash,"
echo "it is the EFI submodule of SYSLINUX. We're not building for EFI, so we can"
echo "ignore it."
echo "--------------------------------"

# pre clean so the init script works even when init has been done already
rm -f *.tgz*
rm -f *.tar*
rm -rf busybox*
rm -rf ncurses*
rm -rf i486-linux-musl*
rm -rf dialog*
rm -rf termtypes*

DOWNLOAD="wget -nc -c"

echo Downloading i486-linux-musl-cross.tgz
$DOWNLOAD http://musl.cc/i486-linux-musl-cross.tgz
tar -xvf i486-linux-musl-cross.tgz

# Download busybox
echo Downloading busybox
$DOWNLOAD https://www.busybox.net/downloads/busybox-1.35.0.tar.bz2
tar -xvf busybox-1.35.0.tar.bz2
rm busybox*.tar.bz2
mv busybox* busybox

# Download dialog
echo Downloading dialog
$DOWNLOAD https://invisible-island.net/datafiles/release/dialog.tar.gz
tar -xvf dialog.tar.gz
mv dialog-* dialog

# Download ncurses
echo Downloading ncurses
$DOWNLOAD https://ftp.gnu.org/pub/gnu/ncurses/ncurses-6.3.tar.gz
tar -xvf ncurses-6.3.tar.gz
rm ncurses*.tar.gz
mv ncurses* ncurses

# Download and build timezone data
mkdir -p termtypes
pushd termtypes
echo Downloading timezone data
$DOWNLOAD http://catb.org/terminfo/termtypes.ti.gz
gunzip termtypes.ti.gz
tic -o . termtypes.ti
popd

rm -rf *.gz
rm -rf *.tgz

# Build config for linux and busybox

cp buildscripts/linux_config linux/.config
cp buildscripts/busybox_config busybox/.config


# Build the dependencies. 

BUILDSCRIPTS="ncurses dialog dosfstools util-linux busybox syslinux"

for dep in $BUILDSCRIPTS
do
	cp buildscripts/$dep.sh $dep/build.sh
done

# We build SYSLINUX anew because it *must* be built with GCC7, with GCC9 it fails on <i686.
pushd syslinux
	make -j8 bios
popd

# Prepare things to be built properly

export CC=$PWD/i486-linux-musl-cross/bin/i486-linux-musl-gcc
export CXX=$PWD/i486-linux-musl-cross/bin/i486-linux-musl-g++
export PREFIX=$PWD/i486-linux-musl-cross/i486-linux-musl
export LIBDIR=$PREFIX/lib
export INCLUDEDIR=$PREFIX/include

pushd ncurses
	./configure --prefix=$PREFIX --host=i486-linux-musl --with-termlib
	./build.sh
popd

pushd dialog
	# binary goes in dialog/OUTPUT, lib & include goes in compiler dir
	./configure --host=i486-linux-musl --prefix=$PWD/OUTPUT --libdir=$LIBDIR --includedir=$INCLUDEDIR
	./build.sh
popd

pushd util-linux
	./autogen.sh
	./configure --host=i486-linux-musl --prefix=$PWD/OUTPUT --disable-use-tty-group --disable-bash-completion --disable-shared --enable-static --without-ncursesw --without-tinfo
	./build.sh
popd

echo "Assuming nothing went wrong, this should be ready to go! :)"
echo "Run ./build.sh to generate the framework!"