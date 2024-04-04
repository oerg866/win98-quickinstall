import os
import shutil
import subprocess
import platform
import fs
from fs.copy import copy_dir    
import struct
from concurrent.futures import ThreadPoolExecutor
import syslinux

# makeusb for Windows 98 QuickInstall.
# Python version.
# (C) 2023 Eric Voirin (oerg866@googlemail.com)

CHS_GEOMETRY_CYLINDERS = 1024
CHS_GEOMETRY_HEADS = 256
CHS_GEOMETRY_SECTORS = 63

PARTITIONENTRY_TYPE_FAT32 = 0x0C
PARTITIONENTRY_SECTOR_SIZE = 512
PARTITIONENTRY_SECTORS_PER_CLUSTER = 8
PARTITIONENTRY_FAT32_MIN_SIZE = 512 * 1024 * 1024 # Min 512MiB

class MasterBootRecord:
    """
    This class models a Master Boot Record.
    """

    class PartitionEntry:
        """
        This class models a partition table entry in a MBR.
        """

        class CHS:
            """ 
            This class models a Cylinder/Head/Sector address.
            """
            def __init__(self, lba_address=0):
                self.cylinder = 0
                self.head = 0
                self.sector = 0
                self.from_lba(lba_address)

            def to_bytes(self):
                b1 = self.head & 0xff                                   # h7-0
                b2 = (self.cylinder >> 2) & 0xC0 | self.sector & 0x3F   # c9-8 | s5-0
                b3 = self.cylinder & 0xff                               # c7-0
                return bytearray([b1, b2, b3])
            
            def to_lba(self):
                """
                Convert a given CHS address to a corresponding LBA address, assuming a fixed geometry of
                1024 cylinders, 256 heads, and 63 sectors per track.
                """
                lba = (self.cylinder * CHS_GEOMETRY_HEADS + self.head) * CHS_GEOMETRY_SECTORS + self.sector - 1

            def from_lba(self, lba_sector):
                """
                Convert a given LBA address to a corresponding CHS address, assuming a fixed geometry of
                1024 cylinders, 256 heads, and 63 sectors per track.
                """
                if lba_sector == 0: # (impossible lol)
                    return
                
                self.cylinder = lba_sector // (CHS_GEOMETRY_HEADS * CHS_GEOMETRY_SECTORS)
                temp = lba_sector % (CHS_GEOMETRY_HEADS * CHS_GEOMETRY_SECTORS)
                self.head = temp // CHS_GEOMETRY_SECTORS
                self.sector = temp % CHS_GEOMETRY_SECTORS + 1

        def __init__(self):
            self.status = 0
            self.chs_start = MasterBootRecord.PartitionEntry.CHS()
            self.type = 0
            self.chs_end = MasterBootRecord.PartitionEntry.CHS()
            self.lba_start = 0
            self.num_sectors = 0

        def to_bytes(self):
            """
            Gives the packed bytes for this partition table entry.
            """
            return struct.pack('=B3sB3sII', self.status, self.chs_start.to_bytes(), self.type, self.chs_end.to_bytes(), self.lba_start, self.num_sectors)
        
        def set(self, partition_start_sectors, partition_size_sectors, partition_type, active: bool):
            if active:
                self.status = 0x80
            else:
                self.status = 0
            
            self.lba_start = partition_start_sectors
            self.chs_start = self.CHS(partition_start_sectors)
            self.chs_end = self.CHS(partition_start_sectors + partition_size_sectors)
            self.num_sectors = partition_size_sectors
            self.type = partition_type

    def __init__(self, disk_size, mbr_code_filename=None):
        self.boot_code = bytearray(446)
        self.partitions = [MasterBootRecord.PartitionEntry() for _ in range(4)]
        self.partition_count = 0
        self.signature = b'\x55\xAA'
        self.disk_size = disk_size

        if mbr_code_filename is not None:
            self.set_mbr_code_from_file(mbr_code_filename)

    def add_partition(self, partition_size_bytes, partition_offset_sectors, partition_type=PARTITIONENTRY_TYPE_FAT32,active=True):
        """
        Add a partition to this MBR.
        """
        print(f'MBR: Adding partition of size {partition_size_bytes} at sector {partition_offset_sectors}')

        partition_size_sectors = (partition_size_bytes + 1) // PARTITIONENTRY_SECTOR_SIZE

        # Find earliest first sector
        if self.partition_count == 4:
            raise ValueError('Partition count is too high')
        
        if (partition_offset_sectors * PARTITIONENTRY_SECTOR_SIZE + partition_size_bytes) > self.disk_size:
            raise ValueError('Partition would go past disk size.')

        partition_start_sectors = partition_offset_sectors
        partition_end_sectors = partition_start_sectors + partition_size_sectors
        
        if self.partition_count > 0:
            # Check if any part of the partition would overlap
            
            for part in self.partitions:
                if part.lba_start == 0 and part.num_sectors == 0:
                    # blank partition, end of partition table
                    break

                this_start = part.lba_start
                this_end = part.lba_start + part.num_sectors

                if partition_start_sectors <= this_start < partition_end_sectors or this_start <= partition_start_sectors < this_end:
                    raise ValueError('Overlapping partitions!')

        self.partitions[self.partition_count].set(partition_start_sectors, partition_size_sectors, partition_type, active)
        
        self.partition_count += 1
        
        return self.partition_count - 1

    def set_mbr_code_from_file(self, filename):
        """
        Reads MBR code from a file and writes it to this MBR.
        """

        with open(filename, 'rb') as f:
            mbr_code = f.read(446)

        # Make sure it's <= 512 bytes
        if len(mbr_code) > len(self.boot_code):
            raise ValueError('MBR code too big!')

        # Only use as many bytes as we need        
        self.boot_code[:min(len(mbr_code), len(self.boot_code))] = mbr_code

    def to_bytes(self):
        partition_bytes = b''.join(partition.to_bytes() for partition in self.partitions)
        return self.boot_code + partition_bytes + self.signature

