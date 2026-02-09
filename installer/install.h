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
    WIZ_VARIANT_SELECT,
    WIZ_MAIN_MENU,
    WIZ_PARTITION,
    WIZ_SELECT_PARTITION,
    WIZ_CONFIGURE,
    WIZ_DO_INSTALL,
    WIZ_THE_END,
    WIZ_REDO_FROM_START,
    WIZ_EXIT_TO_SHELL,
    WIZ_EXIT_ERROR,
    WIZ_REBOOT,
    WIZ_BACK,
    WIZ_NEXT,
} qi_WizardAction;

#pragma pack(1)

typedef struct {
    uint8_t fileFlags;
    uint16_t fileDate;
    uint16_t fileTime;
    uint32_t fileSize;
    int32_t fileno;
} inst_MercyPakFileDescriptor;

#pragma pack()


bool qi_main(int argc, char *argv[]);

/************ INSTALL.C *************/

/* Read error handler for MappedFile. */
MappedFile_ErrorReaction qi_readErrorHandler(int _errno, MappedFile *mf) ;

/************ INSTALL_UTIL.C ************/

/* Gets the absolute CDROM path of a file. 
   osVariantIndex is the index for the source variant, 0 means from the root. */
const char *inst_getSourceFilePath(size_t osVariantIndex, const char *filepath);

/* Gets the would-be absolute target partition path of a file.
   i.e. <partition->mountpoint>/<filepath> */
const char *inst_getTargetFilePath(util_Partition *part, const char *filepath);

/* Configures the CD (or other source media file path) */
void inst_setSourceMedia(const char* sourcePath, const char *sourceDev);

/* Opens a file from the source media. osVariantIndex is the index for the source variant, 0 means from the root. */
MappedFile *inst_openSourceFile(size_t osVariantIndex, const char *filename, size_t readahead);

/* Checks if given hard disk contains the installation source */
bool inst_isInstallationSourceDisk(util_HardDisk *disk);

/* Checks if given partition is the installation source */
bool inst_isInstallationSourcePartition(util_Partition *part);

/* Get string for a disk's partition table, the difference between the raw value and
   what you get here is that "dos" is replaced with the more expressive "mbr"...*/
const char *inst_getTableTypeString(util_HardDisk *disk);

/* Get a nicely formatted string for the install-destination selector menu
   This is too hard to do with just printf.
   And frankly this function is pure garbage but I really wanna get this release finished. */
const char *inst_getPartitionMenuString(util_Partition *part);

/* Get a nicely formatted string for the disk-to-partition selector menu
   This is too hard to do with just printf.
   And frankly this function is pure garbage but I really wanna get this release finished. */
const char *inst_getDiskMenuString(util_HardDisk *disk);

#endif