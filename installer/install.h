#ifndef INSTALL_H
#define INSTALL_H

/*
 * LUNMERCY - Installer component 
 * (C) 2023 Eric Voirin (oerg866@googlemail.com)
 */

#include <stdbool.h>

#include "util.h"
#include "mappedfile.h"

typedef struct {
    size_t          osVariantIndex;
    MappedFile     *sourceFile;
    util_Partition *destinationPartition;
    bool            installSuccess;
    bool            installCancel;
    size_t          readaheadSize;
} inst_Context;

typedef enum {
    INST_OK,
    INST_FILE_COPY_ERROR,
    
} inst_InstallStatus;

typedef bool (*inst_CheckPartitionFunc)(inst_Context *inst);
typedef bool (*inst_PrepareInstallFunc)(inst_Context *inst);
typedef inst_InstallStatus (*inst_DoInstallFunc)(inst_Context *inst);

typedef struct {
    inst_CheckPartitionFunc checkPartition;
    inst_PrepareInstallFunc prepareInstall;
    inst_DoInstallFunc install;
    const util_BootSectorModifierList *bootSectorModifiersFAT16;
    const util_BootSectorModifierList *bootSectorModifiersFAT32;
/*  util_BootSectorModifierList bootSectorModifiersNTFS;
    maybe some day :D */
} inst_OSInstaller;

const char *inst_getCDFilePath(size_t osVariantIndex, const char *filepath);
MappedFile *inst_openSourceFile(size_t osVariantIndex, const char *filename, size_t readahead);

bool inst_copyFiles(MappedFile *file, const char *installPath, const char *filePromptString);
bool inst_main();


#endif