class FAT32Partition:
    """
    This class models some parts integral to a FAT32 partition.
    """

    class BootSector:
        """
        This class models a boot sector for a partition
        """

        class BiosParameterBlock:
            """
            This class models a bios parameter block in the boot sector of a partition.
            """

            def __init__(self, partition_table_entry: MasterBootRecord.PartitionEntry):
                """
                Initialize the BPB, we need the partition table entry from the Master Boot Record for this (eugh not pretty SE-wise)
                """

                # DOS 2.0 stuff
                
                self.bytes_per_sector = PARTITIONENTRY_SECTOR_SIZE
                self.logical_sectors_per_cluster = PARTITIONENTRY_SECTORS_PER_CLUSTER
                self.reserved_logical_sectors = 32
                self.number_of_fats = 2
                self.root_dir_entries = 0
                self.total_logical_sectors = 0
                self.media_descriptor = 0xf8
                self.logical_sectors_per_fat = 0x00 #?

                # DOS 3.31 stuff

                self.physical_sectors_per_track = CHS_GEOMETRY_SECTORS
                self.number_of_heads = CHS_GEOMETRY_HEADS - 1
                self.hidden_sectors = partition_table_entry.lba_start
                self.large_total_logical_sectors = partition_table_entry.num_sectors

                # DOS 7.10 stuff

                self.large_logical_sectors_per_fat = 16363 # I see this value in Ranish partition manager formatted partition. 32 doesn't work, there's no docs on this...
                self.fat_flags = 0  # Not used here
                self.version_number = 0x0000
                self.root_directory_cluster = 2 # No idea why but win98 does this
                self.fs_information_sector = 1
                self.backup_sector = 6
                self.boot_file_name = bytearray(12) # blank, unused
                self.physical_drive_number = 0x80
                self.flags = 0x00
                self.extended_boot_signature = 0x29
                self.volume_serial_number = 0x08660866
                self.volume_label = b'QUICKINST  '
                self.fs_type = b'FAT32   '


            def to_bytes(self):
                b = struct.pack('=HBHBHHBHHHII',        self.bytes_per_sector,
                                                        self.logical_sectors_per_cluster,
                                                        self.reserved_logical_sectors,
                                                        self.number_of_fats,
                                                        self.root_dir_entries,
                                                        self.total_logical_sectors,
                                                        self.media_descriptor,
                                                        self.logical_sectors_per_fat,
                                                        self.physical_sectors_per_track,
                                                        self.number_of_heads,
                                                        self.hidden_sectors,
                                                        self.large_total_logical_sectors)
                b += struct.pack('=IHHIHH12sBBBI11s8s', self.large_logical_sectors_per_fat,
                                                        self.fat_flags,
                                                        self.version_number,
                                                        self.root_directory_cluster,
                                                        self.fs_information_sector,
                                                        self.backup_sector,
                                                        self.boot_file_name,
                                                        self.physical_drive_number,
                                                        self.flags,
                                                        self.extended_boot_signature, 
                                                        self.volume_serial_number,
                                                        self.volume_label,
                                                        self.fs_type)
                return b

        # BootSector stuff

        def __init__(self, partition: MasterBootRecord.PartitionEntry):
            self.jumpopcode = b'\xEB\x58\x90'
            self.oemname = b'OERG866 '
            self.bpb = self.BiosParameterBlock(partition)
            self.bootcode = bytearray(0x1A4)
            self.signature = b'\x55\xAA'
        
        def to_bytes(self):
            return self.jumpopcode + self.oemname + self.bpb.to_bytes() + self.bootcode + self.signature
  
    class FSInfoSector:
        def __init__(self):
            self.fs_info_sig_start = b'RRaA'
            self.reserved = bytearray(480)
            self.fs_info_sig_end = b'rrAa'
            self.last_free_data_clusters = 0xffffffff
            self.last_allocated_cluster = 0xffffffff
            self.reserved2 = bytearray(12)
            self.signature = b'\x00\x00\x55\xAA'

        def to_bytes(self):
            return struct.pack('=4s480s4sII12s4s',  self.fs_info_sig_start,
                                                    self.reserved,
                                                    self.fs_info_sig_end,
                                                    self.last_free_data_clusters,
                                                    self.last_allocated_cluster,
                                                    self.reserved2,
                                                    self.signature)

    class FAT:  # Creates a blank fat32 for now. this is hard :(
        def __init__(self):
            self.FAT_ID = 0x0FFFFFF8 # fixed disk
            self.end_of_chain = 0x0FFFFFFF # end of chain
            self.root_directory_cluster = 0x0FFFFFFF # Cluster for root directory (we have none)

        def to_bytes(self):
            return struct.pack('III', self.FAT_ID, self.end_of_chain, self.root_directory_cluster)

    def __init__(self, partition: MasterBootRecord.PartitionEntry):
        self.bootsector = self.BootSector(partition)
        self.fsinfosector = self.FSInfoSector()
        self.fat = self.FAT()
        self.partition_entry = partition

    def bootsector_bytes(self):
        return self.bootsector.to_bytes()
    
    def fsinfosector_bytes(self):
        return self.fsinfosector.to_bytes()
    
    def fat_bytes(self):
        b = self.fat.to_bytes()
        ret = bytearray(512)
        ret[:len(b)] = b
        return ret
    
    def write_block(self, file, data: bytearray, offset=0, count=0):
        if count == 0:
            count = len(data)
        file.seek(offset, 0)
        file.write(data[:count])

    def write_to_file(self, filename):
        bps = self.bootsector.bpb.bytes_per_sector
        bootsector_start = self.partition_entry.lba_start * bps
        backup_offset = self.bootsector.bpb.backup_sector * bps
        fsinfosector_start = bootsector_start + bps

        fat_1_start = bootsector_start + self.bootsector.bpb.reserved_logical_sectors * bps
        fat_2_start = fat_1_start + self.bootsector.bpb.large_logical_sectors_per_fat * bps

        # Write boot sector
        with open(filename, 'r+b') as f:
            # BOOT & FS info sector
            self.write_block(f, self.bootsector.to_bytes(), bootsector_start)
            self.write_block(f, self.fsinfosector.to_bytes(), fsinfosector_start)
            
            # Backupsectors
            self.write_block(f, self.bootsector.to_bytes(), bootsector_start + backup_offset)
            self.write_block(f, self.fsinfosector.to_bytes(), fsinfosector_start + backup_offset)

            # FAT stuff
            self.write_block(f, self.fat.to_bytes(), fat_1_start)
            self.write_block(f, self.fat.to_bytes(), fat_2_start)

