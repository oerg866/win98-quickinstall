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

static inline void mappedFile_advancePosAndReadAhead(mappedFile *file, size_t len) {
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

mappedFile *mappedFile_open(const char *filename, size_t readahead) {
    mappedFile *file = calloc(1, sizeof(mappedFile));

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

void mappedFile_close(mappedFile *file) {
    munmap(file->mem, file->size);
    close(file->fd);
    free(file);
}

bool mappedFile_copyToFile(mappedFile *file, int outfd, size_t size, bool advancePosition) {
    ssize_t written = write(outfd, file->mem+file->pos, size);

    if (written < 0 || (size_t)written != size) {
        printf("IO Error!\n");
        perror(__func__);
        assert(false);
        return false;
    }

    if (advancePosition)
        mappedFile_advancePosAndReadAhead(file, written);

    return true;
}

bool mappedFile_read(mappedFile *file, void *dst, size_t len) {
    if (mappedFile_available(file) >= len) {
        memcpy(dst, file->mem+file->pos, len);
        mappedFile_advancePosAndReadAhead(file, len);
        return true;
    } else {
        return false;
    }
}