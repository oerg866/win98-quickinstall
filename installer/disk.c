/*
 * LUNMERCY - Disk related functionality
 * (C) 2023 Eric Voirin (oerg866@googlemail.com)
 */

#include "util.h"

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <malloc.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#define CMD_LSBLK_HDD_ONLY "lsblk -I 8 -d -n -p"
#define CMD_LSBLK_HDD_NAME_SIZE "lsblk -I 8 -d -n -b -p -oKNAME,SIZE,MIN-IO,OPT-IO"
#define CMD_LSBLK_HDD_MODEL "lsblk -I 8 -d -n -o MODEL"
#define CMD_LSBLK_HDD_WITH_PARTS_AND_FS "lsblk -I 8 -n -b -p -oKNAME,SIZE,PARTTYPE"
#define CMD_OUTPUT_BUF_SIZE (64*1024)

size_t util_getSystemHardDiskCount() {
    return util_getCommandOutputLineCount(CMD_LSBLK_HDD_ONLY);
}

util_HardDiskArray util_getSystemHardDisks() {
    size_t hddCount = util_getSystemHardDiskCount();
    char *cmdOutputBuf = calloc(1, CMD_OUTPUT_BUF_SIZE);
    char *cmdOutputBufPos = cmdOutputBuf;
    size_t totalPartitionCount = 0;
    util_HardDiskArray ret;

    util_HardDisk *disks = calloc(sizeof(util_HardDisk), hddCount);

    // Get disk models
    util_captureCommandOutput(CMD_LSBLK_HDD_NAME_SIZE, cmdOutputBuf, CMD_OUTPUT_BUF_SIZE);

    for (int i = 0; i < hddCount; ++i) {
        // format of the output is KNAME SIZE MIN-IO OPT-IO\n, so we need to scan until that character, that's our line.
        // cspn then gets us how many characters that whole thing was to advance the string pointer
        sscanf(cmdOutputBufPos, "%16s %llu %u %u[^\n]", disks[i].device, &disks[i].size, &disks[i].sectorSize, &disks[i].optIoSize);
        cmdOutputBufPos += strcspn(cmdOutputBufPos,"\n") + 1;
    }

    // Get disk models
    cmdOutputBufPos = cmdOutputBuf;
    util_captureCommandOutput(CMD_LSBLK_HDD_MODEL, cmdOutputBuf, CMD_OUTPUT_BUF_SIZE);

    for (int i = 0; i < hddCount; ++i) {
        // Can't think of a simpler way to put the whole line from a string into another... why is sgets not a thing??
        sscanf(cmdOutputBufPos, " %[^\n]", disks[i].model);
        cmdOutputBufPos += strcspn(cmdOutputBufPos,"\n") + 1;
        //printf("Disk %d is '%s', Model '%s', Size %llu bytes.\n", i, disks[i].device, disks[i].model, disks[i].size);
    }

    // Reset buffer once again
    cmdOutputBufPos = cmdOutputBuf;

    size_t fullDiskListLineCount = util_captureCommandOutput(CMD_LSBLK_HDD_WITH_PARTS_AND_FS, cmdOutputBuf, CMD_OUTPUT_BUF_SIZE);
    totalPartitionCount = fullDiskListLineCount - hddCount; // The captured list *includes* the hdds themselves! we need to filter them!
    util_Partition *partitions = calloc(sizeof(util_Partition), totalPartitionCount);

    // Now figure out where partitions go

    for (int i = 0, curPart = 0, curDisk = 0; i < fullDiskListLineCount; i++) {
        char partDevice[16] = {0};
        uint64_t partSize = 0;
        uint32_t fsTypeByte;

        sscanf(cmdOutputBufPos, "%16s %llu %x[^\n]", partDevice, &partSize, &fsTypeByte);
        cmdOutputBufPos += strcspn(cmdOutputBufPos,"\n") + 1;
        
        if (!util_stringStartsWith(partDevice, disks[curDisk].device)) {
            // This is on the *next* disk. Increment disk index.
            curDisk++;
            assert(curDisk < hddCount); // oopsie poopsie
        }

        if (util_stringEquals(partDevice, disks[curDisk].device)) {
            // This is our disk, not a partition, ignore it. We take this opportunity to set the current disks partitions pointer.
            disks[curDisk].partitions = &partitions[curPart];
            continue;
        } 

        //printf("Found partition %s, size %llu, file system %s\n", partDevice, partSize, fsString);
        strcpy(partitions[curPart].device, partDevice);
        partitions[curPart].size = partSize;
        partitions[curPart].fileSystem = util_partitionTypeByteToUtilFilesystem((uint8_t) fsTypeByte);
        partitions[curPart].sectorSize = disks[curDisk].sectorSize;
        partitions[curPart].parent = &disks[curDisk];
        partitions[curPart].index = atoi(partDevice + strlen(disks[curDisk].device));   // so for example "/dev/sda1" would have atoi called on the "1" part
        curPart++;

        disks[curDisk].partitionCount++;        
    }

    ret.disks = disks;
    ret.count = hddCount;
    return ret;
}