class HardDiskImage:
    def __init__(self, disk_size, filename, mbr_code_filename):
        self.filename = filename

        # Pad disk size to sector boundary
        disk_size = (disk_size + 1) // PARTITIONENTRY_SECTOR_SIZE * PARTITIONENTRY_SECTOR_SIZE

        # FAT32 partitions must be 512MB, add 2MB for some offset and stuff to be sure...      
        min_disk_size = PARTITIONENTRY_FAT32_MIN_SIZE + 2 * 1024 * 1024

        if (disk_size < min_disk_size):
            disk_size = min_disk_size
            last_offset = (disk_size - PARTITIONENTRY_FAT32_MIN_SIZE) // PARTITIONENTRY_SECTOR_SIZE
            print('WARNING: FAT32 partitions must be at least 512MiB, increasing disk size.')
            print(f'WARNING: Maximum partition offset for a valid FAT32 partition is sector {last_offset}')

        # Create blank disk and master boot record.

        self.create_image_file(disk_size)
        self.mbr = MasterBootRecord(disk_size, mbr_code_filename=mbr_code_filename)
        self.write_mbr()

    def create_image_file(self, size_bytes):
        create_empty_file(self.filename, size_bytes)

    def write_mbr(self):
        with open(self.filename, 'rb+') as f:
            f.seek(0, 0)
            f.write(self.mbr.to_bytes())
    
    def format_partition(self, index):
        partition_entry = self.mbr.partitions[index]
        if partition_entry.type != PARTITIONENTRY_TYPE_FAT32:
            raise ValueError('Unsupported partition type')

        print('Formatting partition...')

        partition_data = FAT32Partition(partition_entry)
        partition_data.write_to_file(self.filename)

    def add_partition(self, partition_size_bytes, partition_offset_sectors, partition_type=PARTITIONENTRY_TYPE_FAT32, active=True):
        if (partition_size_bytes < PARTITIONENTRY_FAT32_MIN_SIZE):
            print('WARNING: FAT32 partitions must be at least 512MiB, increasing partition size.')
            partition_size_bytes = PARTITIONENTRY_FAT32_MIN_SIZE
        
        new_partition_index = self.mbr.add_partition(partition_size_bytes, partition_offset_sectors, partition_type, active)
        self.write_mbr()
        self.format_partition(new_partition_index)
        return

