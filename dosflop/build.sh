#!/bin/bash

echo Building DOS-based boot floppy...
dd if=/dev/zero of=dosflop.img bs=1440k count=1
mkfs.fat -F 12 dosflop.img
dd if=boot/fdboot.bin of=dosflop.img bs=512 count=1 conv=notrunc
pushd boot
mcopy -bsv -i "../dosflop.img" * ::
popd

