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

#include "qi_assert.h"

#define CMD_SURPRESS_OUTPUT " 2>/dev/null 1>/dev/null"

#define CMD_LSBLK_ALL "lsblk -I 8,3,259 -n -b -p -P -oTYPE,KNAME,PARTTYPE,SIZE,MIN-IO,OPT-IO,MODEL,PTTYPE"

// Update parents after a reallocation of a hard disk array
static inline void util_HardDisksUpdatePartitionParents(util_HardDisk *hdds, size_t diskCount) {
    if (hdds != NULL) {
        for (size_t h = 0; h < diskCount; h++) {
            util_HardDisk *hdd = &hdds[h];
            for (size_t p = 0; p < hdd->partitionCount; p++) {
                hdd->partitions[p].parent = hdd;
            }
        }
    }
}

// Appends a HardDisk with given parameters to a HardDiskArray and returns a pointer to the newly created disk.
static util_HardDisk *util_HardDiskArrayAppend(util_HardDiskArray *hda, const char *device, const char *model, uint64_t size, uint32_t sectorSize, uint32_t optIoSize, const char *tableType) {
    QI_ASSERT(hda != NULL);
    QI_ASSERT(device != NULL);
    QI_ASSERT(model != NULL);
    QI_ASSERT(tableType != NULL);

    hda->count++;
    hda->disks = realloc(hda->disks, hda->count * sizeof(util_HardDisk));

    QI_ASSERT(hda->disks != NULL);

    util_HardDisksUpdatePartitionParents(hda->disks, hda->count - 1);

    util_HardDisk *ret = &hda->disks[hda->count - 1];

    memset(ret, 0, sizeof(util_HardDisk));

    strncpy(ret->device, device, sizeof(ret->device));
    strncpy(ret->model, model, sizeof(ret->model));
    strncpy(ret->tableType, tableType, sizeof(ret->tableType));

    ret->size = size;
    ret->sectorSize = sectorSize;
    ret->optIoSize = optIoSize;

    return ret;
}

static util_Partition *util_HardDiskAddPartition(util_HardDisk *hd, const char *device, uint64_t size, uint32_t sectorSize, util_FileSystem fileSystem, size_t indexOnParent) {
    QI_ASSERT(hd != NULL);

    hd->partitionCount++;
    hd->partitions = realloc(hd->partitions, hd->partitionCount * sizeof(util_Partition));

    QI_ASSERT(hd->partitions != NULL);

    util_Partition *ret = &hd->partitions[hd->partitionCount - 1];

    memset(ret, 0, sizeof(util_Partition));

    strncpy(ret->device, device, sizeof(ret->device));

    ret->size = size;
    ret->sectorSize = sectorSize;
    ret->fileSystem = fileSystem;
    ret->parent = hd;
    ret->indexOnParent = indexOnParent;

    return ret;
}

// Gets a value for a given key from a '<key>="value" <key2>="value2"' esque input string
static bool util_getValueFromKey(const char *input, const char *key, char *value, size_t valueBufSize) {
    const char *curPos = input;

    QI_ASSERT(input != NULL);
    QI_ASSERT(key != NULL);
    QI_ASSERT(value != NULL);

    size_t keyLen = strlen(key);
    const char equalsQuote[] = "=\"";

    while (!(util_stringStartsWith(curPos, key) && util_stringStartsWith(curPos + keyLen, equalsQuote))) {
        if (*curPos == 0x00)
            return false;

        curPos++;
    }

    // If we're still here, the string actually matches what we need, skip to the actual value
    curPos += keyLen + sizeof(equalsQuote) - 1;

    // Find the closing quote for the value
    const char *valueEnd = strchr(curPos, '\"');

    if (valueEnd == NULL)
        return false;

    size_t valueLength = valueEnd - curPos;
    
    // Cap to buffer size
    valueLength = MIN(valueLength, valueBufSize - 1);
    memcpy(value, curPos, valueLength);
    value[valueLength] = 0x00;
    
    return true;
}

