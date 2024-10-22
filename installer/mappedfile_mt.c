/*
 * LUNMERCY
 * Mapped File Reader - Multithreaded version (experimental)
 *
 * Function summary: 
 * There is a reader thread that constantly reads data from the input file and stores it in 1 Megabyte chunks.
 * The chunks form a linked list.
 *
 * Files are created with a readahead parameter that contains the amount of bytes that can safely be held in memory.
 *
 * It's up to the caller to figure this out.
 * 
 * (C) 2024 Eric Voirin (oerg866@googlemail.com)
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
#include <pthread.h>
#include <errno.h>

#define MEM_BLOCK_SIZE (1 * 1024 * 1024)

#define __INLINE__ inline __attribute__((always_inline))

typedef struct mappedFile_MemBlock {
    uint8_t mem[MEM_BLOCK_SIZE];
    struct mappedFile_MemBlock *next;
} mappedFile_MemBlock;

typedef struct MappedFile {
    int fd;
    size_t size;
    size_t pos;
    size_t readaheadPos;
    pthread_t thread;
    pthread_mutex_t lock;
    
    bool closing;
    bool readaheadComplete;
    
    size_t blockCount;
    size_t maxBlocks;
    mappedFile_MemBlock *memFirst;
    mappedFile_MemBlock *memLast;
} MappedFile;

static __INLINE__ void mappedFile_lock(MappedFile *mf) {
    pthread_mutex_lock(&mf->lock);
}

static __INLINE__ void mappedFile_unlock(MappedFile *mf) {
    pthread_mutex_unlock(&mf->lock);
}


// Dispose of the first (i.e. oldest) memory block
static __INLINE__ void mappedFile_disposeBlock(MappedFile *mf) {
    mappedFile_lock(mf);
    mappedFile_MemBlock *toDispose = mf->memFirst;

    // Not doing NULL check here, because this function is only called internally when there's a valid block to dispose

    mf->memFirst = mf->memFirst->next;
    free(toDispose);
    mf->blockCount -= 1;
    if (mf->blockCount == 0) {
        mf->memLast = NULL;
    }
    mappedFile_unlock(mf);
}

static __INLINE__ mappedFile_MemBlock *mappedFile_getCurrentBlock(MappedFile *file) {
    mappedFile_lock(file);
    mappedFile_MemBlock *block = file->memFirst;
    mappedFile_unlock(file);
    return block;
}

static __INLINE__ mappedFile_MemBlock *mappedFile_waitForValidBlockAndGet(MappedFile *file) {
    if (file->blockCount > 0) {
        return mappedFile_getCurrentBlock(file);
    }

    size_t blockThreshold = MIN(file->maxBlocks / 2, 8);

    while (file->blockCount < blockThreshold && file->readaheadComplete == false) {
        sched_yield();
    }
    return mappedFile_getCurrentBlock(file);
}

static __INLINE__ void mappedFile_readAhead1Block(MappedFile *mf) {
    size_t toRead = mf->size - mf->readaheadPos;
    toRead = MIN(toRead, MEM_BLOCK_SIZE);
    if (toRead == 0) return;
    mappedFile_MemBlock *block = malloc(sizeof(mappedFile_MemBlock));
    block->next = NULL;

    ssize_t bytesRead = read(mf->fd, block->mem, toRead);

    if (bytesRead < 0 || (size_t)bytesRead != toRead) {
        printf("Read error!\n");
        assert(false);
    }

    mappedFile_lock(mf);
    mf->readaheadPos += toRead;
    mf->blockCount += 1;
    if (mf->memFirst == NULL) mf->memFirst = block;
    if (mf->memLast != NULL) mf->memLast->next = block;
    mf->memLast = block;
    mappedFile_unlock(mf);
}

static void *mappedFile_threadFunc(void *param) {
    MappedFile *mf = (MappedFile *) param;
    
    while (mf->closing == false && mf->readaheadPos < mf->size) {
        sched_yield();
        if (mf->blockCount == mf->maxBlocks) {
            continue;
        }
        mappedFile_readAhead1Block(mf);
    }

    mf->readaheadComplete = true;
    pthread_exit(param);
}

MappedFile *mappedFile_open(const char *filename, size_t readahead) {
    MappedFile *file = calloc(1, sizeof(MappedFile));

    assert(file != NULL);

    file->fd = open(filename, O_RDONLY);

    if (file->fd < 0) {
        printf("Error opening file %s \n", filename);
        free(file);
        return NULL;
    }

    ssize_t fileSize = (ssize_t) lseek(file->fd, 0, SEEK_END);
    assert (fileSize > 0);
    lseek(file->fd, 0, SEEK_SET);

    
    file->size = fileSize;
    file->maxBlocks = readahead / MEM_BLOCK_SIZE;

    assert (0 == pthread_mutex_init(&file->lock, NULL));
    assert (0 == pthread_create(&file->thread, NULL, mappedFile_threadFunc, (void*) file));

    return file;
}

void mappedFile_close(MappedFile *file) {
    file->closing = true;

    pthread_join(file->thread, NULL);
    while (mappedFile_getCurrentBlock(file) != NULL) { 
        mappedFile_disposeBlock(file); 
    };

    pthread_mutex_destroy(&file->lock);
    close(file->fd);
    free(file);
}

bool mappedFile_read(MappedFile *file, void *dst, size_t len) {
    if (file->pos >= file->size) {
        return false;
    }

    while (len) {
        size_t positionInBlock = file->pos % MEM_BLOCK_SIZE;
        size_t leftInFile = file->size - file->pos;
        size_t leftInBlock = MEM_BLOCK_SIZE - positionInBlock;
        size_t maxIterationSize = MIN(leftInFile, leftInBlock);
        size_t toCopy = MIN(len, maxIterationSize);

        mappedFile_MemBlock *currentBlock = mappedFile_waitForValidBlockAndGet(file);
        memcpy(dst, currentBlock->mem + positionInBlock, toCopy);

        leftInBlock -= toCopy;
        len -= toCopy;
        file->pos += toCopy;

        if (leftInBlock == 0) {
            mappedFile_disposeBlock(file);
        }
    }

    return true;
}

// This code is very duplicated but IDK how to make this universal without costing some performance... :(
bool mappedFile_copyToFiles(MappedFile *file, size_t fileCount, int *outfds, size_t len) {
    if (file->pos >= file->size) {
        return false;
    }

    while (len) {
        size_t positionInBlock = file->pos % MEM_BLOCK_SIZE;
        size_t leftInFile = file->size - file->pos;
        size_t leftInBlock = MEM_BLOCK_SIZE - positionInBlock;
        size_t maxIterationSize = MIN(leftInFile, leftInBlock);
        size_t toCopy = MIN(len, maxIterationSize);

        mappedFile_MemBlock *currentBlock = mappedFile_waitForValidBlockAndGet(file);

        for (size_t i = 0; i < fileCount; i++) {
            ssize_t written = write(outfds[i], currentBlock->mem + positionInBlock, toCopy);

            if (written < 0 || (size_t)written != toCopy) {
#ifdef DEBUG
                printf("IO Error: %s!\n", strerror(errno));
#endif
                return false;
            }
        }

        leftInBlock -= toCopy;
        len -= toCopy;
        file->pos += toCopy;

        if (leftInBlock == 0) {
            mappedFile_disposeBlock(file);
        }
    }

    return true;
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
