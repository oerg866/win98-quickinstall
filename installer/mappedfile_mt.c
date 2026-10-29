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

    // Everything below is shared between READER and CONSUMER threads.
    // It is protected by "lock".
    // stateChanged is signaled whenever any of it changes
    // this removes the previous busy-wait implementation
    // (I really hope i got this right...)
    pthread_mutex_t lock;
    pthread_cond_t stateChanged;

    bool closing;
    bool readaheadComplete;

    bool hasError;
    int mErrno;
    MappedFile_ErrorReaction errorReaction;
    MappedFile_ErrorCallback errorCallback;

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

// Must be called with the lock held
static __INLINE__ void mappedFile_wait(MappedFile *mf) {
    pthread_cond_wait(&mf->stateChanged, &mf->lock);
}

// Must be called with the lock held
static __INLINE__ void mappedFile_signal(MappedFile *mf) {
    pthread_cond_broadcast(&mf->stateChanged);
}

// Must be called with the lock held
static __INLINE__ void mappedFile_readErrorAcknowledge(MappedFile *mf, MappedFile_ErrorReaction errorReaction) {
    mf->hasError = false;
    mf->errorReaction = errorReaction;
    mappedFile_signal(mf);
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
    // The reader thread may be waiting for space
    mappedFile_signal(mf);
    mappedFile_unlock(mf);
}

static __INLINE__ mappedFile_MemBlock *mappedFile_getCurrentBlock(MappedFile *file) {
    mappedFile_lock(file);
    mappedFile_MemBlock *block = file->memFirst;
    mappedFile_unlock(file);
    return block;
}

static __INLINE__ mappedFile_MemBlock *mappedFile_waitForValidBlockAndGet(MappedFile *file) {
    // No more data, wait until reader thread has some
    // either over the threshold or the rest of the file if it was read completely
    size_t blockThreshold = MAX(MIN(file->maxBlocks / 2, 8), 1);
    mappedFile_MemBlock *block;

    mappedFile_lock(file);

    if (file->blockCount == 0) {
        while (file->blockCount < blockThreshold && file->readaheadComplete == false) {
            // We may have a read error.
            // The callback is run on the consumer thread, so we won't have concurrency
            // issues with the UI, but it must run without the lock held since it can take a while.
            if (file->hasError) {
                int readErrno = file->mErrno;
                MappedFile_ErrorReaction action = MF_RETRY;

                mappedFile_unlock(file);
                if (file->errorCallback != NULL)  {
                    action = file->errorCallback(readErrno, file);
                }
                mappedFile_lock(file);

                mappedFile_readErrorAcknowledge(file, action);

                if (action == MF_CANCEL) {
                    mappedFile_unlock(file);
                    return NULL;
                }
                continue;
            }

            mappedFile_wait(file);
        }
    }

    block = file->memFirst;
    mappedFile_unlock(file);
    return block;
}

static __INLINE__ bool mappedFile_readInternal(int fd, uint8_t *mem, size_t toRead) {
    while (toRead) {
        ssize_t bytesRead = read(fd, mem, toRead);
        if (bytesRead <= 0) {            
            return false;
        }

        toRead -= (size_t) bytesRead;
        mem    += (size_t) bytesRead;
    }
    return true;
}

static __INLINE__ bool mappedFile_readAhead1Block(MappedFile *mf) {
    size_t toRead = mf->size - mf->readaheadPos;
    toRead = MIN(toRead, MEM_BLOCK_SIZE);

    if (toRead == 0) return true;
    mappedFile_MemBlock *block = malloc(sizeof(mappedFile_MemBlock));

    // Running out of memory is reported like a read error, so the user can retry or cancel
    if (block == NULL) {
        errno = ENOMEM;
        return false;
    }

    block->next = NULL;

    // Catch read errors
    if (!mappedFile_readInternal(mf->fd, block->mem, toRead)) {
        // No memleaks please (and keep the errno of the failed read)
        int readErrno = errno;
        free(block);
        errno = readErrno;
        return false;
    }

    mappedFile_lock(mf);
    mf->readaheadPos += toRead;
    mf->blockCount += 1;
    if (mf->memFirst == NULL) mf->memFirst = block;
    if (mf->memLast != NULL) mf->memLast->next = block;
    mf->memLast = block;
    // The consumer may be waiting for data
    mappedFile_signal(mf);
    mappedFile_unlock(mf);

    return true;
}