def dd (sin: str, sout: str, count=0, inoffset=0, outoffset=0):
    infile = open(sin, 'rb')
    outfile = open(sout, 'r+b')
    infile.seek(0, 2)
    infilesize = infile.tell()
    
    infile.seek(inoffset, 0)
    outfile.seek(outoffset, 0)
    
    if count == 0:
        count = infilesize

    block = infile.read(count)

    outfile.write(block)

    infile.close()
    outfile.close()
    return

def create_empty_file (filename, size):
    with open(filename, 'wb') as f:
        f.seek(size - 1)
        f.write(b'\0')

def delete_if_present(file):
    if os.path.exists(file):
        os.remove(file)

# Gets the directory size. Files are padded to block_size byte boundaries.
def get_directory_size(path, block_size):
    total_size = 0
    for dirpath, dirnames, filenames in os.walk(path):
        for f in filenames:
            fp = os.path.join(dirpath, f)
            total_size += (( os.path.getsize(fp) + block_size - 1 ) // block_size) * block_size # padding
    return total_size

def copy_tree_to_fs_path(input_path, pyfilesystem_path):
    filesystem = fs.open_fs(pyfilesystem_path, writeable=True)

    progress = ''

    for dirpath, dirnames, filenames in os.walk(input_path):
        # Create all folders
        for dir in dirnames:
            source_dir = os.path.join(dirpath, dir)
            target_dir = os.path.relpath(source_dir, input_path)
            target_dir = target_dir.replace(os.path.sep, '/')
            filesystem.makedir(target_dir, recreate=True)
    
        # Create and copy all files
        for file in filenames:
            source_file = os.path.join(dirpath, file)
            target_file = os.path.relpath(source_file, input_path)
            target_file = target_file.replace(os.path.sep, '/')
    
            if not os.path.isfile(source_file):
                continue

            # Cursed way to display the progress. Sorry, it's late and I wanna finish this!

            print('\r', end='')
            print(' ' * len(progress), end='')
            progress_file = target_file
            terminal_width = os.get_terminal_size().columns
            if len(progress_file) > terminal_width - 8:
                progress_file = '...' + progress_file[len(progress_file) - terminal_width + 8 + 3:]

            progress = f'\r => {progress_file}'
            print(progress, end='')

            with open(source_file, "rb") as f:
                filesystem.writebytes(target_file, f.read())

    print('\nDone')
    filesystem.close()

def make_usb(output_base, output_usb):
    bps = PARTITIONENTRY_SECTOR_SIZE
    padding_block_size = PARTITIONENTRY_SECTORS_PER_CLUSTER * bps

    print (f'Block size {padding_block_size}')

    totalsize = get_directory_size(output_base, padding_block_size) 
    partitionsize = (( totalsize + padding_block_size - 1 ) // padding_block_size) * padding_block_size # Pad to cluster boundary
    partitionsize = partitionsize + 32 * 1024 * 1024  # Extra padding just to make sure... (I have no clue how to calculate this properly.)
    disksize = partitionsize + 2 * 1024 * 1024 # 2MB padding for the disk
    partoffset = 63 # 63 sector offset, as usual
    basedir = os.path.join(os.curdir)
    syslinux_mbr = os.path.join(basedir, 'tools', 'syslinux_mbr.bin')

    print('Creating USB image file...')

    print(f'Total size: {totalsize} bytes')

    delete_if_present(output_usb)

    hdd_img = HardDiskImage(disksize, output_usb, syslinux_mbr)
    hdd_img.add_partition(partitionsize, partition_offset_sectors=partoffset)

    # Install syslinux bootloader

    syslinux.install_syslinux(output_usb, partoffset, bps)

    protocol_file = f'fat://{fs.path.normpath(os.path.abspath(output_usb))}?offset={partoffset * bps}'

    # Copy all the files to the new file system

    print ('Copying files to image. WARNING: THIS IS VERY SLOW (I DO NOT KNOW WHY), BE PATIENT')
    copy_tree_to_fs_path(output_base, protocol_file)
 