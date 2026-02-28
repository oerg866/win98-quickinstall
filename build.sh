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

set -e 1

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
	mkdir -pv {dev,proc,etc/init.d,sys,tmp,usr/lib/terminfo/l,lib/firmware,usr/local/share}
	sudo mknod dev/console c 5 1
	sudo mknod dev/null c 1 3
	cp ../supplement/welcome ./
	cp ../supplement/inittab ./etc/inittab
	cp ../supplement/rc ./etc/init.d/rc
	cp ../supplement/findcd.sh ./
	cp ../supplement/setenv.sh ./
	cp "$PREFIX/share/terminfo/l/linux" ./usr/lib/terminfo/l/linux
	cp -r ../supplement/firmware/* ./lib/firmware
	ln -s sbin/init init
	ln -s /install/bin/pci.ids ./usr/local/share/pci.ids
	chmod +x etc/init.d/rc
	chmod +x ./findcd.sh
	sudo chown -R root:root .
	find . | cpio -H newc -o | xz --check=crc32 > ../rootfs.cpio.xz
	find . | cpio -H newc -o > ../rootfs.cpio
popd

pushd linux
	rm -f .config && cp ../buildscripts/linux_config.flp .config && make -j8 bzImage
   	cp arch/x86/boot/bzImage ../bzImage.flp
	rm -f .config && cp ../buildscripts/linux_config.cd .config && make -j8 bzImage
	cp arch/x86/boot/bzImage ../bzImage.cd
	rm -f .config && cp ../buildscripts/linux_config.efi .config && make -j8 bzImage
   	cp arch/x86/boot/bzImage ../bzImage.efi
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
	./build.sh ../bzImage.flp floppy.img 1474560	# Floppy disk boot
	./build.sh ../bzImage.cd cdrom.img 2949120		# CD-ROM / 2.88M boot
popd

pushd installer
	./build.sh
popd

pushd dosflop
	./build.sh
popd

pushd pciutils
	./build.sh
popd

# Copy linux kernel & binaries
mkdir -p "$CDROOT/bin"
cp bzImage.cd "$CDROOT/"
cp tiny-floppy-bootloader/cdrom.img "$CDROOT/"

# Copy some essentials
cp util-linux/OUTPUT/bin/hexdump "$CDROOT/bin/"
cp util-linux/OUTPUT/bin/irqtop "$CDROOT/bin/"
cp util-linux/OUTPUT/bin/isosize "$CDROOT/bin/"
cp util-linux/OUTPUT/bin/lscpu "$CDROOT/bin/"
cp util-linux/OUTPUT/bin/lsirq "$CDROOT/bin/"
cp util-linux/OUTPUT/bin/lsmem "$CDROOT/bin/"
cp util-linux/OUTPUT/bin/whereis "$CDROOT/bin/"

cp util-linux/cfdisk "$CDROOT/bin/"
cp util-linux/lsblk "$CDROOT/bin/"
cp pciutils/lspci "$CDROOT/bin/"
cp pciutils/setpci "$CDROOT/bin/"
cp pciutils/pci.ids "$CDROOT/bin/"
cp dosfstools/OUTPUT/sbin/* "$CDROOT/bin/"
cp supplement/witchery "$CDROOT/bin"
cp supplement/syslinux.cfg "$CDROOT"

# Boot floppies, also copy them to the CDROM root for ~user convenience~
cp tiny-floppy-bootloader/floppy.img "$OUTPUT/"
cp dosflop/dosflop.img "$OUTPUT/"

# Our installer!
cp supplement/install.txt "$CDROOT/"
cp supplement/uefi/*.efi "$CDROOT/"
cp installer/lunmercy "$CDROOT/bin/"

# DOS installation tools (to start the kernel from DOS)
cp supplement/dosinst/* "$CDROOT/"

# Copy sysprep tools
mkdir -p "$OUTPUT/tools"
mkdir -p "$OUTPUT/mercypak"

# SYSLINUX: Extract mbr and bootsector, as well as ldlinux loader files
cp syslinux/bios/mbr/mbr.bin "$OUTPUT/tools/syslinux_mbr.bin"
objcopy -O binary -j .data syslinux/bios/linux/bootsect_bin.o "$OUTPUT/tools/syslinux_bs.bin"
cp syslinux/bios/com32/elflink/ldlinux/ldlinux.c32 "$OUTPUT/tools"
cp syslinux/bios/com32/menu/menu.c32 "$OUTPUT/tools"
cp syslinux/bios/com32/libutil/libutil.c32 "$OUTPUT/tools"
cp syslinux/bios/core/ldlinux.sys "$OUTPUT/tools"

# Copy EFI kernel into tools folder also
cp bzImage.efi "$OUTPUT/tools"

mkdir -p "$OUTPUT/_DRIVER_"
mkdir -p "$OUTPUT/_EXTRA_DRIVER_"

cp -r sysprep/* "$OUTPUT"
cp -r win98-driver-lib-base/* "$OUTPUT/_DRIVER_"
cp -r win98-driver-lib-extra/* "$OUTPUT/_EXTRA_DRIVER_"
cp README.md "$OUTPUT"

pushd "$OUTPUT"
	zip -r "$BASE/Windows98QuickInstall_$(date +%Y%m%d_%H%M).zip" ./
popd

echo "Done. Output is in $OUTPUT."
echo "Run sysprep.py to build an image."