void util_hardDiskArrayDeinit(util_HardDiskArray hdds) {
    if (hdds.disks) {
        // The first partition is the pointer to the start of the memory block that holds *all partitions*, so we only do one free here
        if (hdds.count) free(hdds.disks[0].partitions);
        free(hdds.disks);
    }
}

util_FileSystem util_partitionTypeByteToUtilFilesystem(uint8_t partitionType) {
    switch(partitionType) {
        case 0x00:
            return fs_none;
        case 0x04:  // FAT16  < 32MB
        case 0x06:  // FAT16 >= 32MB 
        case 0x0E:  // FAT16 >= 32MB LBA
        case 0x14:  // FAT16  < 32MB        HIDDEN
        case 0x16:  // FAT16 >= 32MB        HIDDEN
        case 0x1E:  // FAT16 >= 32MB LBA    HIDDEN
            return fs_fat16;
        case 0x0B:  // FAT32
        case 0x0C:  // FAT32 LBA
        case 0x1B:  // FAT32        HIDDEN
        case 0x1C:  // FAT32 LBA    HIDDEN
            return fs_fat32;
        default:
            return fs_unsupported;
    }
}

static const char *UTIL_FS_STRINGS[FS_ENUM_SIZE] = {
    "NO FILE SYSTEM",
    "UNSUPPORTED FILESYSTEM",
    "FAT16",
    "FAT32"
};

const char *util_utilFilesystemToString(util_FileSystem fs) {
    return UTIL_FS_STRINGS[(size_t) fs];
}

const char *util_shortDeviceString(const char *str) {
    if (!str || str[0] != '/') return str;
    return strrchr(str, '/') + 1;
}

// mounts a disk under a special name, i.e. /dev/sda1 will be mounted at /dev_sda1
bool util_mountPartition(util_Partition *part) {
    char *mountPath = strdup(part->device);
    assert(mountPath && strlen(mountPath) > 1);
    util_stringReplaceChar(&mountPath[1], '/', '_'); // Ignore initial '/' character
    mkdir(mountPath, 0777);
    

    char mountCmd[1024];
    snprintf(mountCmd, sizeof(mountCmd), "mount -t vfat %s %s", part->device, mountPath);
    if (system(mountCmd) == 0) {
        part->mountPath = mountPath;
        return true;
    } else {
        part->mountPath = NULL;
        free(mountPath);
        return false;
    }
}

bool util_unmountPartition(util_Partition *part) {
    if (part->mountPath != NULL) {
        char mountCmd[1024];
        snprintf(mountCmd, sizeof(mountCmd), "umount %s", part->mountPath);
        free(part->mountPath);
        part->mountPath = NULL;
        return (system(mountCmd) == 0);
    } else {
        printf("%s: Partition not mounted\n", __func__);
        return false;
    }
}

size_t util_getHardDiskArrayIndexFromDevicestring(util_HardDiskArray *hdds, const char *str) {
    for (size_t i = 0; i < hdds->count; i++)
        if (util_stringEquals(hdds->disks[i].device, str))
            return i;
    return SIZE_MAX;
}

