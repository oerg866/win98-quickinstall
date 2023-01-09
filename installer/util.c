/*
 * LUNMERCY - Utility functionality
 * (C) 2023 Eric Voirin (oerg866@googlemail.com)
 */

#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <linux/msdos_fs.h>
#include <sys/ioctl.h>

/* Utility Functions */

uint64_t util_getProcMeminfoValue(const char *key) {
    FILE *meminfo = fopen("/proc/meminfo", "r");
    uint64_t ret = 0;
    char fmt[1024];
    char line[1024] = {0};
    sprintf(fmt, "%s: %s", key, "%llu kB\n");
    while (!ferror(meminfo) && !feof(meminfo) && sscanf(line, fmt, &ret) < 1)
        fgets(line, sizeof(line), meminfo);
    fclose(meminfo);
    return (uint64_t) ret;
}

size_t util_getCommandOutputLineCount(const char *command) {
    return util_captureCommandOutput(command, NULL, 0);
}

size_t util_captureCommandOutput(const char *command, char *buf, size_t bufSize) {
    // This is hard to make, for now we just ignore bufSize... idk mannenen
    FILE* pipe = popen(command, "r");
    char line[1024];
    char *writePtr = buf;
    size_t lineCount = 0;
    size_t lineLength;
    while (fgets(line, sizeof(line), pipe) != NULL) {
        if (buf && bufSize) { // If buf is NULL, this command can be used to get the line count of the output.
            lineLength = strnlen(line, bufSize);
            strncpy(writePtr, line, bufSize);
            writePtr += lineLength;        
            bufSize -= lineLength;    
        }
        lineCount++;
    }
    pclose(pipe);
    return lineCount;
}

bool util_stringStartsWith(const char *fullString, const char *toCheck) {
    // Check for NULLs and the full string being too short
    if (!fullString || !toCheck || (strlen(fullString) < strlen(toCheck)))
        return false;

    // Return result
    return (memcmp(fullString, toCheck, strlen(toCheck)) == 0);
}

bool util_stringEquals(const char* str1, const char* str2) {
    return strcmp(str1, str2) == 0;
}

void util_stringReplaceChar(char *str, char oldChar, char newChar) {
    while (*str != 0x00) {
        *str = (*str == oldChar) ? newChar : *str;
        str++;
    }
}

char *util_endOfString(char *str) {
    return strchr(str, '\0');
}

void util_hexDump(const uint8_t *buf, size_t offset, size_t length) {
    size_t rows = length / 16;
    size_t lastRow = length % 16;

    for (uint32_t row = 0; row <= rows; row++) {
        size_t rowOffset = offset + row * 16;
        uint32_t cols = (row == rows) ? lastRow : 16;

        if (!cols) break;

        printf("%04x: ", rowOffset);

        for (uint32_t col = 0; col < cols; col++)
            printf("%02x ", buf[rowOffset + col]);

        printf(" | ");
        
        for (uint32_t col = 0; col < cols; col++) {
            char curByte = buf[rowOffset + col];
            putchar((curByte < 0x20) ? '.' : curByte);
        }            

        printf("\n");
    }
}

uint16_t util_getUInt16fromBuffer(const uint8_t *buf, size_t offset) {
    return *((uint16_t *) (buf + offset));
}

uint32_t util_getUInt32fromBuffer(const uint8_t *buf, size_t offset) {
    return *((uint32_t *) (buf + offset));
}

// Convet DOS time to Unix Time and return this in a time_t
time_t util_dosTimeToUnixTime(uint16_t dosDate, uint16_t dosTime) {
    struct tm tmValue;
    tmValue.tm_sec  = (dosTime & 0x1F) * 2;
    tmValue.tm_min  = (dosTime >> 5) & 0x3F;
    tmValue.tm_hour = (dosTime >> 11) & 0x1F;
    tmValue.tm_mday = dosDate & 0x1F;
    tmValue.tm_mon  = ((dosDate >> 5) & 0xF) - 1;
    tmValue.tm_year = (dosDate >> 9) + 1980 - 1900;
    tmValue.tm_isdst = -1;
    return mktime(&tmValue);
}

static bool util_setFileTime(int fd, time_t time) {
    struct timeval newTimes[2] = {{time, 0}, {time, 0}};
    int result = futimes(fd, newTimes);
//    printf("futimes(fd, %lld) = %d\n", time, result);
    return result == 0;
}

bool util_setDosFileTime(int fd, uint16_t dosDate, uint16_t dosTime) {
    return util_setFileTime(fd, util_dosTimeToUnixTime(dosDate, dosTime));
}

bool util_readFirstLineFromFileIntoBuffer(const char *filename, char *dest) {
    FILE *fp = fopen(filename, "r");
    fscanf(fp, "%[^\n]", dest);
    fclose(fp);
    return fp != NULL;
}

bool util_setDosFileAttributes(int fd, int attributes) {
    int ret = ioctl(fd, FAT_IOCTL_SET_ATTRIBUTES, &attributes);
    return ret == 0;
}

mode_t util_dosFileAttributeToUnixMode(uint8_t dosFlags) {
    mode_t ret = 0;
    if (dosFlags & ATTR_DIR)    // The file is a directory
        ret |= S_IFDIR;
    else                    // The file is a regular file
        ret |= S_IFREG;

    // Set the read, write, and execute permissions based on the DOS flags
    if (dosFlags & ATTR_RO)    // The file is read-only
        ret |= S_IRUSR | S_IRGRP | S_IROTH;
    else                    // The file is read-write
        ret |= S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;

    if (dosFlags & ATTR_SYS)    // The file is a "System file"
        ret |= S_ISGID;

    if (dosFlags & ATTR_HIDDEN)    // The file is hidden
        ret |= S_ISVTX;

    return ret;
}