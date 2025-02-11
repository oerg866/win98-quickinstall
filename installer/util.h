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
#define UTIL_CMD_OUTPUT_LINE_LENGTH (1024)
#define UTIL_HDD_DEVICE_STRING_LENGTH (16+1)
#define UTIL_HDD_MODEL_STRING_LENGTH (64+1)

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
    size_t indexOnParent;
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
    size_t modifierCount;   // there must be a more elegant way to do this.. bleh
    const util_BootSectorModifier *modifiers;
    size_t bootStrapCodeSectorCount;
    const uint8_t *bootStrapCode; // For Windows 2000+, stored at the first sector after
    const uint8_t *mbrCode; // MBR code, this MUST NOT BE NULL
} util_BootSectorModifierList;

typedef struct {
    size_t lineCount;
    int returnCode;
    char **lines;
} util_CommandOutput;

#define util_arraySize(array) (sizeof((array))/sizeof((array)[0]))

#define __UTIL__STRINGIFY(X) #X
#define util_stringify(X) __UTIL__STRINGIFY(X)

#define util_returnOnNull(ptr, return_value) if (ptr == NULL) { printf("ERROR - '" #ptr "' is NULL! Result = '" #return_value "'\r\n"); return return_value; }

#define DISK_MBR_CODE_LENGTH (446)

// Get a value for a given key from /proc/meminfo
uint64_t util_getProcMeminfoValue(const char *key);

// Gets safe free amount of memory the system has at current time in bytes. 
uint64_t util_getProcSafeFreeMemory(void);

// Returns the stdout output of a command. Call commandOutputDestroy after use. Returns NULL in case of errors.
util_CommandOutput *util_commandOutputCapture(const char *command);
// Free a CommandOutput structure
void util_commandOutputDestroy(util_CommandOutput *co);

// Allocates and gets a list of all hard drives with partitions. Don't forget to call util_hardDiskArrayDestroy!
util_HardDiskArray *util_getSystemHardDisks(void);
// Deallocates a hard disk array including all internal data structures
void util_hardDiskArrayDestroy(util_HardDiskArray *hdds);

// Converts a MBR patition type byte to an util_FileSystem enum value
util_FileSystem util_partitionTypeByteToUtilFilesystem(uint8_t partitionType);
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

// Gets the command to format a partition. Returns false if there was an error, such as unsupported filesystem.
bool util_getFormatCommand(util_Partition *part, util_FileSystem fs, char *buf, size_t bufSize);

// Gets the index of the disk in 'hdds' that has the device string 'str'. Returns SIZE_MAX if not found.
size_t util_getHardDiskArrayIndexFromDevicestring(util_HardDiskArray *hdds, const char *str);
// Gets the partition 
util_Partition *util_getPartitionFromDevicestring(util_HardDiskArray *hdds, const char *str);
// Gets the n-th partition in the entire hard disk array.
util_Partition *util_getPartitionFromIndex(util_HardDiskArray *hdds, size_t index);


/* Disk IO functions */

// Reads a sector from a physical disk into an existing buffer
bool util_readSectorFromDisk(util_HardDisk *hdd, size_t sector, uint8_t *buf);
// Reads a sector from a partition on a disk into an existing buffer
bool util_readSectorFromPartition(util_Partition *part, size_t sector, uint8_t *buf);
// Reads a sector from a physical disk into a newly allocated buffer
uint8_t *util_readSectorFromDiskAllocate(util_HardDisk *hdd, size_t sector);
// Reads a sector from a partition on a disk into a newly allocated buffer
uint8_t *util_readSectorFromPartitionAllocate(util_Partition *part, size_t sector);

// Writes a new MBR to a physical disk (FDISK /MBR equivalent)
bool util_writeMBRToDrive(util_HardDisk *hdd, const uint8_t *newMBRCode);
// Writes a modified (new) Boot Sector to a partition on a disk
bool util_modifyBootSector(util_Partition *part, const util_BootSectorModifierList *modifierList);
// Writes a modified (new) Boot Sector + overwrite backup boot sector + write boot strap code
bool util_modifyAndWriteBootSectorToPartition(util_Partition *part, const util_BootSectorModifierList *modifierList);

/* File IO functions*/

// Convert DOS time to Unix Time and then apply it to an open file descriptor
bool util_setDosFileTime(int fd, uint16_t dosDate, uint16_t dosTime);
// Sets an open file's attributes
bool util_setDosFileAttributes(int fd, uint32_t attributes);
// Checks if a file exists.
bool util_fileExists(const char *filename);

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
bool util_readFirstLineFromFileIntoBuffer(const char *filename, char *dest, size_t bufSize);

// Converts a DOS date/time to a unix epoch time stamp
time_t util_dosTimeToUnixTime(uint16_t dosDate, uint16_t dosTime);
// Converts a DOS Flag byte to a mode_t for use with chmod or somesuch
mode_t util_dosFileAttributeToUnixMode(uint8_t dosFlags);

// Copies a file with FAT32 file system attributes
bool util_copyFile(const char *src, const char *dst);
#endif