util_HardDiskArray *util_getSystemHardDisks() {
    util_HardDiskArray *ret = calloc(1, sizeof(util_HardDiskArray));

    QI_ASSERT(ret != NULL);

    // Get disk models
    util_CommandOutput *lsblkOut = util_commandOutputCapture(CMD_LSBLK_ALL);

    QI_ASSERT(lsblkOut != NULL);

    if (lsblkOut->lineCount == 0) {
        util_commandOutputDestroy(lsblkOut);
        return ret;
    }

    util_HardDisk *currentDisk = NULL;

    for (size_t i = 0; i < lsblkOut->lineCount; ++i) {
        char tmpType        [4+1] = "";
        char tmpDevice      [UTIL_HDD_DEVICE_STRING_LENGTH];
        char tmpModel       [UTIL_HDD_MODEL_STRING_LENGTH];
        char tmpPtType      [UTIL_TABLE_TYPE_STRING_LENGTH] = "";
        char tmpNumStr      [64+1] = "";
        bool success = true;
        
        success &= util_getValueFromKey(lsblkOut->lines[i], "TYPE", tmpType, sizeof(tmpType));
        
        success &= util_getValueFromKey(lsblkOut->lines[i], "KNAME", tmpDevice, sizeof(tmpDevice));

        success &= util_getValueFromKey(lsblkOut->lines[i], "MODEL", tmpModel, sizeof(tmpModel));

        success &= util_getValueFromKey(lsblkOut->lines[i], "PTTYPE", tmpPtType, sizeof(tmpPtType));
        
        success &= util_getValueFromKey(lsblkOut->lines[i], "PARTTYPE", tmpNumStr, sizeof(tmpNumStr));
        util_FileSystem tmpFileSystem = util_partitionTypeByteToUtilFilesystem(strtoul(tmpNumStr, NULL, 16));

        success &= util_getValueFromKey(lsblkOut->lines[i], "SIZE", tmpNumStr, sizeof(tmpNumStr));
        uint64_t tmpSize = strtoull(tmpNumStr, NULL, 10);
        
        success &= util_getValueFromKey(lsblkOut->lines[i], "MIN-IO", tmpNumStr, sizeof(tmpNumStr));
        uint32_t tmpSectorSize = strtoull(tmpNumStr, NULL, 10);
        
        success &= util_getValueFromKey(lsblkOut->lines[i], "OPT-IO", tmpNumStr, sizeof(tmpNumStr));
        uint32_t tmpOptIoSize = strtoull(tmpNumStr, NULL, 10);

        if (util_stringEquals(tmpType, "disk")) {
            /* This is a hard disk. */
            currentDisk = util_HardDiskArrayAppend(ret, tmpDevice, tmpModel, tmpSize, tmpSectorSize, tmpOptIoSize, tmpPtType);
        } else if (util_stringEquals(tmpType, "part")) {
            /* This is a partition. */
            QI_ASSERT(currentDisk != NULL);
            
            /* The beginning of the device name must match the parent disk's as a child partition of that disk. */
            QI_ASSERT(util_stringStartsWith(tmpDevice, currentDisk->device) == true);

            size_t index = (size_t) atoi(tmpDevice + strlen(currentDisk->device));   // so for example "/dev/sda1" would have atoi called on the "1" part
            util_HardDiskAddPartition(currentDisk,tmpDevice, tmpSize, tmpSectorSize, tmpFileSystem, index);
        } else {
            /* Tape streamer or other weird thing, not of interest for us... */
            continue;
        }
    }

    util_commandOutputDestroy(lsblkOut);
    return ret;
}

