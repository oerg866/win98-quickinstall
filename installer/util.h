#ifndef UTIL_H
#define UTIL_H

/*
 * LUNMERCY - Utility functionality
 * (C) 2023 Eric Voirin (oerg866@googlemail.com)
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <sys/param.h>
#include <sys/types.h>

#define UTIL_MAX_CMD_LENGTH (2048)


// Get a value for a given key from /proc/meminfo
uint64_t util_getProcMeminfoValue(const char *key);

// Gets safe free amount of memory the system has at current time in bytes. 
#define util_getProcSafeFreeMemory() (MIN(util_getProcMeminfoValue("MemFree"), util_getProcMeminfoValue("MemAvailable")) * 1024UL)

size_t util_getCommandOutputLineCount(const char *command);
size_t util_captureCommandOutput(const char *command, char *buf, size_t bufSize);

#define UTIL_HDD_DEVICE_STRING_LENGTH (16)
#define UTIL_HDD_MODEL_STRING_LENGTH (64)

// this enum shows the file system of a partition
typedef enum {
    fs_none = 0,
    fs_unsupported,
    fs_fat16,
    fs_fat32,
    FS_ENUM_SIZE
} util_FileSystem;

// this struct models a partition on a hard drive
typedef struct {
    char device[UTIL_HDD_DEVICE_STRING_LENGTH];
    uint64_t size;
    uint32_t sectorSize;
    util_FileSystem fileSystem;
    struct util_HardDisk *parent;
    int index;
    char *mountPath;
} util_Partition;

// this struct models a hard drive
typedef struct util_HardDisk {
    char device[UTIL_HDD_DEVICE_STRING_LENGTH];
    char model[UTIL_HDD_MODEL_STRING_LENGTH];
    uint64_t size;
    uint32_t sectorSize;
    uint32_t optIoSize;    
    size_t partitionCount;
    util_Partition *partitions;
} util_HardDisk;

typedef struct {
    size_t count;
    util_HardDisk *disks;    
} util_HardDiskArray;

// This struct models a part of a boot sector that is to be overwritten with parts of data blocks
typedef struct {
    size_t sectorIndex;
    size_t offset;
    size_t length;
    const uint8_t *replacementData;
} util_BootSectorModifier;

typedef struct {
    size_t count;   // there must be a more elegant way to do this.. bleh
    const util_BootSectorModifier *modifiers;
} util_BootSectorModifierList;

// Allocates and gets a list of all hard drives with partitions. Don't forget to call util_HardDiskArrayDeinit!
util_HardDiskArray util_getSystemHardDisks();
// Deallocates a hard disk array including all internal data structures
void util_hardDiskArrayDeinit(util_HardDiskArray hdds);

// Converts a MBR patition type byte to an util_FileSystem enum value
util_FileSystem util_partitionTypeByteToUtilFilesystem(uint8_t partitionType);
// Converts a lsblk FSTYPE string to an util_FileSystem enum value
util_FileSystem util_lsblkFsStringToUtilFilesystem(const char *device, const char* fsString);
// Converts an util_FileSystem enum value to a string
const char *util_utilFilesystemToString(util_FileSystem fs);
// Gets the short version of a device string (after the last /, so /dev/sda1 becomes sda1)
const char *util_shortDeviceString(const char *str);

// Mounts a partition.
bool util_mountPartition(util_Partition *part);
// Unmounts a partition.
bool util_unmountPartition(util_Partition *part);

// Checks if a partition is currently mounted.
bool util_isPartitionMounted(util_Partition *part);

// Formats a partition
bool util_formatPartition(util_Partition *part, util_FileSystem fs);

// Gets the index of the disk in 'hdds' that has the device string 'str'. Returns SIZE_MAX if not found.
size_t util_getHardDiskArrayIndexFromDevicestring(util_HardDiskArray *hdds, const char *str);
// Gets the partition 
util_Partition *util_getPartitionFromDevicestring(util_HardDiskArray *hdds, const char *str);

/* Disk IO functions */

// Reads a sector from a physical disk into an existing buffer
bool util_readSectorFromDisk(util_HardDisk *hdd, size_t sector, uint8_t *buf);
// Reads a sector from a partition on a disk into an existing buffer
bool util_readSectorFromPartition(util_Partition *part, size_t sector, uint8_t *buf);
// Reads a sector from a physical disk into a newly allocated buffer
uint8_t *util_readSectorFromDiskAllocate(util_HardDisk *hdd, size_t sector);
// Reads a sector from a partition on a disk into a newly allocated buffer
uint8_t *util_readSectorFromPartitionAllocate(util_Partition *part, size_t sector);

// Writes a Windows 98 MBR to a physical disk (FDISK /MBR equivalent)
bool util_writeWin98MBRToDrive(util_HardDisk *hdd);
// Writes a Windows 98 Boot Sector to a partition on a disk (SYS.COM equivalent, sans copying system files)
bool util_writeWin98BootSectorToPartition(util_Partition *part);

/* File IO functions*/

// Dumb file copy routine... just a wrapper around the "cp" command...
bool util_copyFile(const char *src, const char *dst);
// Convert DOS time to Unix Time and then apply it to an open file descriptor
bool util_setDosFileTime(int fd, uint16_t dosDate, uint16_t dosTime);
// Sets an open file's attributes
bool util_setDosFileAttributes(int fd, int attributes);

/* String functions */

// checks if strings are equal, assumes the strings are VALID!!!
bool util_stringEquals(const char *str1, const char *str2);
// checks if strings are equal, handles NULLs, assumes not NULL strings are terminated properly!!!
bool util_stringStartsWith(const char *fullString, const char *toCheck);
// replaces every instance of a character with another in a string
void util_stringReplaceChar(char *str, char oldChar, char newChar);
// Gets a pointer to the end of a string (i.e. the null terminator)
char *util_endOfString(char *str);

/* Misc functions */

// Outputs a HEX / ASCII dump of a buffer, with an offset and length
void util_hexDump(const uint8_t *buf, size_t offset, size_t length);

// Gets an unsigned 16 bit value from a raw buffer
uint16_t util_getUInt16fromBuffer(const uint8_t *buf, size_t offset);
// Gets an unsigned 32 bit value from a raw buffer
uint32_t util_getUInt32fromBuffer(const uint8_t *buf, size_t offset);

// Reads the first line of a file into a buffer.
bool util_readFirstLineFromFileIntoBuffer(const char *filename, char *dest);

// Converts a DOS date/time to a unix epoch time stamp
time_t util_dosTimeToUnixTime(uint16_t dosDate, uint16_t dosTime);
// Converts a DOS Flag byte to a mode_t for use with chmod or somesuch
mode_t util_dosFileAttributeToUnixMode(uint8_t dosFlags);

#endif