// Must be called with the lock held
static MappedFile_ErrorReaction mappedFile_readAheadHandleError(MappedFile *mf, int readErrno) {
    // First, we need to actually flag the error internally. We only do this here, the hasError variable is ONLY for the consumer!!
    mf->mErrno = readErrno;
    mf->errorReaction = MF_NONE;
    mf->hasError = true;
    mappedFile_signal(mf);

    // Wait for the consumer to tell us what to do now... unless the file gets closed in the meantime.
    while (mf->errorReaction == MF_NONE && mf->closing == false) {
        mappedFile_wait(mf);
    }

    return mf->closing ? MF_CANCEL : mf->errorReaction;
}

static void *mappedFile_threadFunc(void *param) {
    MappedFile *mf = (MappedFile *) param;

    mappedFile_lock(mf);

    while (mf->closing == false && mf->readaheadPos < mf->size) {
        // Are we full?
        if (mf->blockCount >= mf->maxBlocks) {
            // Sleep until the consumer finishes a block (or closes the file)
            mappedFile_wait(mf);
            continue;
        }

        // Read without locking, so the consumer can keep...consuming
        mappedFile_unlock(mf);
        bool readOk = mappedFile_readAhead1Block(mf);
        int readErrno = errno;
        mappedFile_lock(mf);

        // Handle an error condition
        if (!readOk) {
            MappedFile_ErrorReaction action = mappedFile_readAheadHandleError(mf, readErrno);
            assert(action != MF_NONE);

            if (action == MF_CANCEL) {
                break;
            }
            // Else, we retry... Reset the position and seek backwards.
            lseek(mf->fd, mf->readaheadPos, SEEK_SET);
        }
    }

    mf->readaheadComplete = true;
    mappedFile_signal(mf);
    mappedFile_unlock(mf);

    pthread_exit(param);
}

MappedFile *mappedFile_open(const char *filename, size_t readahead, MappedFile_ErrorCallback errorCallback) {
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
    // At least one block, otherwise the reader thread would never read anything
    file->maxBlocks = MAX(readahead / MEM_BLOCK_SIZE, 1);
    file->errorCallback = errorCallback;

    if (0 != pthread_mutex_init(&file->lock, NULL)
     || 0 != pthread_cond_init(&file->stateChanged, NULL)
     || 0 != pthread_create(&file->thread, NULL, mappedFile_threadFunc, (void*) file)) {
        printf("Error starting reader thread for file %s \n", filename);
        close(file->fd);
        free(file);
        return NULL;
    }

    return file;
}

void mappedFile_close(MappedFile *file) {
    // Wake the reader thread up
    // it could be waiting for buffer space or for read error acknowledgement
    mappedFile_lock(file);
    file->closing = true;
    mappedFile_signal(file);
    mappedFile_unlock(file);

    pthread_join(file->thread, NULL);
    while (mappedFile_getCurrentBlock(file) != NULL) {
        mappedFile_disposeBlock(file);
    };

    pthread_mutex_destroy(&file->lock);
    pthread_cond_destroy(&file->stateChanged);
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

        // Asked for more data than the file has left (truncated or corrupt file)
        if (toCopy == 0) {
            return false;
        }

        mappedFile_MemBlock *currentBlock = mappedFile_waitForValidBlockAndGet(file);

        // This means the read error was handed with the "cancel" action
        if (currentBlock == NULL) {
            return false;
        }

        memcpy(dst, currentBlock->mem + positionInBlock, toCopy);

        // Reads can span blocks, the next part goes after this one
        dst = (uint8_t *) dst + toCopy;
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

        // Asked for more data than the file has left (truncated or corrupt file)
        if (toCopy == 0) {
            return false;
        }

        mappedFile_MemBlock *currentBlock = mappedFile_waitForValidBlockAndGet(file);

        // This means the read error was handed with the "cancel" action
        if (currentBlock == NULL) {
            return false;
        }


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
