#!/bin/sh

# This script builds the Win98QI ISO.
# It expects the OS files to be in _OS_ROOT_

set -e

echo "Building Windows 98 QuickInstall ISO"

BASEDIR=$PWD
ISODIR=$(realpath .)/__ISO__
ISOFILE=$ISODIR/win98qi_$(date +%Y%m%d_%H%M).iso
OUTPUT=./__OUTPUT__
EXTRA=./_EXTRA_CD_FILES_
DRIVER=$(realpath ./_DRIVER_)
DRIVEREX=./_EXTRA_DRIVER_
DRIVEREXOUTPUT=$OUTPUT/DRIVER.EX
DRIVERTMP=./.drvtmp
OSROOT=./_OS_ROOT_
CDROOTSOURCE=./cdromroot
OEMINFO=./_OEMINFO_

# Sorry, too busy right now to natively port this to linux :(
MERCYPAK="mercypak/mercypak"
DRIVERCOPY="wine tools/drivercopy.exe"

osroot_file_delete () {
  local file_name=$1
  echo $file_name
  find . -iwholename "$file_name" -delete
}

rm -rf $OUTPUT
rm -rf $ISODIR
mkdir -p $OUTPUT
mkdir -p $ISODIR

# Build OS data package

# Find windows CAB dir
OSWINCABDIR=$(find "$OSROOT" -iname "precopy2.cab" -printf '%h\n' -quit)
OSWINDIR=$(find "$OSROOT" -iname "win.com" -printf '%h\n' -quit)
OSRELATIVECABDIR=$(echo "$OSWINCABDIR" | sed "s|$OSROOT/||")

echo "Windows CAB directory: $OSWINCABDIR"
echo "Relative CAB path within OS root: $OSRELATIVECABDIR"
echo "Windows directory: $OSWINDIR"

if [ -z "$OSWINDIR" ]; then
  echo "Windows directory not found."
  exit -1

fi

if [ -z "$OSWINCABDIR" ]; then
  echo "Windows CAB / CD directory not found."
  exit -1
fi

#Filter some garbage data first
osroot_file_delete $OSWINDIR/win386.swp
osroot_file_delete $OSWINDIR/sysbckup/*
osroot_file_delete $OSWINDIR/inf/mdm*.inf
osroot_file_delete $OSWINDIR/inf/wdma_*.inf
osroot_file_delete $OSWINDIR/recent/*
osroot_file_delete $OSROOT/bootlog.*
osroot_file_delete $OSROOT/frunlog.txt
osroot_file_delete $OSROOT/detlog.txt
osroot_file_delete $OSROOT/setuplog.txt
osroot_file_delete $OSROOT/scandisk.log
osroot_file_delete $OSROOT/netlog.txt

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
$DRIVERCOPY "$DRIVER" "$DRIVERTMP"
$DRIVERCOPY "$DRIVEREX" "$DRIVEREXOUTPUT"
rm -f makecab.exe

# Important step: separate INFs and CABs
mkdir -p "$DRIVERTMP/$OSRELATIVECABDIR"
mkdir -p "$DRIVERTMP/DRIVER"
mv "$DRIVERTMP"/*.cab "$DRIVERTMP/$OSRELATIVECABDIR" || true
mv "$DRIVERTMP"/*.inf "$DRIVERTMP/DRIVER" || true

$MERCYPAK $DRIVERTMP $OUTPUT/DRIVER.866

# Copy extra CD files
mkdir -p $OUTPUT/extras
cp -r $EXTRA/* $OUTPUT/extras || true
cp -r $CDROOTSOURCE/* $OUTPUT/

if [ ! -f "$OUTPUT/FULL.866" ] || [ ! -f "$OUTPUT/DRIVER.866" ]; then
  echo "The required .866 files do not exist, sysprep cannot continue."
	exit -1
fi

# Build ISO image.
# TODO: Make ISOLINUX variant. 

cd $OUTPUT
mkisofs -r -V "Win98 QuickInstall" -o "$ISOFILE" -b disk.img .
cd $BASEDIR
