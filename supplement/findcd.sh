#!/bin/sh

CD=/install
DEV=

beginsWith() { case $2 in "$1"*) true;; *) false;; esac; }

tryMountPattern()
{
	echo "Searching for '$2' file systems in '/dev/${1}*'"
	for f in ${1}*
	do
		# If nothing is found, the script will be run with literally a * attached to the parameter
		# so we check if the strings are equal to omit that case...
		if [ "$f" = "${1}*" ]; then return 1; fi;

		if mount -t $2 /dev/$f $CD >/dev/null 2>&1 ; then
			echo "$2 fs found on $f"
			if test -e $CD/bzImage.cd ; then
				DEV=/dev/$f
				return 0
			fi
			umount -f $CD
		fi
	done
	return 1
}

mkdir -p $CD

cd /dev

while :
do 
	if tryMountPattern sr iso9660; then break; fi;		# CD-ROM
	if tryMountPattern cd iso9660; then break; fi;	    # CD-ROM (alternate)
	if tryMountPattern ide iso9660; then break; fi;		# CD-ROM (alternate)
	if tryMountPattern sd iso9660; then break; fi;		# Flash drives can be in ISO 
	if tryMountPattern hd iso9660; then break; fi;		# Flash drives can be in ISO 
	if tryMountPattern sd vfat; then break; fi;			# Flash drive or hard drive in FAT32
	if tryMountPattern hd vfat; then break; fi;			# Flash drive or hard drive in FAT32
	if tryMountPattern nv vfat; then break; fi;			# NVME (lol?)
	break
done

# back to root
cd /

if test -z "$DEV"; then
	echo Install media / CD-ROM Not found!
	return 127
else
	echo Install media / CD-ROM found and mounted at $CD
	# Copy some files to memory for faster execution
	cp $CD/bin/lunmercy /bin
	cp $CD/bin/cfdisk /bin
	cp $CD/bin/lsblk /bin
	cp $CD/bin/mkfs.fat /bin
	cp $CD/install.txt /
	
	export CDROM=$CD
	export CDDEV=$DEV
	export PATH=$PATH:$CD/bin
	return 0
fi
