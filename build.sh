#!/bin/bash

BASE=$PWD/
PREFIX=$PWD/i486-linux-musl-cross/i486-linux-musl
OUTPUT=$PWD/__BIN__
CDROOT=$OUTPUT/cdromroot

sudo rm -rf ./filesystem
sudo rm -rf $OUTPUT
sudo rm -rf mnt
mkdir -p mnt
mkdir -p ./filesystem


# Build busybox

pushd busybox
	./build.sh
	mv _install/* ../filesystem
	# Hack for reboot.. only works with -f attached because $reasons ...
	rm ../filesystem/sbin/reboot
	echo "#!/bin/sh" >../filesystem/sbin/reboot
	echo "busybox reboot -f" >>../filesystem/sbin/reboot
	chmod +x ../filesystem/sbin/reboot
popd

# Prepare initrd

pushd filesystem
	mkdir -pv {dev,proc,etc/init.d,sys,tmp,usr/lib/terminfo/l}
	sudo mknod dev/console c 5 1
	sudo mknod dev/null c 1 3
	cp ../supplement/welcome ./
	cp ../supplement/inittab ./etc/inittab
	cp ../supplement/rc ./etc/init.d/rc
	cp ../supplement/findcd.sh ./
	cp ../supplement/setenv.sh ./
	cp $PREFIX/share/terminfo/l/linux ./usr/lib/terminfo/l/linux
	ln -s sbin/init init
	chmod +x etc/init.d/rc
	chmod +x ./findcd.sh
	sudo chown -R root:root .
	find . | cpio -H newc -o | xz --check=crc32 > ../rootfs.cpio.xz
	find . | cpio -H newc -o > ../rootfs.cpio
popd

pushd linux
	make -j8 bzImage
    cp arch/x86/boot/bzImage ../
popd

pushd dosfstools
	./build.sh
popd

pushd syslinux
	./build.sh
popd

pushd util-linux
	./build.sh
popd

pushd tiny-floppy-bootloader
	./build.sh
popd

pushd installer
	./build.sh
popd

pushd mercypak
	./build.sh
popd

# Ran out of space on the boot floppy, so we only can use the tiny floppy bootloader now =(
#rm floppy.img
#dd if=/dev/zero of=floppy.img bs=1k count=1440
#mkdosfs -F 12 floppy.img
#sudo syslinux/bios/linux/syslinux --stupid --install floppy.img
#mkdir -p ./mnt
#sudo mount -o loop floppy.img ./mnt
#sudo cp ./syslinux/bios/com32/elflink/ldlinux/ldlinux.c32 ./mnt
#sudo rm ./mnt/*.sys
#sudo cp bzImage ./mnt
#sudo cp rootfs.cpio.xz ./mnt
#sudo cp supplement/syslinux.cfg ./mnt
#sudo chmod +x ./mnt/syslinux.cfg
#sudo umount ./mnt

# Copy linux kernel & binaries
mkdir -p $CDROOT/bin
cp bzImage $CDROOT/
cp tiny-floppy-bootloader/disk.img $CDROOT/
cp util-linux/OUTPUT/bin/* $CDROOT/bin/
cp util-linux/OUTPUT/sbin/* $CDROOT/bin/
cp util-linux/cfdisk $CDROOT/bin/
cp util-linux/lsblk $CDROOT/bin/
cp util-linux/mount $CDROOT/bin/mount2 #busybox provides mount, this is just for more compatibility
cp dosfstools/OUTPUT/sbin/* $CDROOT/bin/
cp dialog/dialog $CDROOT/bin/
cp supplement/get* $CDROOT/bin/

# Copy sysprep tools & base drivers
mkdir -p $OUTPUT/tools
mkdir -p $OUTPUT/mercypak
cp -r tools $OUTPUT
cp -r mercypak/mercypak $OUTPUT/mercypak
cp -r mercypak/*.exe $OUTPUT/mercypak
cp -r sysprep/* $OUTPUT
cp -r win98-driver-lib-base/* $OUTPUT/_DRIVER_


pushd $OUTPUT
	zip -r "$BASE/Windows98QuickInstall_$(date +%Y%m%d_%H%M).zip" ./
popd

echo "Done. Output is in $OUTPUT."
echo "Run sysprep.sh (Linux) or sysprep.bat (Windows) to build an ISO image."