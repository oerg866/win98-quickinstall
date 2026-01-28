#ifndef INSTALL_H
#define INSTALL_H

/*
 * LUNMERCY - Installer component 
 * (C) 2023 Eric Voirin (oerg866@googlemail.com)
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "mappedfile.h"
#include "util.h"

typedef enum {
    QI_POSTINSTALL_REBOOT = 0,
    QI_POSTINSTALL_MAIN_MENU,
    QI_POSTINSTALL_EXIT,
    QI_POSTINSTALL_COUNT
} qi_PostInstallOption;

#pragma pack(1)

typedef struct {
    uint8_t fileFlags;
    uint16_t fileDate;
    uint16_t fileTime;
    uint32_t fileSize;
    int32_t fileno;
} inst_MercyPakFileDescriptor;

#pragma pack()


bool inst_main(int argc, char *argv[]);

/************ INSTALL_UTIL.C ************/

/* Gets the absolute CDROM path of a file. 
   osVariantIndex is the index for the source variant, 0 means from the root. */
const char *inst_getSourceFilePath(size_t osVariantIndex, const char *filepath);

/* Configures the CD (or other source media file path) */
void inst_setSourceMedia(const char* sourcePath, const char *sourceDev);

/* Opens a file from the source media. osVariantIndex is the index for the source variant, 0 means from the root. */
MappedFile *inst_openSourceFile(size_t osVariantIndex, const char *filename, size_t readahead);

/* Checks if given hard disk contains the installation source */
bool inst_isInstallationSourceDisk(util_HardDisk *disk);

/* Checks if given partition is the installation source */
bool inst_isInstallationSourcePartition(util_Partition *part);

/* Gets all system hard disks with UI notice. Remember to destroy allocated HDA later. */
util_HardDiskArray *inst_getSystemHardDisks(void);

/* Formats partition with UI notice */
bool inst_formatPartition(util_Partition *part);

#endif