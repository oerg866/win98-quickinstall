# SYSLINUX Installer
# Python Port
# (C) 2023 Eric Voirin

import os
import mmap
import fs
import struct

# OK Realtalk(TM)

# There's no proper installer for Windows, for no apparent reason. It can only operate on mounted drives, not on files.
# So I'm trying to replicate its functionality in python. The problem with this is that it's absolutely CURSED.
# Instead of parsing the filesystem's root directory, it patches the boot sector and a file with the offsets and whatnot.
# This is batshit insane, I'm very sorry to say. There may or may not be a very good reason to do this? I'm not seeing it.
# In any case I wrote this working python installer for it and I still don't really understand how or why it works, so
# good luck figuring it out...

# Excuse the harsh language in this file, but this was really *really* f*cking frustrating.

def get_file_size(file):
    # get file size in bytes
    file_size = os.stat(file.fileno()).st_size
    return file_size

class SectorExtent:
    def __init__(self, lba, length):
        self.lba = lba
        self.length = length

def patch_ldlinux_sys_extents(ldlinux_bytes: bytearray, ldlinux_relative_sector, ldlinux_sys_size_including_adv, extent_offset, extent_count, bytes_per_sector=512):

    ldlinux_sys_size_without_adv_and_bootsector = ldlinux_sys_size_including_adv - 3 * bytes_per_sector # Without boot sector and without ADV sectors so minus 3
    
    total_sectors = (ldlinux_sys_size_without_adv_and_bootsector // 512)

    all_sectors = list()
    all_extents = list()

    # Populate a list of all sectors

    for i in range(total_sectors):
        all_sectors.append(ldlinux_relative_sector + i + 1) # +1 because we skip the boot sector.

    # This is somewhat ported from the original C code. It's horrible to read and i have no idea what it does (thank god, else i'd be a sicko)

    addr = 0x8000 # ldlinux.sys starts loading here
    base = addr
    
    current_lba = 0
    current_len = 0

    current_sector_index = 0
    
    while total_sectors:
        current_sector = all_sectors[current_sector_index]
        
        if current_len > 0:
            xbytes = (current_len + 1) * bytes_per_sector
            
            if current_sector == current_lba + current_len and xbytes < 65536 and (addr ^ (base + xbytes - 1)) & 0xffff0000 == 0: # what the fucking shit
                # So if this condition is true then we can add to the extent instead of making a new one. OK let's do that.
                current_len += 1
                current_sector_index += 1
                total_sectors -= 1
                addr += bytes_per_sector
                continue

            # OK for some reason the above condition is not true so we need to finish this extent!            
            all_extents.append(SectorExtent(current_lba, current_len))
        
        base = addr
        current_lba = current_sector
        current_len = 1
        
        current_sector_index += 1
        total_sectors -= 1
        addr += bytes_per_sector
    
    # We reached the end, so if there's a pending extent write it out now

    if current_len > 0:
        all_extents.append(SectorExtent(current_lba, current_len))

    #print(f'Extent count: {len(all_extents)}')
    
    # Write the extents at the extent offset

    EXTENT_SIZE = 10 # uint64_t + uint16_t = 10 bytes
    current_patch_offset = extent_offset

    for extent in all_extents:
        #print(f'extent, lba: {extent.lba} len: {extent.length} patch: offset {current_patch_offset}')
        bytearray_write64(ldlinux_bytes, extent.lba,    current_patch_offset + 0)
        bytearray_write16(ldlinux_bytes, extent.length, current_patch_offset + 8)        
        current_patch_offset += EXTENT_SIZE

def patch_ldlinux_sys(mm: mmap.mmap, partition_starting_sector, ldlinux_sys_offset, ldlinux_sys_size, ldlinux_size_including_adv, bytes_per_sector=512):

    """ For reference on this sh*t, here's some of the defines in the SYSLINUX boot code...
    DataSectors	dw 0		; Number of sectors (not including bootsec)
    ADVSectors	dw 0		; Additional sectors for ADVs
    LDLDwords	dd 0		; Total dwords starting at ldlinux_sys,
    CheckSum	dd 0		; Checksum starting at ldlinux_sys
                    ; value = LDLINUX_MAGIC - [sum of dwords]
    MaxTransfer	dw 127		; Max sectors to transfer
    EPAPtr		dw EPA - LDLINUX_SYS	; Pointer to the extended patch area


    EPA:
    ADVSecPtr	dw ADVSec0 - LDLINUX_SYS                                                        # 16d0 <-- THESE ARE RELATIVE TO PARTITION START!!!! 
                                                                                                    ATTN! need to write two 64 bit values here for both sectors!
    CurrentDirPtr	dw CurrentDirName-LDLINUX_SYS	; Current directory name string             # 145c <-- Don't need to touch this
    CurrentDirLen	dw CURRENTDIR_MAX                                                           # 0100 <-- Don't need to touch this
    SubvolPtr	dw SubvolName-LDLINUX_SYS                                                       # 155c <-- Don't need to touch this
    SubvolLen	dw SUBVOL_MAX                                                                   # 0100 <-- Don't need to touch this
    SecPtrOffset	dw SectorPtrs-LDLINUX_SYS                                                   # 01ee <-- need to touch the area where this points by generating the "extents" 
    SecPtrCnt	dw (SectorPtrsEnd - SectorPtrs)/10                                              # 000c <-- ^ whatever the hell that means

    Sect1Ptr0Ptr	dw Sect1Ptr0 - bootsec		; Pointers to Sector 1 location                 # <-- patch this location with low dword
    Sect1Ptr1Ptr	dw Sect1Ptr1 - bootsec                                                      # <-- patch this location with high dword
    RAIDPatchPtr	dw kaboom.again - bootsec	; Patch to INT 18h in RAID mode                 # <-- we're not doing this so whatever
    """

    # why is this so ass backwards, just to save a few bytes in the boot sector?!?!
    # why not just use two sectors? you have a million reserved sectors before any relevant data
    # fat32 uses 2 boot sectors anyway like why
    # just why
    # ???

    LDLINUX_MAGIC = 0x3eb202fe
    LDLINUX_MAGIC_BYTES = bytearray([0xfe, 0x02, 0xb2, 0x3e])

    LDLINUX_ADV_LEN = 512 - 3 * 4

    ldlinux_sys_sector = ldlinux_sys_offset // bytes_per_sector
    ldlinux_sys_sector_relative = ldlinux_sys_sector - partition_starting_sector

    ldlinux_sys_size_padded = get_padded_size(ldlinux_sys_size, bytes_per_sector)

    # print(f'ldlinux offset {hex(ldlinux_sys_offset)}, ldlinux absolute sector {hex(ldlinux_sys_sector)} relative {hex(ldlinux_sys_sector_relative)}')

    ldlinux_bytes = mm_read_bytearray(mm, ldlinux_size_including_adv, ldlinux_sys_offset)

    # Find patch area

    ldlinux_sys_patch_area_offset = ldlinux_bytes.index(LDLINUX_MAGIC_BYTES) + 8 # Skip magic and date thing

    DataSectors = ldlinux_sys_size_padded // 512   # size in sectors excluding ADV
    ADVSectors = 2  # 2, i donno why... too complicated
    LDLDWords = get_padded_size(ldlinux_sys_size, 4) // 4 # size in DWORDs because why tf not... 
    CheckSum = 0 # We will calculate it later

    # Write all of this crap

    bytearray_write16(ldlinux_bytes, DataSectors,   ldlinux_sys_patch_area_offset + 0)
    bytearray_write16(ldlinux_bytes, ADVSectors,    ldlinux_sys_patch_area_offset + 2)
    bytearray_write32(ldlinux_bytes, LDLDWords,     ldlinux_sys_patch_area_offset + 4)
    bytearray_write32(ldlinux_bytes, CheckSum,      ldlinux_sys_patch_area_offset + 8)

    # Find Extended Patch Area (EPA)

    ldlinux_sys_epa_offset = bytearray_read16(ldlinux_bytes, ldlinux_sys_patch_area_offset + 14)

    # print(f'ldlinux Patch offset {hex(ldlinux_sys_patch_area_offset)} EPA offset: {hex(ldlinux_sys_epa_offset)}')

    # ADV sector address = ldlinux relative address + ldlinux size padded to sector size/bytes per sector

    ldlinux_adv_sector_ptr = bytearray_read16(ldlinux_bytes, ldlinux_sys_epa_offset + 0)
    ldlinux_adv_sector_relative = ldlinux_sys_sector_relative + (ldlinux_sys_size_padded // bytes_per_sector)

    bytearray_write64(ldlinux_bytes, ldlinux_adv_sector_relative, ldlinux_adv_sector_ptr)
    bytearray_write64(ldlinux_bytes, ldlinux_adv_sector_relative + 1, ldlinux_adv_sector_ptr + 8)

    # Do the sector extents....
    ldlinux_extents_ptr = bytearray_read16(ldlinux_bytes, ldlinux_sys_epa_offset + 10)
    ldlinux_extents_count = bytearray_read16(ldlinux_bytes, ldlinux_sys_epa_offset + 12)
    
    patch_ldlinux_sys_extents(ldlinux_bytes, ldlinux_sys_sector_relative, ldlinux_size_including_adv, ldlinux_extents_ptr, ldlinux_extents_count, bytes_per_sector)

    # Make two blank ADV sectors
    blank_adv_bytes = syslinux_make_blank_adv()
    adv1_offset = ldlinux_sys_size_padded
    adv2_offset = adv1_offset + bytes_per_sector

    ldlinux_bytes[adv1_offset:adv1_offset + bytes_per_sector] = blank_adv_bytes
    ldlinux_bytes[adv2_offset:adv2_offset + bytes_per_sector] = blank_adv_bytes

    # Now calculate the checksum and write it

    CheckSum = LDLINUX_MAGIC
    for i in range(0, LDLDWords):
        CheckSum -= bytearray_read32(ldlinux_bytes, i * 4)
        CheckSum &= 0xFFFFFFFF
        
    bytearray_write32(ldlinux_bytes, CheckSum, ldlinux_sys_patch_area_offset + 8)

    mm_write_bytearray(mm, ldlinux_bytes, ldlinux_sys_offset)

# Bytearray helper functions

def bytearray_read16(ba: bytearray, offset=0):
    return struct.unpack_from('=H', ba, offset)[0]

def bytearray_read32(ba: bytearray, offset=0):
    return struct.unpack_from('=I', ba, offset)[0]

def bytearray_write8(ba: bytearray, num, offset=0):
    ba[offset] = num

def bytearray_write16(ba: bytearray, num, offset=0):
    w = struct.pack('=H', num)
    ba[offset:offset+len(w)] = w

def bytearray_write32(ba: bytearray, num, offset=0):
    w = struct.pack('=I', num)
    ba[offset:offset+len(w)] = w
    
def bytearray_write64(ba: bytearray, num, offset=0):
    w = struct.pack('=Q', num)
    ba[offset:offset+len(w)] = w

def mm_read_bytearray(mm: mmap.mmap, count, offset=0):
    mm.seek(offset, 0)
    return bytearray(mm.read(count))

def mm_write_bytearray(mm: mmap.mmap, buffer: bytearray, offset=0):
    mm.seek(offset, 0)
    mm.write(buffer)

def syslinux_make_blank_adv():
    ADV_MAGIC1 = 0x5a2d2fa5 # /* Head signature */
    ADV_MAGIC2 = 0xa3041767 # /* Total checksum */
    ADV_MAGIC3 = 0xdd28bf64 # /* Tail signature */
    ADV_SIZE = 512

    adv_bytes = bytearray(ADV_SIZE)

    # Write signature 1

    bytearray_write32(adv_bytes, ADV_MAGIC1, 0)

    # Calculate a checksum ... ported from c Code
    
    csum = ADV_MAGIC2

    for i in range(8, ADV_SIZE - 4, 4):
        csum -= bytearray_read32(adv_bytes, i)
    
    # Write the checksum and tail sig

    bytearray_write32(adv_bytes, csum, 4)
    bytearray_write32(adv_bytes, ADV_MAGIC3, ADV_SIZE - 4)

    return adv_bytes


def patch_syslinux_bootsector(mm: mmap.mmap, partition_offset_sector, ldlinux_sys_sector, bytes_per_sector=512):
   
    bootsect_bin = os.path.join(os.curdir, 'tools', 'syslinux_bs.bin')

    partition_offset_bytes = partition_offset_sector * bytes_per_sector

    ldlinux_sys_sector_relative = ldlinux_sys_sector - partition_offset_sector 

    with open(bootsect_bin, 'rb') as f:
        bootsector_bytes = bytearray(f.read(512))

    # Find byte patterns for the low  and high dwords of the first sector address

    ptr1 = bootsector_bytes.index(bytearray([0xef, 0xbe, 0xad, 0xde])) # 0xdeadbeef
    ptr2 = bootsector_bytes.index(bytearray([0xce, 0xfa, 0xed, 0xfe])) # 0xfeedface

    high_dword = struct.pack('=I', ldlinux_sys_sector_relative >> 32)
    low_dword = struct.pack('=I', ldlinux_sys_sector_relative & 0xFFFFFFFF)

    print(f'Patching SYSLINUX boot sector at sector {hex(partition_offset_sector)}, LDLINUX.SYS sector {hex(ldlinux_sys_sector)} (partition-relative {hex(ldlinux_sys_sector_relative)})')

    bootsector_bytes[ptr1:ptr1+4] = low_dword[0:4]
    bootsector_bytes[ptr2:ptr2+4] = high_dword[0:4]

    existing_bs = mm_read_bytearray(mm, bytes_per_sector, partition_offset_bytes)

    # JMP opcode, identifier
    existing_bs[0x00:0x0B] = bootsector_bytes[0x00:0x0B]

    # MBR code
    existing_bs[0x5A:0x200] = bootsector_bytes[0x5A:0x200]
    
    mm_write_bytearray(mm, existing_bs, partition_offset_bytes)
    
    # Write the copy too
    bootsector_copy_offset = partition_offset_bytes + bytearray_read16(existing_bs, 0x32) * bytes_per_sector
    mm_write_bytearray(mm, existing_bs, bootsector_copy_offset)

def get_padded_size(size, padding):
    return (size + padding - 1) // padding * padding

def install_syslinux(filename, partition_starting_sector, bytes_per_sector=512):
    print(f'Installing SYSLINUX bootloader to image "{filename}"')

    protocol_file = f'fat://{fs.path.normpath(os.path.abspath(filename))}?offset={partition_starting_sector * bytes_per_sector}'

    ldlinux_sys_path = os.path.join(os.curdir, 'tools', 'ldlinux.sys')
    ldlinux_c32_path = os.path.join(os.curdir, 'tools', 'ldlinux.c32')

    with fs.open_fs(protocol_file) as filesystem:
        # I think with PyFilesystem it's faster to write the files manually than using the copy functions? at least that's how it's behaving on my Windows box

        with filesystem.open('ldlinux.sys', 'wb') as outfile:
            with open(ldlinux_sys_path, 'rb') as infile:
                ldlinux_sys_bytes = infile.read()
                ldlinux_sys_size = len(ldlinux_sys_bytes) # Pad to sector size + 2 sectors for ADV
            
            # We need to pad to the next sector size and add 2 sectors to add the "ADV" region whatever the f*** that is
            ldlinux_sys_write_size = get_padded_size(ldlinux_sys_size + 2 * bytes_per_sector, bytes_per_sector)
            outfile.write(ldlinux_sys_bytes)
            outfile.write(bytearray(ldlinux_sys_write_size - ldlinux_sys_size))
            

        with filesystem.open('ldlinux.c32', 'wb') as outfile:
            with open(ldlinux_c32_path, 'rb') as infile:
                ldlinux_c32_bytes = infile.read()
                outfile.write(ldlinux_c32_bytes)

    filesystem.close()

    # get offset of the ldlinux.sys file we just wrote.
    # It seems this can be done with "get_cluster" but this is all a bit too complicated for my tiny brain

    with open(filename, "r+b") as f:
        mm = mmap.mmap(f.fileno(), 0)

        ldlinux_sys_offset = mm.find(ldlinux_sys_bytes)
        ldlinux_sys_sector = ldlinux_sys_offset // bytes_per_sector

        # print(f'LDLINUX.SYS found at {ldlinux_sys_offset} ({hex(ldlinux_sys_offset)}), sector {ldlinux_sys_sector} (hex({hex(ldlinux_sys_sector)}))')

        # Patch boot sector to include the starting sector of syslinux
        patch_syslinux_bootsector(mm, partition_starting_sector, ldlinux_sys_sector)

        # Patch all the crap to be patched in LDLINUX.SYS
        patch_ldlinux_sys(mm, partition_starting_sector, ldlinux_sys_offset, ldlinux_sys_size, ldlinux_sys_write_size, bytes_per_sector)