void util_hardDiskArrayDestroy(util_HardDiskArray *hdds) {
    if (hdds) {
        if (hdds->disks) {
            // Free partitions
            for (size_t i = 0; i < hdds->count; i++) {
                for (size_t p = 0; p < hdds->disks[i].partitionCount; p++) {
                    util_unmountPartition(&hdds->disks[i].partitions[p]);
                }
                free(hdds->disks[i].partitions);
            }
            // Free disks
            free(hdds->disks);
        }

        free(hdds);
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
    snprintf(mountCmd, sizeof(mountCmd), "mount -t vfat %s %s" CMD_SURPRESS_OUTPUT, part->device, mountPath);
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
        snprintf(mountCmd, sizeof(mountCmd), "umount %s" CMD_SURPRESS_OUTPUT, part->mountPath);
        free(part->mountPath);
        part->mountPath = NULL;
        return (system(mountCmd) == 0);
    } else {
        return true;
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

util_Partition *util_getPartitionFromIndex(util_HardDiskArray *hdds, size_t index) {
    size_t curDisk = 0;
    for (size_t disk = 0; disk < hdds->count; disk++) {
        for (size_t part = 0; part < hdds->disks[disk].partitionCount; part++) {
            if (curDisk == index) return &hdds->disks[disk].partitions[part];
            curDisk++;

        }
    }
    return NULL;
}

// Reads bytes from a file descriptor in a loop until all of them are read
static bool util_readFromFD(int fd, uint8_t *buf, size_t length) {    
    ssize_t bytesRead;
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
    ssize_t bytesWritten;
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
    util_returnOnNull(hdd, NULL);
    return util_readSector(hdd->device, sector, hdd->sectorSize, buf);
}

bool util_readSectorFromPartition(util_Partition *part, size_t sector, uint8_t *buf) {
    util_returnOnNull(part, NULL);
    return util_readSector(part->device, sector, part->sectorSize, buf);
}

bool util_writeSectorToDisk(util_HardDisk *hdd, size_t sector, const uint8_t *buf) {
    util_returnOnNull(hdd, NULL);
    return util_writeSector(hdd->device, sector, hdd->sectorSize, buf);
}

bool util_writeSectorToPartition(util_Partition *part, size_t sector, const uint8_t *buf) {
    util_returnOnNull(part, NULL);
    return util_writeSector(part->device, sector, part->sectorSize, buf);
}

uint8_t *util_readSectorFromDiskAllocate(util_HardDisk *hdd, size_t sector) {
    util_returnOnNull(hdd, NULL);
    return util_readSectorAllocate(hdd->device, sector, hdd->sectorSize);
}

uint8_t *util_readSectorFromPartitionAllocate(util_Partition *part, size_t sector) {
    util_returnOnNull(part, NULL);
    return util_readSectorAllocate(part->device, sector, part->sectorSize);
}

bool util_writeMBRToDrive(util_HardDisk *hdd, const uint8_t *newMBRCode) {
    // Read existing MBR first
    uint8_t *existingMBR = util_readSectorFromDiskAllocate(hdd, 0);
    if (!existingMBR) return false;

    QI_ASSERT(newMBRCode != NULL);

    // Overwrite the start with the win98 MBR code
    memcpy(existingMBR, newMBRCode, DISK_MBR_CODE_LENGTH);

    // write it back to the disk
    bool result = util_writeSectorToDisk(hdd, 0, existingMBR);
    free(existingMBR);
    return result;
}

bool util_wipePartitionTable(util_HardDisk *hdd) {
    bool success = true;
    uint8_t dummySector[512];

    QI_ASSERT(hdd != NULL);

    memset(dummySector, 0, sizeof(dummySector));

    // Wiping the first 34 sectors is enough to make cf not recognize GPT anymore
    for (size_t sectorIndex = 0; sectorIndex < 34; sectorIndex++) {
        success &= util_writeSectorToDisk(hdd, sectorIndex, dummySector);
    }

    sync();

    return success;
}

static bool util_modifyBootSector(util_Partition *part, const util_BootSectorModifier *modifierList) {
    uint8_t *sector = NULL;
    size_t oldSectorIndex = 0;
    bool success = true;

    QI_ASSERT(part != NULL);
    QI_ASSERT(modifierList != NULL);

    // ReplacementData == NULL -> End of list marker
    while (modifierList->replacementData != NULL) {
        const util_BootSectorModifier *mod = modifierList;

        // New sector index found in the list = we need to write previous sector and free its databuffer
        if (sector && (mod->sectorIndex != oldSectorIndex)) {
            success = util_writeSectorToPartition(part, oldSectorIndex, sector);
            free (sector);
            sector = NULL;
        }

        // If we need to read a new sector
        if (sector == NULL) {
            sector = util_readSectorFromPartitionAllocate(part, mod->sectorIndex);
            oldSectorIndex = mod->sectorIndex;
            success &= (sector != NULL);
        }

        QI_ASSERT(success && "modifyBootSector failed");

        if (!success) {
            free(sector);
            return false;
        }

        // Copy the replacement data therefore injecting the boot sector code
        memcpy(sector + mod->offset, mod->replacementData, mod->length);

        // Next modifier
        modifierList++;
    }    

    // If there is a pending sector write left, execute it now
    if (sector) {
        success = util_writeSectorToPartition(part, oldSectorIndex, sector);
        free(sector);
    }

    return success;
}

bool util_modifyAndwriteBootSectorToPartition(util_Partition *part, const util_BootSectorModifier *modifierList) {
    bool result = util_modifyBootSector(part, modifierList);

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
    // Check if partition is mounted using /proc/self/mounts
    char devStringToCompare[UTIL_HDD_DEVICE_STRING_LENGTH+1] = "";

    // you could have a lot of logical  partitions so it's best to check for that PLUS a space.
    sprintf(devStringToCompare, "%s ", part->device);

    util_CommandOutput *mountLines = util_commandOutputCapture("cat /proc/self/mounts");
    QI_ASSERT(mountLines != NULL);

    bool ret = false;

    for (size_t i = 0; i < mountLines->lineCount; i++) {
        if (util_stringStartsWith(mountLines->lines[i], devStringToCompare)) {
            ret = true;
            break;
        }
    }

    util_commandOutputDestroy(mountLines);

    return ret;
}

bool util_getFormatCommand(util_Partition *part, util_FileSystem fs, char *buf, size_t bufSize) {
    QI_ASSERT(part);

    if (util_isPartitionMounted(part)) {
        util_unmountPartition(part);
    }

    if (fs == fs_fat16) {
        snprintf(buf, bufSize, "mkfs.fat -v -S %d -F 16 %s", part->sectorSize, part->device); // no clue what win9x wants to see here tbh...
    } else if (fs == fs_fat32) {
        snprintf(buf, bufSize, "mkfs.fat -v -S %d -s 8 -R 32 -f 2 -F 32 %s", part->sectorSize, part->device);
    } else {
        QI_ASSERT(false && "Wrong file system");
    }
    // Set partition file system to new file system
    part->fileSystem = fs;    
    return true;
}
