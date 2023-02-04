#!/bin/sh

# This script builds the Win98QI ISO.
# It expects the OS files to be in _OS_ROOT_

set -e

echo "Building Windows 98 QuickInstall ISO"

BASEDIR=$PWD
ISODIR=$(realpath .)/__ISO__
OUTPUT=$PWD/__OUTPUT__
EXTRA=$PWD/_EXTRA_CD_FILES_
DRIVER=$PWD/_DRIVER_
DRIVEREX=$PWD/_EXTRA_DRIVER_
DRIVEREXOUTPUT=$OUTPUT/DRIVER.EX
REGTMP=$PWD/.regtmp
CDROOTSOURCE=$PWD/cdromroot
OEMINFO=$PWD/_OEMINFO_

# Sorry, too busy right now to natively port this to linux :(
MERCYPAK=$(realpath "./mercypak/mercypak")
DRIVERCOPY="wine tools/drivercopy.exe"

# This is necessary because windows doesn't care about cases but linux does...
osroot_file_delete () {
  find "$1" -iwholename "*$2" -print -delete
}

rm -rf $OUTPUT
rm -rf $ISODIR
mkdir -p $OUTPUT
mkdir -p $ISODIR

# Build OS data package


# Figure out OS root directory and ISO path
OSROOT=$1
ISOFILE=$(realpath $2)
if [ -z "$ISOFILE" ]; then
  ISOFILE=$ISODIR/win98qi_$(date +%Y%m%d_%H%M).iso
fi

if [ -z "$OSROOT" ]; then
  OSROOT=./_OS_ROOT_
fi

# Build OS data package

# Find windows CAB dir
OSWINCABDIR=$(find "$OSROOT" -iname "precopy2.cab" -printf '%h\n' -quit)
OSWINDIR=$(find "$OSROOT" -iname "win.com" -printf '%h\n' -quit)
OSRELATIVECABDIR=$(echo "$OSWINCABDIR" | sed "s|$OSROOT/||")
OSRELATIVEWINDIR=$(echo "$OSWINDIR" | sed "s|$OSROOT/||")

echo "Windows directory: $OSWINDIR"
echo "Relative Windows directory within OS root: $OSRELATIVEWINDIR"
echo "Windows CAB directory: $OSWINCABDIR"
echo "Relative CAB path within OS root: $OSRELATIVECABDIR"

if [ -z "$OSWINDIR" ]; then
  echo "Windows directory not found."
  exit 1
fi

if [ -z "$OSWINCABDIR" ]; then
  echo "Windows CAB / CD directory not found."
  exit 1
fi

# Prepare registry
# Find SYSTEM.DAT and USER.DAT files
SYSTEMDAT=$(find "$OSWINDIR" -maxdepth 1 -iname "SYSTEM.DAT" -print -quit)
USERDAT=$(find "$OSWINDIR" -maxdepth 1 -iname "USER.DAT" -print -quit)

if [ ! -f "$SYSTEMDAT" ]; then
  echo "Windows registry not found."
  exit 127
fi

# Process registry

cd registry

make_registry () {
  local system_dat=$1
  local user_dat=$2
  local reg_file=$3
  local output_file=$4
  echo Adding $reg_file to registry...
  rm -rf "$REGTMP"
  mkdir -p "$REGTMP/$OSRELATIVEWINDIR" 
  cp -f "$system_dat" ./SYSTEM.DAT
  cp -f "$user_dat" ./USER.DAT
  wine ../tools/msdos.exe regedit.exe /L:SYSTEM.DAT /R:USER.DAT $reg_file
  cp -f ./SYSTEM.DAT "$REGTMP/$OSRELATIVEWINDIR"
  cp -f ./USER.DAT "$REGTMP/$OSRELATIVEWINDIR"
  $MERCYPAK "$REGTMP" "$output_file"
}

# Do Slow Non-PNP detection variant
make_registry "$SYSTEMDAT" "$USERDAT" slowpnp.reg "$OUTPUT/SLOWPNP.866"
# Fast PNP-only detection variant
make_registry "$SYSTEMDAT" "$USERDAT" fastpnp.reg "$OUTPUT/FASTPNP.866"

cd "$BASEDIR"

#Filter some garbage data first
osroot_file_delete "$OSWINDIR" win386.swp
osroot_file_delete "$OSWINDIR" sysbckup/*
osroot_file_delete "$OSWINDIR" inf/mdm*.inf
osroot_file_delete "$OSWINDIR" inf/wdma_*.inf
osroot_file_delete "$OSWINDIR" recent/*
osroot_file_delete "$OSROOT" bootlog.*
osroot_file_delete "$OSROOT" frunlog.txt
osroot_file_delete "$OSROOT" detlog.txt
osroot_file_delete "$OSROOT" setuplog.txt
osroot_file_delete "$OSROOT" scandisk.log
osroot_file_delete "$OSROOT" netlog.txt
osroot_file_delete "$OSROOT" suhdlog.dat

# Copy oeminfo
cp $OEMINFO/* $OSWINDIR/SYSTEM

# Create MercyPak file for the system root
$MERCYPAK $OSROOT $OUTPUT/FULL.866

# Build driver package
# This *has* to be done using WINE sadly, because linux neither has a convenient way of
# creating Win9x compatible CAB files, nor exists a library (to my knowledge) to parse
# Win9x style INF files, other than what WINE ships with, and I'm not trusting that
# so this ships with a copy of windows XP's SETUPAPI.DLL, because it knows how a
# Win9x style INF file exactly has to look.
mkdir -p $DRIVEREXOUTPUT
cp tools/makecab.exe .
WINE_DRIVER=$(winepath -w "$DRIVER")
WINE_DRIVEREX=$(winepath -w "$DRIVEREX")

# Working around a bug in drivercopy (or more specifically makecab) where the output has to be relative
# So all the cab files go into a local directory.

DRIVERTMP=.drvtmp

$DRIVERCOPY "$WINE_DRIVER" "$DRIVERTMP"

# Important step: separate INFs and CABs
mkdir -p "$DRIVERTMP/$OSRELATIVECABDIR"
mkdir -p "$DRIVERTMP/DRIVER"
mv "$DRIVERTMP"/*.cab "$DRIVERTMP/$OSRELATIVECABDIR" || true
mv "$DRIVERTMP"/*.inf "$DRIVERTMP/DRIVER" || true

$MERCYPAK "$DRIVERTMP" "$OUTPUT/DRIVER.866"

rm -rf "$DRIVERTMP"/*

$DRIVERCOPY "$WINE_DRIVEREX" "$DRIVERTMP"
cp -rf "$DRIVERTMP"/* "$DRIVEREXOUTPUT"

rm -f makecab.exe

# Copy extra CD files
mkdir -p $OUTPUT/extras
cp -r $EXTRA/* $OUTPUT/extras || true
cp -r $CDROOTSOURCE/* $OUTPUT/

if [ ! -f "$OUTPUT/FULL.866" ] || [ ! -f "$OUTPUT/DRIVER.866" ]; then
  echo "The required .866 files do not exist, sysprep cannot continue."
exit 1
fi

# Build ISO image.

cd $OUTPUT
mkisofs -J -r -V "Win98 QuickInstall" -o "$ISOFILE" -b cdrom.img .
cd $BASEDIR
