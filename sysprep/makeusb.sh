#!/bin/sh

ISOSIZE=$(du -sh --bytes __OUTPUT__ | awk '{print $1}')
PADDING=$((1024 * 10)) # To make sure we have enough space for all files, add some extra 5MB or so
FATSECTORS=$(($ISOSIZE / 512 + $PADDING))
FATOFFSET=2048
DISKSECTORS=$(($FATSECTORS + $FATOFFSET))
BASEDIR=$PWD

DISK=$BASEDIR/.tmp_disk.img
FAT=$BASEDIR/.tmp_fat.img
echo "ISO Size: ${ISOSIZE} bytes"

rm -f "$DISK"
rm -f "$FAT"

# create temporary images
dd if=/dev/zero of="$DISK" bs=512 count=$DISKSECTORS
dd if=/dev/zero of="$FAT" bs=512 count=$FATSECTORS

# make mbr
parted -s "$DISK" mklabel msdos
parted -s "$DISK" mkpart primary fat32 2048s ${FATSECTORS}s
parted -s "$DISK" set 1 boot on


# make FAT file system
mkfs.vfat -F 32 "$FAT"

# copy all files to it
cd "__OUTPUT__"
mcopy -bsv -i "$FAT" * ::
cd "$BASEDIR"

# Copy syslinux boot sector and system files
syslinux -i "$FAT"

# Copy the file system to the disk image
dd if="$FAT" of="$DISK" bs=512 seek=$FATOFFSET conv=notrunc

# Copy syslinux mbr
dd if="./tools/syslinux_mbr.bin" of="$DISK" bs=440 count=1 conv=notrunc

rm -f "$FAT"
rm -f "$1"
mv "$DISK" "$1"