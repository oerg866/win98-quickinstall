/*
 * LUNMERCY
 * Mapped File Reader - Single threaded version
 *
 * Function summary: 
 * I honestly can't really remember what I did here, this is all very weird black magic with the mmap.
 * I thought it'd do asynchronous readaheads, but this is actually not true. So this is a very complex
 * way to do very basic single threaded file I/O. LOL.
 * 
 * (C) 2023 Eric Voirin (oerg866@googlemail.com)
 */

#include "mappedfile.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/param.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#define MEM_PAGE_SIZE (4096)
#define BITMASK_PAGE (~(MEM_PAGE_SIZE - 1))

#define __INLINE__ inline __attribute__((always_inline))

typedef struct MappedFile {
    int fd;
    size_t size;
    size_t pos;
    uint8_t *mem;
} MappedFile;

static inline void mappedFile_advancePosAndReadAhead(MappedFile *file, size_t len) {
    size_t oldPage = file->pos & BITMASK_PAGE;
    size_t newPage = (file->pos + len) & BITMASK_PAGE;
    size_t adviseLen = newPage - oldPage;

    file->pos += len;

    if ((oldPage != newPage) && ((file->pos + adviseLen ) <= file->size)) {
        if (madvise(file->mem + newPage, adviseLen, MADV_SEQUENTIAL | MADV_WILLNEED) != 0) {
            perror(__func__);
            assert(false && "madvise failed");
        }
    }
}

MappedFile *mappedFile_open(const char *filename, size_t readahead, MappedFile_ErrorCallback callback) {
    MappedFile *file = calloc(1, sizeof(MappedFile));

    (void) callback; // Callback is unused in this version of the mapped file.

    file->fd = open(filename, O_RDONLY);

    assert (file->fd >= 0);

    if (file->fd < 0) {
        printf("Error opening file %s \n", filename);
        free(file);
        return NULL;
    }

    ssize_t fileSize = (ssize_t) lseek(file->fd, 0, SEEK_END);

    assert (fileSize > 0);

    file->size = fileSize;

    lseek(file->fd, 0, SEEK_SET);

    file->mem = mmap(NULL, fileSize, PROT_READ, MAP_SHARED, file->fd, 0);

    assert (file->mem);

    if (file->size > MEM_PAGE_SIZE) {
        if (madvise(file->mem, MIN(readahead, file->size), MADV_SEQUENTIAL | MADV_WILLNEED) != 0) {
            perror(__func__);
            assert(false && "madvise failed");
        }
    }

    if (!file->mem) {
        perror(__func__);
        assert(false && "mmap failed");
    }

    return file;
}

void mappedFile_close(MappedFile *file) {
    munmap(file->mem, file->size);
    close(file->fd);
    free(file);
}

bool mappedFile_copyToFiles(MappedFile *file, size_t FileCount, int *outfds, size_t len) {
    for (size_t i = 0; i < FileCount; i++) {
        ssize_t written = write(outfds[i], file->mem+file->pos, len);

        if (written < 0 || (size_t)written != len) {
            printf("IO Error!\n");
            perror(__func__);
            assert(false);
            return false;
        }

    }

    mappedFile_advancePosAndReadAhead(file, len);

    return true;
}

static __INLINE__ size_t mappedFile_available(MappedFile *file) {
    return (file->size - file->pos);
}

bool mappedFile_read(MappedFile *file, void *dst, size_t len) {
    if (mappedFile_available(file) >= len) {
        memcpy(dst, file->mem+file->pos, len);
        mappedFile_advancePosAndReadAhead(file, len);
        return true;
    } else {
        return false;
    }
}

__INLINE__ bool mappedFile_eof(MappedFile *file) {
    return file->pos >= file->size;
}
__INLINE__ bool mappedFile_getUInt8(MappedFile *file, uint8_t *dst)  {
    return mappedFile_read(file, dst, sizeof(uint8_t)); 
}
__INLINE__ bool mappedFile_getUInt16(MappedFile *file, uint16_t *dst) {
    return mappedFile_read(file, dst, sizeof(uint16_t)); 
}
__INLINE__ bool mappedFile_getUInt32(MappedFile *file, uint32_t *dst) {
    return mappedFile_read(file, dst, sizeof(uint32_t)); 
}
__INLINE__ size_t mappedFile_getFileSize(MappedFile *file) {
    return file->size;
}
__INLINE__ size_t mappedFile_getPosition(MappedFile *file) {
    return file->pos;
}
