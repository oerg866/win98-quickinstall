#include <stdio.h>
#include <string.h>

#include "install.h"
#include "qi_assert.h"
#include "util.h"
#include "mappedfile.h"

static char cdrompath[PATH_MAX+1] = {0};    // Path to install source media
static char cdromdev[PATH_MAX+1] = {0};     // Block device for install source media
                                            // ^ initialized in setSourceMedia, called by inst_main

static char staticSourceBuf[PATH_MAX+1] = {0};
static char staticTargetBuf[PATH_MAX+1] = {0};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"

const char *inst_getTargetFilePath(util_Partition *part, const char *path) {
    QI_FATAL(part->mountPath != NULL, "Partition not mounted");
    snprintf(staticTargetBuf, PATH_MAX, "%s/%s", part->mountPath, path);
    return staticTargetBuf;
}

const char *inst_getSourceFilePath(size_t osVariantIndex, const char *filepath) {
    if (osVariantIndex > 0) {
        snprintf(staticSourceBuf, PATH_MAX, "%s/osroots/%zu/%s", cdrompath, osVariantIndex, filepath);
    } else {
        snprintf(staticSourceBuf, PATH_MAX, "%s/%s", cdrompath, filepath);
    }
    return staticSourceBuf;
}

#pragma GCC diagnostic pop

void inst_setSourceMedia(const char* sourcePath, const char *sourceDev) {
    QI_ASSERT(sourcePath != NULL);
    QI_ASSERT(sourceDev != NULL);
    QI_ASSERT(strlen(sourcePath) < sizeof(cdrompath));
    QI_ASSERT(strlen(sourceDev) < sizeof(cdromdev));
    strncpy(cdrompath, sourcePath, sizeof(cdrompath));
    strncpy(cdromdev, sourceDev, sizeof(cdromdev));
    QI_FATAL(util_fileExists(cdrompath) && util_fileExists(cdromdev), "Invalid source path/device");
}

MappedFile *inst_openSourceFile(size_t osVariantIndex, const char *filename, size_t readahead) {
    return mappedFile_open(inst_getSourceFilePath(osVariantIndex, filename), readahead, qi_readErrorHandler);
}

bool inst_isInstallationSourceDisk(util_HardDisk *disk) {
    return (util_stringStartsWith(cdromdev, disk->device));
}

bool inst_isInstallationSourcePartition(util_Partition *part) {
    return (util_stringEquals(cdromdev, part->device));
}

bool inst_formatPartition(util_Partition *part) {
    char formatCmd[UTIL_MAX_CMD_LENGTH];
    bool ret = util_getFormatCommand(part, part->fileSystem, formatCmd, UTIL_MAX_CMD_LENGTH);
    QI_ASSERT(ret && "GetFormatCommand");
    return (0 == ad_runCommandBox("Formatting partition...", formatCmd));
}