util_Partition *util_getPartitionFromDevicestring(util_HardDiskArray *hdds, const char *str) {
    for (size_t disk = 0; disk < hdds->count; disk++)
        for (size_t part = 0; part < hdds->disks[disk].partitionCount; part++)
            if (util_stringEquals(hdds->disks[disk].partitions[part].device, str))
                return &hdds->disks[disk].partitions[part];
    return NULL;
}

// Reads bytes from a file descriptor in a loop until all of them are read
static bool util_readFromFD(int fd, uint8_t *buf, size_t length) {    
    size_t bytesRead;
    while (length) {
        bytesRead = read(fd, buf, length);
        if (bytesRead < 0) return false;
        length -= bytesRead;
        buf += bytesRead;        
    }
    return true;
}

// Writes bytes to a file descriptor in a loop until all of them are read
static bool util_writeToFD(int fd, const uint8_t *buf, size_t length) {    
    size_t bytesWritten;
    while (length) {
        bytesWritten = write(fd, buf, length);
        if (bytesWritten < 0) return false;
        length -= bytesWritten;
        buf += bytesWritten;        
    }
    return true;
}

// Reads a sector from a device into existing buffer
static bool util_readSector(char *dev, size_t sector, size_t ioSize, uint8_t *buf) {
    int disk = open(dev, O_RDONLY);
    if (disk < 0) return false;

    size_t offset = ioSize * sector;
    lseek(disk, offset, SEEK_SET);
    bool result = util_readFromFD(disk, buf, ioSize);
    close(disk);
    return result;
}

// Allocates and returns a buffer reading a sector from a device
static uint8_t *util_readSectorAllocate(char *dev, size_t sector, size_t ioSize) {
    uint8_t *sectorBuf = calloc(ioSize, 1);
    if (!sectorBuf) return NULL;
    if (!util_readSector(dev, sector, ioSize, sectorBuf)) {
        free(sectorBuf);
        return NULL;
    }
    return sectorBuf;
}

// Write a sector to a device
static bool util_writeSector(char *dev, size_t sector, size_t ioSize, const uint8_t *buf) {

    int disk = open(dev, O_WRONLY);
    if (!disk) return false;

    size_t offset = ioSize * sector;
    lseek(disk, offset, SEEK_SET);
    bool result = util_writeToFD(disk, buf, ioSize);
    close(disk);
    return result;
}

bool util_readSectorFromDisk(util_HardDisk *hdd, size_t sector, uint8_t *buf) {
    return util_readSector(hdd->device, sector, hdd->sectorSize, buf);
}

bool util_readSectorFromPartition(util_Partition *part, size_t sector, uint8_t *buf) {
    return util_readSector(part->device, sector, part->sectorSize, buf);
}

bool util_writeSectorToDisk(util_HardDisk *hdd, size_t sector, const uint8_t *buf) {
    return util_writeSector(hdd->device, sector, hdd->sectorSize, buf);
}

bool util_writeSectorToPartition(util_Partition *part, size_t sector, const uint8_t *buf) {
    return util_writeSector(part->device, sector, part->sectorSize, buf);
}

uint8_t *util_readSectorFromDiskAllocate(util_HardDisk *hdd, size_t sector) {
    return util_readSectorAllocate(hdd->device, sector, hdd->sectorSize);
}

uint8_t *util_readSectorFromPartitionAllocate(util_Partition *part, size_t sector) {
    return util_readSectorAllocate(part->device, sector, part->sectorSize);
}

#include "mbr_boot_win98.h"

bool util_writeWin98MBRToDrive(util_HardDisk *hdd) {
    // Read existing MBR first
    uint8_t *existingMBR = util_readSectorFromDiskAllocate(hdd, 0);
    if (!existingMBR) return false;

    // Overwrite the start with the win98 MBR code
    memcpy(existingMBR, __MBR_WIN98__, DISK_MBR_CODE_LENGTH);

    // write it back to the disk
    bool result = util_writeSectorToDisk(hdd, 0, existingMBR);
    free(existingMBR);
    return result;
}

