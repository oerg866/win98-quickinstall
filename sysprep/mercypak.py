'''
-------------------------------------------------------------------------------
MercyPak is a simple binary blob "packer" (not compressor!) intended 
for old computers.

Version 2.0

!!! THIS IS ALL SLOPPY AND UNSAFE, DO NOT USE IN PRODUCTION ENVIRONMENT !!!

You have been warned :P

Python Version for Windows 98 QuickInstall
(C) 2023 Eric Voirin (oerg866@googlemail.com)
-------------------------------------------------------------------------------
File Format definition

File extension: don't really care, but you can use ".866" :-)

FILE HEADER:

* ASCII File identifier                     4 Bytes ASCII

    V1: "ZIEG"
    V2: "MRCY"

* Directory count                           UINT32
* File count                                UINT32

Per directory (repeat "directory count"-times):

    * Dir attributes (hidden, sys, etc)         BYTE

    Combination of flags (taken from direct.h in OWC)

    _A_NORMAL       0x00    /* Normal file - read/write permitted */
    _A_RDONLY       0x01    /* Read-only file */
    _A_HIDDEN       0x02    /* Hidden file */
    _A_SYSTEM       0x04    /* System file */
    _A_VOLID        0x08    /* Volume-ID entry */
    _A_SUBDIR       0x10    /* Subdirectory */
    _A_ARCH         0x20    /* Archive file */

    * Directory String length                   UINT8
    * Directory String                          BYTE [ x Directory String Length ]
    String is NOT terminated.

For each file:

    V1:

        * File Name String length                   UINT8
        * File Name String                          BYTE [ x File Name String Length ]
        String is NOT terminated.

        Includes subidrectory. Example:
        "FOLDER1\FILE2.DAT"

        * File attributes                           BYTE

        (See "Dir attributes")

        * File Date                                 UINT16 packed MS-DOS System Date
        * File Time                                 UINT16 packed MS-DOS System Time

        * File size (Max 4GB, sorry)                UINT32
        * Binary blob of the actual file            BYTE * (file size)

    V2:

        * Amount of identical files following       UINT8

        For each of those files:

            * File Name String length                   UINT8
            * File Name String                          BYTE [ x File Name String Length ]
            String is NOT terminated.

            Includes subidrectory. Example:
            "FOLDER1\FILE2.DAT"

            * File attributes                           BYTE

            (See "Dir attributes")

            * File Date                                 UINT16 packed MS-DOS System Date
            * File Time                                 UINT16 packed MS-DOS System Time

        * File size (Max 4GB, sorry)                UINT32
        * Binary blob of the actual file            BYTE * (file size)

          In V2 this is only once and this one block is used for every identical file (see above)


And that's it! simplistic as hell
'''

import os
import struct
import datetime
import sys
import subprocess
import time
import hashlib

MERCYPAK_V1_MAGIC = b'ZIEG'
MERCYPAK_V2_MAGIC = b'MRCY'

FS_FAT      = 3
FS_NTFS     = 2
FS_OTHER    = 1
FS_UNK      = 0

MAX_FILES_PER_KNOWN_DATA = 8

mpak_fs_type = FS_UNK

# Get the current local time in seconds since the epoch
mpak_local_time = time.time()

# Get the UTC offset of the local time zone
mpak_utc_offset = datetime.datetime.fromtimestamp(mpak_local_time) - datetime.datetime.utcfromtimestamp(mpak_local_time)

if 'linux' in sys.platform:
    import fcntl
    import ctypes
    import xattr

def getfstype(path):
    if 'linux' in sys.platform:
        result = subprocess.run(['df', '-T', path], capture_output=True, text=True)
        if result.returncode != 0:
            raise RuntimeError(f"Failed to determine file system type for path '{path}'")
        output_lines = result.stdout.strip().split('\n')
        if len(output_lines) != 2:
            raise RuntimeError(f"Unexpected output format from 'df' command for path '{path}'")
        _, fs_string = output_lines[1].split()[1:3]
        if fs_string == 'ntfs':
            return FS_NTFS
        elif 'fat' in fs_string:
            return FS_FAT
        else:
            return FS_OTHER

def getfatattr(path):
    if sys.platform == 'win32':
        file_stat = os.stat(path)
        return file_stat.st_file_attributes & 0xff
    else:
        global mpak_fs_type
        if mpak_fs_type == FS_UNK:
            mpak_fs_type = getfstype(path)

        if mpak_fs_type == FS_FAT:
            # Use FAT32 ioctl
            fd = os.open(path, os.O_RDONLY)
            buf = ctypes.c_uint32()
            fcntl.ioctl(fd, FAT_IOCTL_GET_ATTRIBUTES, buf, True)
            os.close(fd)
            fat_attrs = buf.value
            return fat_attrs
        elif mpak_fs_type == FS_NTFS:
            # For NTFS
            attr = 0xff & xattr.get(path, "system.ntfs_attrib")
            # The lower 8 bits seem to be compatible to the regular DOS attributes but
            # they do not seem to contain directory flag so we add it manually
            if os.path.isdir(path):                 # check directory attribute
                attr |= 0x10            
            return attr
        else:
            # Fallback
            attr = 0x00
            if os.path.isdir(path):                 # check directory attribute
                attr |= 0x10  
            if not os.access(path, os.W_OK):        # check read-only attribute
                attr |= 0x01
            if os.path.isfile(path) and os.stat(path).st_mode & 0o444: 
                                                    # set archive attribute
                                                    # (according to chatgpt, i don't think this is true)
                attr |= 0x20  
            return attr

