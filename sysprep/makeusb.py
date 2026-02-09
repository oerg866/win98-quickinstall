import os
import syslinux

# makeusb for Windows 98 QuickInstall.
# Python version.
# (C) 2023 Eric Voirin (oerg866@googlemail.com)

# Gets the directory size. Files are padded to block_size byte boundaries.
def getDirectorySizeAligned(path, blockSize):
    total_size = 0
    for dirpath, dirnames, filenames in os.walk(path):
        for f in filenames:
            fp = os.path.join(dirpath, f)
            total_size += (( os.path.getsize(fp) + blockSize - 1 ) // blockSize) * blockSize # padding
    return total_size

from FATtools import Volume, FAT, partutils, mkfat

def makeUsb(baseDir, outputUsb):

    totalSize = getDirectorySizeAligned(baseDir, 4096) # 4K alignment
    totalSize += (32 * 1024 * 1024) # something extra just to make sure

    if os.path.exists(outputUsb):
        os.remove(outputUsb)

    # Create blank image
    with open(outputUsb, 'wb') as f:
        f.seek(totalSize - 1)
        f.write(b'\0')

    # Partition it
    disk = Volume.vopen(outputUsb, 'r+b', 'disk')
    partutils.partition(disk, 'mbr')
    Volume.vclose(disk)

    # Format it
    part = Volume.vopen(outputUsb, 'r+b', 'partition0')
    mkfat.fat_mkfs(part, part.size, params = {'fat_bits': 32})
    Volume.vclose(part)

    # Now install Syslinux to the freshly baked file system
    syslinux.install_syslinux(outputUsb)

    # Re-open the volume and open the file system
    part = Volume.vopen(outputUsb, 'r+b', 'partition0')
    fs = Volume.openvolume(part)

    # Copy all the files
    print('Copying files into USB image (this may take a while...)')
    Volume.copy_tree_in(baseDir, fs)

    # Copy EFI stuff
    efiDir = fs.mkdir('EFI')
    bootDir = efiDir.mkdir('BOOT')

    with open(os.path.join('tools', 'bzImage.efi'), 'rb') as f:
        bootia32 = bootDir.create('BOOTIA32.EFI')
        bootia32.write(f.read())
        bootia32.close()

    Volume.vclose(part)

