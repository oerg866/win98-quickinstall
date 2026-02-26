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
#include <ctype.h>
#include <unistd.h>
#include <linux/msdos_fs.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <errno.h>

#include "qi_assert.h"

/* Utility Functions */

bool util_runningAsRoot(void) {
    return geteuid() == 0;
}

uint64_t util_getProcMeminfoValue(const char *key) {
    FILE *meminfo = fopen("/proc/meminfo", "r");
    uint64_t ret = 0;
    char fmt[1024];
    char line[1024] = {0};
    char *result = line;
    sprintf(fmt, "%s: %s", key, "%llu kB\n");
    while (!ferror(meminfo) && !feof(meminfo) && sscanf(line, fmt, &ret) < 1 && result)
        result = fgets(line, sizeof(line), meminfo);
    fclose(meminfo);
    return (uint64_t) ret;
}

inline uint64_t util_getProcSafeFreeMemory() {
    uint64_t commitLimit = util_getProcMeminfoValue("CommitLimit") * 1024ULL;
    uint64_t memAvailable = util_getProcMeminfoValue("MemAvailable") * 1024ULL;
    return MIN(commitLimit, memAvailable);
}

util_CommandOutput *util_commandOutputCapture(const char *command) {
    FILE* pipe = popen(command, "r");
    util_CommandOutput *ret = calloc(1, sizeof(util_CommandOutput));
    
    QI_ASSERT(ret != NULL);
    QI_ASSERT(pipe != NULL);

    while (true) {
        char *tmpLine = malloc(UTIL_CMD_OUTPUT_LINE_LENGTH);

        QI_ASSERT(tmpLine != NULL);

        // If fgets returns NULL, the program is finished and we need to discard this line
        if (NULL == fgets(tmpLine, UTIL_CMD_OUTPUT_LINE_LENGTH, pipe)) {
            free(tmpLine);
            ret->returnCode = WEXITSTATUS(pclose(pipe));
            return ret;
        }

        /* Append this line to the end of our line array*/

        ret->lineCount++;
        ret->lines = realloc(ret->lines, ret->lineCount * sizeof(ret->lines[0]));

        QI_ASSERT(ret->lines != NULL);

        ret->lines[ret->lineCount - 1] = tmpLine;
    }

    return ret;
}

void util_commandOutputDestroy(util_CommandOutput *co) {
    if (co) {
        if (co->lines) {
            for (size_t i = 0; i < co->lineCount; ++i) {
                free(co->lines[i]);
            }
            free(co->lines);
        }
        free(co);
    }
}