static bool util_modifyBootSector(util_Partition *part, util_BootSectorModifierList modifierList) {
    uint8_t *sector = NULL;
    size_t oldSectorIndex = 0;
    bool success = true;
    for (size_t i = 0; i < modifierList.count; i++) {
        util_BootSectorModifier mod = modifierList.modifiers[i];
        // New sector index found in the list = we need to write previous sector and free its databuffer
        if (sector && (mod.sectorIndex != oldSectorIndex)) {
            success = util_writeSectorToPartition(part, oldSectorIndex, sector);
            free (sector);
            sector = NULL;
        }
        
        printf("%d %d %d %d\n", i, (int) mod.sectorIndex, (int) mod.offset, (int) mod.length);
        // util_hexDump(mod.replacementData, 0, mod.length);

        // If we need to read a new sector
        if (sector == NULL) {
            sector = util_readSectorFromPartitionAllocate(part, mod.sectorIndex);
            oldSectorIndex = mod.sectorIndex;
            success &= (sector != NULL);
        }

        assert(success && "modifyBootSector failed");

        if (!success) {
            free(sector);
            return false;
        }

        // Copy the replacement data therefore injecting the boot sector code

        memcpy(sector + mod.offset, mod.replacementData, mod.length);
    }

    // If there is a pending sector write left, execute it now
    if (sector) {
        success = util_writeSectorToPartition(part, oldSectorIndex, sector);
        free(sector);
    }

    return success;
}

bool util_writeWin98BootSectorToPartition(util_Partition *part) {
    // Fat16 only has one boot sector and smaller BPB, so we need to use a different, simpler modification list for it
    util_BootSectorModifierList bsModifierList = part->fileSystem == fs_fat16 ? __WIN98_FAT16_BOOT_SECTOR_MODIFIER_LIST__ 
                                                                              : __WIN98_FAT32_BOOT_SECTOR_MODIFIER_LIST__;
    bool result = util_modifyBootSector(part, bsModifierList);

    if (result && part->fileSystem == fs_fat32) {
        // Copy sectors to backup now (FAT32 only, FAT16 has no backup it seems, at least not on Win9x)
        size_t backupSectorIndex = 0;

        for (size_t i = 0; i < 3; i++) {
            uint8_t *sector = util_readSectorFromPartitionAllocate(part, i);
            assert(sector);
            // Get backup sector index
            if (i == 0) backupSectorIndex = (size_t) util_getUInt16fromBuffer(sector, 0x32);

            // Write sector to backup location
            result &= util_writeSectorToPartition(part, i + backupSectorIndex, sector);
            free(sector);
        }
    }

    return result;
}

bool util_isPartitionMounted(util_Partition *part) {
    return (part->mountPath != NULL);
}

bool util_formatPartition(util_Partition *part, util_FileSystem fs) {
    assert(part);
    if (util_isPartitionMounted(part)) {
        util_unmountPartition(part);
    }

    char formatCmd[1024];
    if (fs == fs_fat16) {
        snprintf(formatCmd, sizeof(formatCmd), "mkfs.fat -S %d -F 16 %s", part->sectorSize, part->device); // no clue what win9x wants to see here tbh...
    } else if (fs == fs_fat32) {
        snprintf(formatCmd, sizeof(formatCmd), "mkfs.fat -S %d -s 8 -R 32 -f 2 -F 32 %s", part->sectorSize, part->device);
    } else {
        assert(false && "Wrong file system");
        abort();
    }
    return (system(formatCmd) == 0);
}

bool util_copyFile(const char *src, const char *dst) {
    // TODO: Implement this ourselves. Sorry the programmer is quite lazy
    // if the paths contain spaces, all hell breaks loose ....
    char copyCmd[1024];
    snprintf(copyCmd, sizeof(copyCmd), "cp %s %s", src, dst);
    return (system(copyCmd) == 0);
}

