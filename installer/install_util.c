#include <stdio.h>
#include <string.h>

#include "install.h"
#include "qi_assert.h"
#include "util.h"
#include "mappedfile.h"

static char cdrompath[PATH_MAX] = "";   // Path to install source media
static char cdromdev[PATH_MAX] = "";    // Block device for install source media
                                        // ^ initialized in setSourceMedia, called by inst_main

const char *inst_getSourceFilePath(size_t osVariantIndex, const char *filepath) {
    static char staticPathBuf[PATH_MAX*2];
    if (osVariantIndex > 0) {
        snprintf(staticPathBuf, sizeof(staticPathBuf), "%s/osroots/%zu/%s", cdrompath, osVariantIndex, filepath);
    } else {
        snprintf(staticPathBuf, sizeof(staticPathBuf), "%s/%s", cdrompath, filepath);
    }
    return staticPathBuf;
}

void inst_setSourceMedia(const char* sourcePath, const char *sourceDev) {
    QI_ASSERT(sourcePath != NULL);
    QI_ASSERT(sourceDev != NULL);
    QI_ASSERT(strlen(sourcePath) < sizeof(cdrompath));
    QI_ASSERT(strlen(sourceDev) < sizeof(cdromdev));
    strncpy(cdrompath, sourcePath, sizeof(cdrompath));
    strncpy(cdromdev, sourceDev, sizeof(cdromdev));
    QI_ASSERT(util_fileExists(cdrompath));
    QI_ASSERT(util_fileExists(cdromdev));
}

MappedFile *inst_openSourceFile(size_t osVariantIndex, const char *filename, size_t readahead) {
    return mappedFile_open(inst_getSourceFilePath(osVariantIndex, filename), readahead);
}

bool inst_isInstallationSourceDisk(util_HardDisk *disk) {
    return (util_stringStartsWith(cdromdev, disk->device));
}

bool inst_isInstallationSourcePartition(util_Partition *part) {
    return (util_stringEquals(cdromdev, part->device));
}

util_HardDiskArray *inst_getSystemHardDisks(void) {
    ad_setFooterText("Obtaining System Hard Disk Information...");
    util_HardDiskArray *ret = util_getSystemHardDisks();
    ad_clearFooter();
    return ret;
}

bool inst_formatPartition(util_Partition *part) {
    char formatCmd[UTIL_MAX_CMD_LENGTH];
    bool ret = util_getFormatCommand(part, part->fileSystem, formatCmd, UTIL_MAX_CMD_LENGTH);
    QI_ASSERT(ret && "GetFormatCommand");
    return (0 == ad_runCommandBox("Formatting partition...", formatCmd));
}