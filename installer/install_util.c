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

const char *inst_getSizeString(uint64_t size) {
    static const char *suffixes[] = { " B", "KB", "MB", "GB", "TB", "PB" };
    static char sizeString[32] = "";

    double leftover = (double) size;
    size_t suffixIdx = 0;

    while (leftover >= 1024.0 && suffixIdx < util_arraySize(suffixes)) {
        leftover /= 1024.0;
        suffixIdx++;
    }

    uint64_t integral = (uint64_t) leftover;
    uint64_t decimal = (uint64_t) (10.0 * (leftover - (double) integral));
    sprintf(sizeString, "%llu.%llu %s", integral, decimal, suffixes[suffixIdx]);
    return sizeString;
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

/********************************* Table code  */

typedef struct {
    size_t width;
    const char *label;
} inst_UiTableColumn;

typedef struct {
    size_t colCount;
    inst_UiTableColumn *cols;
} inst_UiTableRow;

static size_t inst_tableGetInsertPos(const inst_UiTableRow *row, size_t index) {
    size_t totalLen = 0;
    for (size_t i = 0; i < index; i++) { totalLen += row->cols[i].width; }
    return totalLen;
}

static size_t inst_tableGetRowWidth (const inst_UiTableRow *row) {
    return inst_tableGetInsertPos(row, row->colCount);
}

// Set string in the selected column of the selected row definition in the OUT string. 
// It is assumed OUT's size is >= inst_table_getRowWidth
static void inst_tableSetString(char *out, const inst_UiTableRow *row, size_t column, const char *str) {
    size_t pos = inst_tableGetInsertPos(row, column);
    size_t width = row->cols[column].width;
    memset(&out[pos], ' ', width);
    util_getCappedString(&out[pos], str, row->cols[column].width - 1);
}

// Get a header string for ad_Menu that shows the labels of all the columns.
static const char *inst_tableGetHeader(const inst_UiTableRow *row) {
    static char rowStr[128+2] = "  "; // +2 because menu items start 2 columns offset.
    QI_FATAL(inst_tableGetRowWidth(row) < 128, "Row too big!");
    for (size_t i = 0; i < row->colCount; i++) {
        inst_tableSetString(&rowStr[2], row, i, row->cols[i].label);
    }
    return rowStr;
}


/********************************* PARTITION MENU ROW TABLE FORMATTING STUFF */

static inst_UiTableColumn partitionMenuCols[] = {
    { 22,   "Disk Model/Name" },
    { 12,   "" }, // Disk total size, no label due to clutter
    { 14,   "Partition" },
    { 10,   "Size" },
    { 10,   "Format" },
};
static inst_UiTableRow partitionMenuRow = { util_arraySize(partitionMenuCols), partitionMenuCols };

const char *inst_getPartitionMenuHeader(void) {
    static char ret[128] = " ";
    // Offset by one character, since we need the extra char to put an asterisk on source media
    snprintf(ret, 128, " %s", inst_tableGetHeader(&partitionMenuRow));
    return ret;
}

const char *inst_getPartitionMenuString(util_Partition *part) {
    // Asterisk at the start if it's the source partition.
    static char ret[128] = " ";
    char *start = &ret[1];
    util_HardDisk *disk = part->parent;
    inst_UiTableRow *row = &partitionMenuRow;
    char partSizeString[32];
    // On extended partitions, don't show a size.
    sprintf(partSizeString, "%s",  part->fileSystem != fs_extended ? inst_getSizeString(part->size)
                                                                   : "--------");
    // We want to declutter a bit - so we only show the disk MODEL if
    // it's the first partition in this disk's partition list
    // we do that in a VERY hacky way: check if part pointer is the same
    // as the pointer to the first element in its parent's partitions array. 
    bool isFirstPartInDisk = part == (&part->parent->partitions[0]);
    const char *modelString = isFirstPartInDisk ? disk->model : "";
    char diskSizeString[32] = "";
    if (isFirstPartInDisk) {
        sprintf(diskSizeString, "(%s)", inst_getSizeString(disk->size));
    }

    inst_tableSetString(start, row, 0, modelString);
    inst_tableSetString(start, row, 1, diskSizeString);
    inst_tableSetString(start, row, 2, util_shortDeviceString(part->device));
    inst_tableSetString(start, row, 3, partSizeString);
    inst_tableSetString(start, row, 4, util_utilFilesystemToString(part->fileSystem));
    ret[0] = inst_isInstallationSourcePartition(part) ? '*' : ' ';
    return ret;
}

/********************************* DISK MENU ROW TABLE FORMATTING STUFF */

static inst_UiTableColumn diskMenuCols[] = {
    { 22,   "Disk Model/Name" },
    { 14,   "Device" },
    { 11,   "Size" },
    { 11,   "Table Type" },
};

static inst_UiTableRow diskMenuRow = { util_arraySize(diskMenuCols), diskMenuCols };

const char *inst_getDiskMenuHeader(void) {
    static char ret[128] = " ";
    // Offset by one character, since we need the extra char to put an asterisk on source media
    snprintf(ret, 128, " %s", inst_tableGetHeader(&diskMenuRow));
    return ret;
}

const char *inst_getDiskMenuString(util_HardDisk *disk) {
    static char ret[128] = " ";
    char *start = &ret[1];
    inst_UiTableRow *row = &diskMenuRow;
    inst_tableSetString(start, row, 0, disk->model);
    inst_tableSetString(start, row, 1, util_shortDeviceString(disk->device));
    inst_tableSetString(start, row, 2, inst_getSizeString(disk->size));
    inst_tableSetString(start, row, 3, inst_getTableTypeString(disk));
    ret[0] = inst_isInstallationSourceDisk(disk) ? '*' : ' ';
    return ret;
}
