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

const char *inst_getTableTypeString(util_HardDisk *disk) {
    bool hasTableType = strlen(disk->tableType) > 0;
    if (hasTableType && util_stringEquals(disk->tableType, "dos")) {
        // Replace "dos" with "mbr" because that's nicer to look at
        return "mbr";
    } else if (hasTableType) {
        return disk->tableType;
    } else {
        return "<?>";
    }
}

const char *inst_getPartitionMenuString(util_Partition *part) {
    static char entry[128];
    util_HardDisk *disk = part->parent;
    char sizeString[32];
    uint64_t sizeMB = part->size / 1024ULL / 1024ULL;

    sizeMB = MIN(99999999ULL, sizeMB); // Sorry, we don't have the technology for disks bigger than 99TB :)
    snprintf(sizeString, 32, "%llu MB", sizeMB);

    //              *[other Hard Disk Name][nvme99n99p99][Unsupported][11222333 MB]  
    sprintf(entry, " [                    ][            ][           ][           ] ");
    
    // Asterisk on install source
    entry[0] = inst_isInstallationSourcePartition(part) ? '*' : ' ';

    // HDD names are capped at 20 chars in our display
    char hddName[20+1] = "";
    char devName[12+1] = "";    // Now honestly - this may end up wild, but if you have more than 99 nvme disks with more than 99 partitions, you are weird
    char formatName[11+1] = "";
    util_getCappedString(hddName, disk->model, 20);
    util_getCappedString(devName, util_shortDeviceString(part->device), 12);
    util_getCappedString(formatName, util_utilFilesystemToString(part->fileSystem), 11);
    util_stringInsert(&entry[1 + 1],                hddName);
    util_stringInsert(&entry[1 + 22 + 1],           devName);
    util_stringInsert(&entry[1 + 22 + 14 + 1],      formatName);
    util_stringInsert(&entry[1 + 22 + 14 + 13 + 1], sizeString);

    return entry;
}

const char *inst_getDiskMenuString(util_HardDisk *disk) {
    static char entry[128];
    char sizeString[32];
    uint64_t sizeMB = disk->size / 1024ULL / 1024ULL;

    sizeMB = MIN(99999999ULL, sizeMB); // Sorry, we don't have the technology for disks bigger than 99TB :)
    snprintf(sizeString, 32, "%llu MB", sizeMB);


    // *[other Hard Disk Name][nvme99n99   ][mbr        ][11222333 MB]
   
    sprintf(entry, 
      " [                    ][            ][           ][           ] ");
    
    // Asterisk on install source
    entry[0] = inst_isInstallationSourceDisk(disk) ? '*' : ' ';

    // HDD names are capped at 20 chars in our display
    char hddName[20+1] = "";
    char devName[12+1] = "";    // Now honestly - this may end up wild, but if you have more than 99 nvme disks with more than 99 partitions, you are weird
    char tableType[11+1] = "";
    util_getCappedString(hddName, disk->model, 20);
    util_getCappedString(devName, util_shortDeviceString(disk->device), 12);
    util_getCappedString(tableType, inst_getTableTypeString(disk), 11);
    util_stringInsert(&entry[1 + 1],                hddName);
    util_stringInsert(&entry[1 + 22 + 1],           devName);
    util_stringInsert(&entry[1 + 22 + 14 + 1],      tableType);
    util_stringInsert(&entry[1 + 22 + 14 + 13 + 1], sizeString);

    return entry;
}