bool util_stringStartsWith(const char *fullString, const char *toCheck) {
    return strncmp(toCheck, fullString, strlen(toCheck)) == 0;
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

void util_getCappedString(char *dst, const char* src, size_t maxLen) {
    if (strlen(src) > maxLen) {
        memcpy(dst, src, maxLen - 3);
        memcpy(dst + maxLen - 3, "...", 3);
    } else {
        memcpy(dst, src, strlen(src));
    }
}

void util_stringInsert(char *dst, const char *src) {
    memcpy(dst, src, strlen(src));
}

void util_stringRTrim(char *str) {
    char *cur = &str[strlen(str) - 1];

    while (cur >= str && isspace(*cur)) {
            *cur = 0x00;
            cur--;
    }
}

void util_hexDump(const uint8_t *buf, size_t offset, size_t length) {
    size_t rows = length / 16;
    size_t lastRow = length % 16;

    for (uint32_t row = 0; row <= rows; row++) {
        size_t rowOffset = offset + row * 16;
        uint32_t cols = (row == rows) ? lastRow : 16;

        if (!cols) break;

        printf("%04lx: ", (unsigned long) rowOffset);

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
    return result == 0;
}

bool util_setDosFileTime(int fd, uint16_t dosDate, uint16_t dosTime) {
    return util_setFileTime(fd, util_dosTimeToUnixTime(dosDate, dosTime));
}

bool util_readFirstLineFromFileIntoBuffer(const char *filename, char *dest, size_t bufSize) {
    FILE *fp;
    char *result = NULL;

    util_returnOnNull(dest, false);

    fp = fopen(filename, "r");
    util_returnOnNull(fp, false);

    *dest = 0x00;
    result = fgets(dest, (int) bufSize, fp);

    fclose(fp);

    // fgets returning NULL means failure, and we also need a non-empty string
    if (result == NULL || strlen(dest) == 0) {
        return false;
    }

    util_stringReplaceChar(dest, '\n', 0x00);
    return true;
}

bool util_setDosFileAttributes(int fd, uint32_t attributes) {
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

bool util_fileExists(const char *filename) {
    return (access(filename, F_OK) == 0);
}

bool util_mkDir(const char *dirName, uint32_t dosFlags) {
    char tmp[PATH_MAX + 1] = {0};

    util_returnOnNull(dirName, false);

    strncpy(tmp, dirName, sizeof(tmp));

    // Skip beginning when parsing
    char *curDir = dirName[0] == '/' ? &tmp[1] : tmp;
    

    bool success = true;
    bool lastIteration = false;

    do {
        // if the input string has ended, run this loop one more time so that
        // we create the final directory
        lastIteration = (curDir[0] == 0x00);

        if (curDir[0] == '/' || lastIteration) {
            // Temporarily replace / with a null terminator
            curDir[0] = 0x00;

            // Create the actual directory now
            if (!util_fileExists(tmp)) {
                int result = mkdir(tmp, 0777);
                success &= (result == 0) || (result == EEXIST);
            }
            
            // Put the slash back
            curDir[0] = '/';
        }
        curDir++;
    } while (!lastIteration);

    // Handle the DOS dir flags
    // Remove DIR flag since that's read only in the ioctl
    dosFlags &= ~(ATTR_DIR);

    int fd = open(dirName, O_RDONLY | O_DIRECTORY);
    success &= fd >= 0;

    if (fd) {
        success &= 0 == ioctl(fd, FAT_IOCTL_SET_ATTRIBUTES, &dosFlags);
        close(fd);
    }

    return success;
}

bool util_isFile(const char *path) {
        struct stat st;
        return (0 == lstat(path, &st)) && S_ISREG(st.st_mode);
}

bool util_isDir(const char *path) {
        struct stat st;
        return (0 == lstat(path, &st)) && S_ISDIR(st.st_mode);
}

char *util_pathAppend(const char *basePath, const char *subPath) {
    util_returnOnNull(basePath, NULL);
    util_returnOnNull(subPath, NULL);
    char *target = malloc(strlen(basePath) + 1 + strlen(subPath) + 1); // base + '/' + sub + null terminator
    util_returnOnNull(target, NULL);
    sprintf(target, "%s/%s", basePath, subPath);
    return target;
}

size_t util_getFileCountRecursive(const char *baseDir) {
    struct dirent *e;
    DIR *d = opendir(baseDir);

    util_returnOnNull(d, 0);
    
    size_t count = 0;

    while ((e = readdir(d))) {
        if (util_stringEquals(e->d_name, ".") || util_stringEquals(e->d_name, ".."))
            continue;

        char *newPath = util_pathAppend(baseDir, e->d_name);

        QI_FATAL(newPath != NULL, "Failed to allocate path string");

        if      (util_isDir(newPath))   count += util_getFileCountRecursive(newPath);
        else if (util_isFile(newPath))  count += 1;

        free (newPath);
    }

    closedir(d);
    return count;
}

static size_t util_getFileSizeFromFd(int fd) {
    ssize_t fileSize = (ssize_t) lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    if (fileSize < 0) return 0;

    return (size_t) fileSize;
}

static inline __always_inline bool util_fileCopyWriteEnsure(int fd, uint8_t *buf, size_t size) {
    while (size) {
        ssize_t written = write(fd, buf, size);

        if (written <= 0) {
            return false;
        }

        buf += written;
        size -= written;
    }
    return true;
}

// Fallback for fileCopy if sendfile fails.
static bool util_fileCopyReadWrite(const char *source, const char *dest) {
    int in = open(source, O_RDONLY);
    int out = open(dest, O_WRONLY | O_CREAT | O_TRUNC, 0777);
    bool success = (in >= 0) && (out >= 0);

    if (!success) goto cleanup;

    size_t leftToRead = util_getFileSizeFromFd(in);
    
    while (success && (leftToRead > 0)) {
        uint8_t buf[4096];
        ssize_t hasRead = read(in, buf, sizeof(buf));

        leftToRead -= hasRead;

        if (hasRead < 0 || !util_fileCopyWriteEnsure(out, buf, hasRead)) {
            success = false;
        } else if (hasRead == 0) {
            break;
        }
    }

cleanup:
    close(in);
    close(out);
    return success;
}

bool util_fileCopy(const char *source, const char *dest) {
    int in = open(source, O_RDONLY);
    int out = open(dest, O_WRONLY | O_CREAT | O_TRUNC, 0777);
    bool success = (in >= 0) && (out >= 0);

    if (!success) goto cleanup;

    off_t offset = 0;
    size_t fileSize = util_getFileSizeFromFd(in);

    while (success && (size_t) offset < fileSize) {
        size_t toWrite = fileSize - offset;
        ssize_t written = sendfile(out, in, &offset, toWrite);
        if (written < 0) {
            success = false;
        }
    }

cleanup:
    close(in);
    close(out);

    if (!success) success = util_fileCopyReadWrite(source, dest);
    return success;
}