class fileInfo:
    def __init__(self, filename: str, attribute: int, dos_date: int, dos_time: int):
        self.filename = filename
        self.attribute = attribute
        self.dos_date = dos_date
        self.dos_time = dos_time


class fileData:
    def __init__(self, data: bytearray):
        self.data = data
        self.hash = hashlib.sha256()
        self.hash.update(self.data)
        self.files_with_this_data = list[fileInfo]()
    
    def add_file(self, filename: str, attribute, dos_date, dos_time):
        self.files_with_this_data.append(fileInfo(filename, attribute, dos_date, dos_time))

def add_to_known_files(file_data_list: list[fileData], data:bytearray, filename, attribute, dos_date, dos_time):
            
    hash = hashlib.sha256()
    hash.update(data)
    
    for file_data in file_data_list:
        if file_data.hash.digest() == hash.digest() and len(file_data.files_with_this_data) < MAX_FILES_PER_KNOWN_DATA:
            print(f'file {filename} is duplicate, optimizing...')
            file_data.add_file(filename, attribute, dos_date, dos_time)
            return

    # We don't know any files with this data block yet, so we add a new one
    new_file_data = fileData(data)
    new_file_data.add_file(filename, attribute, dos_date, dos_time)
    file_data_list.append(new_file_data)


def mercypak_pack(dir_path, output_file, mercypak_v2=False):
    # Collect directory and file information
    dir_count = 0
    file_count = 0
    dir_info = []
#    file_info = []
    dir_path = os.path.abspath(dir_path)
    known_file_infos = list[fileData]()

    for root, dirs, files in os.walk(dir_path):
        for dir_name in dirs:
            dir_count += 1
            dir_abs_path = os.path.join(root, dir_name)
            dir_rel_path = os.path.relpath(dir_abs_path, dir_path)
            dir_dos_attr = getfatattr(dir_abs_path)
            dir_info.append((dir_rel_path.encode(), dir_dos_attr))
        for file_name in files:
            file_count += 1
            file_abs_path = os.path.join(root, file_name)
            file_rel_path = os.path.relpath(file_abs_path, dir_path)
            file_stat = os.stat(file_abs_path)
            file_dos_date = dos_date(file_stat.st_mtime)
            file_dos_time = dos_time(file_stat.st_mtime)
            file_dos_attr = getfatattr(file_abs_path)
            with open(file_abs_path, 'rb') as f:
                file_data = f.read()

            add_to_known_files(known_file_infos, file_data, file_rel_path.encode(), file_dos_attr, file_dos_date, file_dos_time)
 #           file_info.append((file_rel_path.encode(), file_dos_attr, file_dos_date, file_dos_time, len(file_data), file_data))

    print(f'known unique files: {len(known_file_infos)}, total files {file_count}')

    # Write the archive
    with open(output_file, 'wb') as f:
        # Write file header
        if mercypak_v2:
            f.write(MERCYPAK_V2_MAGIC)
        else:
            f.write(MERCYPAK_V1_MAGIC)

        f.write(struct.pack('<II', dir_count, file_count))

        # Write directory information
        for dir in dir_info:
            dir_rel_path, dir_mode = dir
            f.write(struct.pack('B', dir_mode & 0xff))
            f.write(struct.pack('B', len(dir_rel_path)))
            f.write(dir_rel_path)

        # Write file information
        for file_data in known_file_infos:
            file_size = len(file_data.data)

            if file_size > 0xffffffff:
                raise ValueError(f'File is too big.')

            files_with_this_data_count = len(file_data.files_with_this_data)

            if files_with_this_data_count > 0xff:
                raise ValueError(f'Too many identical files. Something is wrong with the script')

            if mercypak_v2:

                # MERCYPAK V2: Write redundant files only once. 

                f.write(struct.pack('B', files_with_this_data_count))

                for file_info in file_data.files_with_this_data:
                    file_rel_path = file_info.filename

                    if len(file_rel_path) > 0xff:
                        raise ValueError(f'File path "{file_rel_path}" is too long (max. 255 characters)')

                    f.write(struct.pack('B', len(file_rel_path) & 0xff))
                    f.write(file_rel_path)
                    f.write(struct.pack('B', file_info.attribute & 0xff))
                    f.write(struct.pack('<HH', file_info.dos_date, file_info.dos_time))

                f.write(struct.pack('<I', file_size))
                f.write(file_data.data)
            
            else:

                # MERCYPAK V1: Write every file individually, even if it is redundant.

                for file_info in file_data.files_with_this_data:

                    if len(file_rel_path) > 0xff:
                        raise ValueError(f'File path "{file_rel_path}" is too long (max. 255 characters)')

                    f.write(struct.pack('B', len(file_rel_path) & 0xff))
                    f.write(file_rel_path)
                    f.write(struct.pack('B', file_info.attribute & 0xff))
                    f.write(struct.pack('<HH', file_info.dos_date, file_info.dos_time))
                    f.write(struct.pack('<I', file_size))
                    f.write(file_data.data)



def dos_date(mtime):
    timestamp = datetime.datetime.utcfromtimestamp(mtime) + mpak_utc_offset
    return ((timestamp.year - 1980) << 9) | (timestamp.month << 5) | timestamp.day

def dos_time(mtime):
    timestamp = datetime.datetime.utcfromtimestamp(mtime) + mpak_utc_offset
    return (timestamp.hour << 11) | (timestamp.minute << 5) | (timestamp.second // 2)
