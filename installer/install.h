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
#include "anbui/anbui.h"


typedef enum {
    WIZ_VARIANT_SELECT,
    WIZ_MAIN_MENU,
    WIZ_DISKMGMT,
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
    WIZ_DO_NOTHING
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

typedef enum {
    o_writeMBRAndSetActive = 0,
    o_formatTargetPartition,
    o_bootSector,
    o_skipLegacyDetection,
    o_installDriversBase,
    o_installDriversExtra,
    o_copyExtras,
    o_cregfix,
    o_lba64,
    o_uefi,
    QI_OPTIONIDX_MAX,
    o_baseOS, // Base OS copy, this is always enabled and this is just a hack to make the install code a bit nicer
    o_mount, // Mounting partition, ditto
    o_registry, // copy system registry, ditto
} qi_OptionIdx;

#define QI_OPTION_YES               (0)
#define QI_OPTION_NO                (1)
#define QI_MAX_OPTIONS_PER_CONFIG   (2)

typedef struct {
    qi_OptionIdx idx;
    const char *prompt;
    size_t optionCount;
    const char *optionStrings[QI_MAX_OPTIONS_PER_CONFIG];
    size_t selected;
    size_t progressBarIdx;
    bool partOfPreparation;
} qi_Option;

// Structure depicting all the static state needed for the wizard / installers
#define QI_VARIANT_NAME_SIZE (70)
typedef struct {
    bool disclaimerShown;                   // Indicates disclaimer was shown
    MappedFile *osRootFile;                 // The main OS data file, opened early for prebuffering
    uint64_t readahead;                     // Maximum safe readahead memory size
    ad_ProgressBox *progress;               // Multi-progress-bar-box ui element
    util_HardDiskArray *hda;                // Hard Disk Array of all disks in the system
    util_Partition *destination;            // destiination partition (Child of hda)
    size_t variantCount;                    // Amount of OS variants in  this image
    size_t variantIndex;                    // Selected OS variant
    char variantName[QI_VARIANT_NAME_SIZE]; // Name of selected OS variant
    bool error;                             // an error occurred in the installation
    qi_OptionIdx errorIndex;
    uint32_t preparationProgress;
} qi_InstallContext;

bool qi_main(int argc, char *argv[]);

/************ INSTALL.C *************/

/* Read error handler for MappedFile. */
MappedFile_ErrorReaction qi_readErrorHandler(int _errno, MappedFile *mf) ;

/* Disk management and partitioning menu(s) */
qi_WizardAction qi_diskMgmtMenu(qi_InstallContext *ctx);

/* Refresh and obtain system hard disk information into the install context */
bool qi_refreshDisks(qi_InstallContext *ctx);

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

/* Get a string of format "xxx.y zB" where xxx.y is a floating point number and z is a size suffix
   example: "123.4 GB" based on a raw input size in bytes
   dst is expected to be 16 bytes in size.  */
const char *inst_getSizeString(uint64_t size);

/* Get header string for partition menu table with all the labels */
const char *inst_getPartitionMenuHeader(void); 

/* Get a nicely formatted string for the install-destination selector menu */
const char *inst_getPartitionMenuString(util_Partition *part);

/* Get header string for disk menu table with all the labels */
const char *inst_getDiskMenuHeader(void); 

/* Get a nicely formatted string for the partitioning disk selector menu */
const char *inst_getDiskMenuString(util_HardDisk *disk);

#endif