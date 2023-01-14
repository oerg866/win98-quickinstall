#ifndef MPAK_LIB_H
#define MPAK_LIB_H

#include "platform.h"

typedef struct DirList DirList;
typedef struct FileList FileList;

typedef struct {
    uint32_t dirCount;
    uint32_t fileCount;
    char *outputFileName;
    FILE *outputFile;
    FileList *files;
    DirList *dirs;
    FileList *filesTail;
    DirList *dirsTail;
} MercyPackContext;

#define MERCYPAK_MAGIC_NUMBER "ZIEG"

MercyPackContext *mp_MercyPackContext_init(char *targetFileName);
void mp_MercyPackContext_destroy(MercyPackContext *ctx);

bool mp_MercyPackContext_addPath(MercyPackContext *ctx, const char* path);
bool mp_MercyPackContext_doPackAll(MercyPackContext *ctx);

#endif