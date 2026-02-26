/*
 * LUNMERCY - Installer component
 * (C) 2023 Eric Voirin (oerg866@googlemail.com)
 */

#include "install.h"

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <pthread.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/reboot.h>
#include <fcntl.h>
#include <sched.h>
#include <unistd.h>
#include <errno.h>
#include <utime.h>
#include <unistd.h>
#include <dirent.h>
#include <locale.h>

#include "qi_assert.h"
#include "mappedfile.h"
#include "util.h"
#include "version.h"
#include "mbr_boot_win98.h"

#include "anbui/anbui.h"

#include "install_msg.inc"
#include "install_cfg.inc"

// -sizeof(int) because the fileno is not part of the descriptor read from the mercypak file
#define MERCYPAK_FILE_DESCRIPTOR_SIZE (sizeof(inst_MercyPakFileDescriptor) - sizeof(int))
// -sizeof(uint32_t) because the filesize is not part of the descriptor read from the mercypak v2 file
#define MERCYPAK_V2_FILE_DESCRIPTOR_SIZE ((MERCYPAK_FILE_DESCRIPTOR_SIZE) - sizeof(uint32_t))
#define MERCYPAK_V2_MAX_IDENTICAL_FILES (16)
#define MERCYPAK_STRING_MAX (256)

#define MERCYPAK_V1_MAGIC "ZIEG"
#define MERCYPAK_V2_MAGIC "MRCY"


#define INST_SYSROOT_FILE "FULL.866"
#define INST_CREGFIX_FILE "CREGFIX.866"
#define INST_LBA64_FILE   "LBA64.866"
#define INST_DRIVER_FILE  "DRIVER.866"
#define INST_SLOWPNP_FILE "SLOWPNP.866"
#define INST_FASTPNP_FILE "FASTPNP.866"

typedef bool (*qi_OptionFunc)(size_t progressBarIndex);

static qi_InstallContext qi_wizData;


/* Gets a MercyPak string (8 bit length + n chars) into dst. Must be a buffer of >= 256 bytes size. */
static inline bool inst_getMercyPakString(MappedFile *file, char *dst) {
    bool success;
    uint8_t count;
    success = mappedFile_getUInt8(file, &count);
    success &= mappedFile_read(file, (uint8_t*) dst, (size_t) (count));
    dst[(size_t) count] = 0x00;
    return success;
}

// Creates all directory from an opened and header-parsed MercyPak file
// destPath is a buffer to hold the destination path name
// destPathAppend is a pointer to within that buffer where the mount point path stops and the target file name starts
// The buffer size starting at the append pointer needs to be MERCYPAK_STRING_MAX + 1 bytes
static bool qi_unpackCreateDirectories(MappedFile *file, uint32_t dirCount, char *destPath, char *destPathAppend) {
    bool success = true;
    for (uint32_t d = 0; d < dirCount; d++) {
        uint8_t dirFlags;
        success &= mappedFile_getUInt8(file, &dirFlags);
        success &= inst_getMercyPakString(file, destPathAppend);
        util_stringReplaceChar(destPathAppend, '\\', '/'); // DOS paths innit
        success &= util_mkDir(destPath, dirFlags);
    }
    return success;
}

// Unpacks all files from an opened and header-parsed MercyPak V1 file
// destPath is a buffer to hold the destination path name
// destPathAppend is a pointer to within that buffer where the mount point path stops and the target file name starts
// The buffer size starting at the append pointer needs to be MERCYPAK_STRING_MAX + 1 bytes
static bool qi_unpackExtractAllFilesV1(MappedFile *file, uint32_t fileCount, char *destPath, char *destPathAppend, size_t progressBarIndex) {
    inst_MercyPakFileDescriptor fileToWrite;
    bool success = true;

    for (uint32_t f = 0; f < fileCount; f++) {

        ad_progressBoxMultiUpdate(qi_wizData.progress, progressBarIndex, mappedFile_getPosition(file));

        /* Mercypak file metadata (see mercypak.txt) */

        success &= inst_getMercyPakString(file, destPathAppend);    // First, filename string
        util_stringReplaceChar(destPathAppend, '\\', '/');          // DOS paths innit

        success &= mappedFile_read(file, &fileToWrite, MERCYPAK_FILE_DESCRIPTOR_SIZE);

        int outfd = open(destPath,  O_WRONLY | O_CREAT | O_TRUNC);
        QI_ASSERT(outfd >= 0);

        success &= mappedFile_copyToFiles(file, 1, &outfd, fileToWrite.fileSize);

        success &= util_setDosFileTime(outfd, fileToWrite.fileDate, fileToWrite.fileTime);
        success &= util_setDosFileAttributes(outfd, fileToWrite.fileFlags);

        close(outfd);
    }

    return success;
}

// Unpacks all files from an opened and header-parsed MercyPak V2 file
// destPath is a buffer to hold the destination path name
// destPathAppend is a pointer to within that buffer where the mount point path stops and the target file name starts
// The buffer size starting at the append pointer needs to be MERCYPAK_STRING_MAX + 1 bytes
static bool qi_unpackExtractAllFilesV2(MappedFile *file, uint32_t fileCount, char *destPath, char *destPathAppend, size_t progressBarIndex) {
    /* Handle mercypak v2 pack file with redundant files optimized out */

    inst_MercyPakFileDescriptor    *filesToWrite            = calloc(MERCYPAK_V2_MAX_IDENTICAL_FILES, sizeof(inst_MercyPakFileDescriptor));
    int                            *fileDescriptorsToWrite  = calloc(MERCYPAK_V2_MAX_IDENTICAL_FILES, sizeof(int));
    uint8_t                         identicalFileCount      = 0;
    bool                            success                 = true;

    QI_FATAL(filesToWrite != NULL,              "Error allocating MercyPak V2 file headers.");
    QI_FATAL(fileDescriptorsToWrite != NULL,    "Error allocating MercyPak V2 file descriptors.");

    for (uint32_t f = 0; f < fileCount;) {
        ad_progressBoxMultiUpdate(qi_wizData.progress, progressBarIndex, mappedFile_getPosition(file));

        success &= mappedFile_getUInt8(file, &identicalFileCount);

        QI_ASSERT(identicalFileCount <= MERCYPAK_V2_MAX_IDENTICAL_FILES);

        // For every output file for this input file, open a write file descriptor
        for (uint32_t subFile = 0; success && subFile < identicalFileCount; subFile++) {
            success &= inst_getMercyPakString(file, destPathAppend);
            util_stringReplaceChar(destPathAppend, '\\', '/');

            fileDescriptorsToWrite[subFile] = open(destPath,  O_WRONLY | O_CREAT | O_TRUNC);

            success &= (fileDescriptorsToWrite[subFile] > 0);
            success &= mappedFile_read(file, &filesToWrite[subFile], MERCYPAK_V2_FILE_DESCRIPTOR_SIZE);
        }

        if (!success) break;
        
        uint32_t fileSize = 0;
        success &= mappedFile_getUInt32(file, &fileSize);

        success &= mappedFile_copyToFiles(file, identicalFileCount, fileDescriptorsToWrite, fileSize);

        for (uint32_t subFile = 0; success && subFile < identicalFileCount; subFile++) {
            success &= util_setDosFileTime(fileDescriptorsToWrite[subFile], filesToWrite[subFile].fileDate, filesToWrite[subFile].fileTime);
            success &= util_setDosFileAttributes(fileDescriptorsToWrite[subFile], filesToWrite[subFile].fileFlags);
            close(fileDescriptorsToWrite[subFile]);
        }

        f += identicalFileCount;
    }

    // Just in case
    if (!success) {
        for (uint32_t subFile = 0; subFile < MERCYPAK_V2_MAX_IDENTICAL_FILES; subFile++) {
            close(fileDescriptorsToWrite[subFile]);
        }
    }

    free(filesToWrite);
    free(fileDescriptorsToWrite);
    return success;
}

// Unpack an already opened Mercypak File. installPath = destination, progressBarIndex = progress bar in the main box to update
static bool qi_unpackGeneric(MappedFile *file, const char *installPath, size_t progressBarIndex) {
    char fileHeader[5] = {0};
    char *destPath = calloc(1, strlen(installPath) + MERCYPAK_STRING_MAX + 1);   // Full path of destination dir/file, the +256 is because mercypak strings can only be 255 chars max
    char *destPathAppend = destPath + strlen(installPath) + 1;  // Pointer to first char after the base install path in the destination path + 1 for the extra "/" we're gonna append

    sprintf(destPath, "%s/", installPath);

    bool success = true;
    uint32_t dirCount;
    uint32_t fileCount;

    success &= mappedFile_read(file, (uint8_t*) fileHeader, 4);
    success &= mappedFile_getUInt32(file, &dirCount);
    success &= mappedFile_getUInt32(file, &fileCount);

    if (!success) {
        return false;
    }

    // Check if we're unpacking a V2 file, which does redundancy stuff.
    bool isV1 = util_stringEquals(fileHeader, MERCYPAK_V1_MAGIC);
    bool isV2 = util_stringEquals(fileHeader, MERCYPAK_V2_MAGIC);

    QI_FATAL(isV1 || isV2, "MercyPak File Version Error");

    // Create all the directories
    if (!qi_unpackCreateDirectories(file, dirCount, destPath, destPathAppend)) {
        msg_directoryWarning();
    }

    // Unpack all the files
    ad_progressBoxSetMaxProgress(qi_wizData.progress, progressBarIndex, mappedFile_getFileSize(file));

    if (isV2) {
        success = qi_unpackExtractAllFilesV2(file, fileCount, destPath, destPathAppend, progressBarIndex);
    } else {
        success = qi_unpackExtractAllFilesV1(file, fileCount, destPath, destPathAppend, progressBarIndex);
    }

    ad_progressBoxMultiUpdate(qi_wizData.progress, progressBarIndex, mappedFile_getPosition(file));

    free(destPath);
    return success;
}

// Copy all files from sourceBase to targetBase.
// fileCount is updated for every file copied
// progressBarIndex = progress bar in the main box to update
static bool qi_copyAllFilesInDirectoryRecursive(const char *sourceBase, const char *targetBase, uint32_t *fileCount, size_t progressBarIndex) {
    struct dirent *e;
    DIR *d = opendir(sourceBase);
    util_returnOnNull(d, false);

    bool success = true;

    if (!util_fileExists(targetBase)) {
        success &= util_mkDir(targetBase, 0);        
    }

    while (success && (e = readdir(d))) {
        if (util_stringEquals(e->d_name, ".") || util_stringEquals(e->d_name, ".."))
            continue;

        char *newSource = util_pathAppend(sourceBase, e->d_name);
        char *newTarget = util_pathAppend(targetBase, e->d_name);       

        QI_FATAL(newSource != NULL && newTarget != NULL, "Failed to allocate path string");

        if (util_isFile(newSource)) {
            success &= util_fileCopy(newSource, newTarget);
            *fileCount += 1;
            ad_progressBoxMultiUpdate(qi_wizData.progress, progressBarIndex, *fileCount);
        } else if (util_isDir(newSource)) {
            qi_copyAllFilesInDirectoryRecursive(newSource, newTarget, fileCount, progressBarIndex);
        }

        free(newSource);
        free(newTarget);
    }

    closedir(d);
    return success;
}

// Copies a file tree from <source media>/source to <destination partition>/subdir, updating progress bar in the process
static bool qi_copyFileTree(const char *source, const char *subdir, size_t progressBarIndex) {
    const char *sourceBase = inst_getSourceFilePath(0, source);
    const char *targetBase = inst_getTargetFilePath(qi_wizData.destination, subdir);
    
    ad_progressBoxSetMaxProgress(qi_wizData.progress, progressBarIndex, util_getFileCountRecursive(sourceBase));

    uint32_t fileCount = 0;
    return qi_copyAllFilesInDirectoryRecursive(sourceBase, targetBase, &fileCount, progressBarIndex);
}

MappedFile_ErrorReaction qi_readErrorHandler(int _errno, MappedFile *mf) {
    const char *errorMenuOptions[] = { "Retry", "Cancel" };
    
    ad_screenSaveState();
    ad_restore();

    int32_t whatToDo = ad_menuExecuteDirectly("Read error!", false, 2, errorMenuOptions,
        "An error has occured while reading the install data!\n\n"
        "Position: %zu Bytes\n"
        "Error:    %s (%d)", mappedFile_getPosition(mf), strerror(_errno), _errno);
    
    ad_screenLoadState();

    return whatToDo == 0 ? MF_RETRY : MF_CANCEL;
}

// Does a cleanup of all dynamic resources in the install context
static void qi_cleanup() {
    // If OSRoot file is open, close it first.
    if (qi_wizData.osRootFile != NULL) {
        mappedFile_close(qi_wizData.osRootFile);
        qi_wizData.osRootFile = NULL;
    }

    // if we have a destination partition, make sure it is unmounted.
    if (qi_wizData.destination != NULL) {
        util_unmountPartition(qi_wizData.destination);
        qi_wizData.destination = NULL;
    }

    if (qi_wizData.hda != NULL) {
        util_hardDiskArrayDestroy(qi_wizData.hda);
        qi_wizData.destination = NULL;
        qi_wizData.hda = NULL;
    }

    if (qi_wizData.progress != NULL) {
        ad_progressBoxDestroy(qi_wizData.progress);
        qi_wizData.progress = NULL;
    }

    qi_wizData.error = false;
    qi_wizData.preparationProgress = 0;
}

static qi_WizardAction qi_variantSelectAndStartPrebuffering() {
    size_t variantCount = 0;

    ad_Menu *menu = ad_menuCreate("Installation Variant", "Select the operating system variant you wish to install.", false, false);
    QI_ASSERT(menu != NULL);

    while (true) {
        // Find and get all available OS variants on this source disc
        char tmpVariantLabel[128];
        const char *tmpInfPath = inst_getSourceFilePath(variantCount + 1, "win98qi.inf");

        if (!util_fileExists(tmpInfPath)) break;

        bool infFileReadOK = util_readFirstLineFromFileIntoBuffer(tmpInfPath, tmpVariantLabel, sizeof(tmpVariantLabel));
        QI_FATAL(infFileReadOK, "Cannot get OS data from source.");

        ad_menuAddItemFormatted(menu, "%s", tmpVariantLabel);

        variantCount++;
    }

    QI_FATAL(variantCount != 0, "No OS variants found on this install source.");

    // Don't have to show a menu if we have no choice to do innit.
    int menuResult = 0;

    if (variantCount > 1) {
        menuResult = ad_menuExecute(menu);
    }

    ad_menuGetItemText(menu, (size_t) menuResult, qi_wizData.variantName, QI_VARIANT_NAME_SIZE);

    ad_menuDestroy(menu);

    qi_wizData.variantCount = variantCount;
    qi_wizData.readahead = util_getProcSafeFreeMemory() * 6 / 10;
    qi_wizData.variantIndex = (size_t) menuResult + 1;
    qi_wizData.osRootFile = inst_openSourceFile(qi_wizData.variantIndex, INST_SYSROOT_FILE, qi_wizData.readahead);

    QI_FATAL(qi_wizData.osRootFile != NULL, "Could not open OS data file for reading");

    return WIZ_NEXT;
}

#ifdef RAID_TEST
static void qi_findRaids() {
    ad_yesNoBox("Find and activate RAID sets", false,
        "This option is meant for users with 'Fake-RAID' cards, such as"
        "HighPoint or Promise IDE/SATA RAID cards which can only boot"
        "from RAID arrays created by them (e.g. FastTrak S150 TX4)."
        ""
        "Since this is not well supported by the Linux ecosystem anymore,"
        "it is recommended that you do not use the RAID features of the"
        "card and use the raw disks themselves instead."
        ""
        "This option is available as a 'last resort' and may not be"
        "supported in the future."
        ""
        "Do you wish to continue?");
}
#endif

static qi_WizardAction qi_mainMenu() {
    if (!qi_wizData.disclaimerShown) {
        msg_disclaimer();
        qi_wizData.disclaimerShown = true;
    }

    ad_Menu *menu = ad_menuCreate("Windows 9x QuickInstall: Main Menu", 
        msg_mainMenuText, true, false);
    QI_ASSERT(menu);
    ad_menuAddItemFormatted(menu, "[INSTALL] Install %s", qi_wizData.variantName);
    ad_menuAddItemFormatted(menu, "   [DISK] Manage and partition hard disks");
#ifdef RAID_TEST
    ad_menuAddItemFormatted(menu, "   [RAID] Try to find and activate RAID sets");
#endif
    ad_menuAddItemFormatted(menu, "  [SHELL] Exit to minmal diagnostic Linux shell");

    // Show the OS selection option in the menu only if we have more than 1 OS variant in this image.
    if (qi_wizData.variantCount > 1) {
        ad_menuAddItemFormatted(menu, "     [OS] Change Operating System variant");
    }


    int menuResult = ad_menuExecute(menu);
    ad_menuDestroy(menu);

    QI_ASSERT(menuResult != AD_ERROR);

    switch (menuResult) {
        case 0:             return WIZ_SELECT_PARTITION;
        case 1:             return WIZ_DISKMGMT;
        case 2:             return WIZ_EXIT_TO_SHELL;
        case 3:             return WIZ_REDO_FROM_START;
        case AD_CANCELED:   return WIZ_REDO_FROM_START;
        default:            QI_FATAL(false, "Inconsistent menu state");
                            return WIZ_REDO_FROM_START;
    }
}

static qi_WizardAction qi_destinationSelect(void) {
    char menuPrompt[512];
    snprintf(menuPrompt, sizeof(menuPrompt),
            "Select the partition you wish to install to.\n"
            "An asterisk (*) means that this is the source media and\n"
            "cannot be used.\n\n"
            "%s", inst_getPartitionMenuHeader());

    if (!qi_refreshDisks(&qi_wizData)) {
        msg_refreshDiskError();
        return WIZ_MAIN_MENU;
    } else if (qi_wizData.hda->count == 0) {
        msg_noHardDisksFoundError();
        return WIZ_MAIN_MENU;
    }

    while (1) {
        ad_Menu *menu = ad_menuCreate("Installation Destination", menuPrompt,true, false);

        QI_ASSERT(menu);

        for (size_t disk = 0; disk < qi_wizData.hda->count; disk++) {
            util_HardDisk *harddisk = &qi_wizData.hda->disks[disk];

            for (size_t part = 0; part < harddisk->partitionCount; part++) {
                ad_menuAddItemFormatted(menu, "%s", inst_getPartitionMenuString(&harddisk->partitions[part]));
            }
        }

        if (ad_menuGetItemCount(menu) == 0) {
            ad_menuDestroy(menu);
            ad_okBox("Error", false, "No partitions were found! Partition a disk and try again!");
            return WIZ_BACK;
        }

        int menuResult = ad_menuExecute(menu);
        ad_menuDestroy(menu);
        
        if (menuResult == AD_CANCELED) {
            return WIZ_MAIN_MENU;
        }

        util_Partition *partition = util_getPartitionFromIndex(qi_wizData.hda, menuResult);
        QI_ASSERT(partition);

        // Is this the installation source? If yes, show menu again.
        if (inst_isInstallationSourcePartition(partition)) {
            msg_sourcePartitionError();
            continue;
        }

        if (!util_stringEquals(partition->parent->tableType, "dos")) {
            msg_destinationInvalidPartitionTable();
            continue;
        }

        if (partition->fileSystem == fs_unsupported || partition->fileSystem == fs_none) {
            msg_unsupportedFileSystemError();
            continue;
        }

        if (util_isPartitionMounted(partition)) {
            if (!msg_askUnmountBeforeInstall(partition)) {
                // Partition is mounted but user does not wish to unmount -- continue looping
                continue;
            }

            util_unmountPartition(partition);
        }

        qi_wizData.destination = partition;
        return WIZ_NEXT;
    }
}

qi_Option *qi_configGetItemByOptionIdx(qi_OptionIdx index) {
    for (size_t i = 0; i < QI_OPTION_ARRAY_SIZE; i++) {
        if (qi_options[i].idx == index)
            return &qi_options[i];
    }
    QI_FATAL(false, "Internal configuration state invalid.");
    return NULL;
}

static size_t qi_configGet(qi_OptionIdx index) {
    qi_Option *cfg = qi_configGetItemByOptionIdx(index);
    return cfg->selected;
}

static void qi_configSet(qi_OptionIdx index, size_t value) {
    qi_Option *cfg = qi_configGetItemByOptionIdx(index);
    cfg->selected = value;
}

static size_t qi_configGetProgressBarIndex(qi_OptionIdx index) {
    qi_Option *cfg = qi_configGetItemByOptionIdx(index);
    return cfg->progressBarIdx;
}

static void qi_configSetProgressBarIndex(qi_OptionIdx index, size_t value) {
    qi_Option *cfg = qi_configGetItemByOptionIdx(index);
    QI_FATAL(cfg != NULL, "Internal configuration state invalid.");
    cfg->progressBarIdx = value;
}

static size_t qi_configGetPreparationStepCount() {
    size_t result = 0;
    for (size_t i = 0; i < QI_OPTION_ARRAY_SIZE; i++) {
        if (qi_options[i].partOfPreparation && qi_options[i].selected == QI_OPTION_YES) {
            result++;
        }
    }
    return result;
}

static const char *qi_configGetLabel(qi_OptionIdx index) {
    qi_Option *cfg = qi_configGetItemByOptionIdx(index);
    return cfg->prompt;
}

static qi_WizardAction qi_config(void) {
    ad_MultiSelector *menu = ad_multiSelectorCreate("Configuration", 
        "Please configure your installation.\n"
        "\n"
        "NOTE: Pressing [ENTER] will immediately start the installation!", 
        true);

    QI_ASSERT(menu);

    // Make a menu selector entry for every configurable detail
    for (qi_OptionIdx i = 0; i < QI_OPTIONIDX_MAX; i++) {
        qi_Option *cfg = qi_configGetItemByOptionIdx(i);
        QI_FATAL(cfg != NULL, "Internal configuration state invalid.");
        ad_multiSelectorAddItem(menu, cfg->prompt, cfg->optionCount, cfg->selected, cfg->optionStrings);
    }
    int menuResult = ad_multiSelectorExecute(menu);

    // Read the configuration back from the MultiSelctor TUI object and store the results in our options array.
    for (qi_OptionIdx i = 0; i < QI_OPTIONIDX_MAX; i++) {
        size_t value = ad_multiSelectorGet(menu, (size_t) i);
        qi_configSet(i, value);
    }

    ad_multiSelectorDestroy(menu);

    if (menuResult == AD_CANCELED) {
        return WIZ_BACK;
    }

    if (QI_OPTION_YES == qi_configGet(o_uefi)) {
        msg_uefiInfoBox();
    }

    if (QI_OPTION_YES == qi_configGet(o_cregfix)) {
        msg_cregfixInfoBox();
    }
    
    if (QI_OPTION_YES == qi_configGet(o_lba64)) {
        msg_lba64InfoBox();
    }
 
    return WIZ_NEXT;
}

static void qi_installAddToProgressBoxIfEnabled(size_t *pbIndex, qi_OptionIdx optionIndex, const char *label) {
    if (QI_OPTION_YES == qi_configGet(optionIndex)) {
        ad_progressBoxAddItem(qi_wizData.progress, label, 0);
        qi_configSetProgressBarIndex(optionIndex, *pbIndex);
        (*pbIndex)++;
    }
}

static bool qi_installWriteMbrSetActive(size_t progressBarIndex) {
    bool success = util_writeMBRToDrive(qi_wizData.destination->parent, __MBR_WIN98__)
                && util_setPartitionActive(qi_wizData.destination);
    ad_progressBoxMultiUpdate(qi_wizData.progress, progressBarIndex, ++qi_wizData.preparationProgress);
    return success;
}

static bool qi_installWriteBootSector(size_t progressBarIndex) {
    const util_BootSectorModifier *bsModifierList = NULL;

    if (qi_wizData.destination->fileSystem == fs_fat16) {
        bsModifierList = __WIN98_FAT16_BOOT_SECTOR_MODIFIERS__;
    } else if (qi_wizData.destination->fileSystem == fs_fat32) {
        bsModifierList = __WIN98_FAT32_BOOT_SECTOR_MODIFIERS__;
    }

    bool success = util_modifyAndwriteBootSectorToPartition(qi_wizData.destination, bsModifierList);

    ad_progressBoxMultiUpdate(qi_wizData.progress, progressBarIndex, ++qi_wizData.preparationProgress);
    return success;
}

static bool qi_installFormat(size_t progressBarIndex) {
    char formatCmd[UTIL_MAX_CMD_LENGTH];
    bool ret = util_getFormatCommand(qi_wizData.destination, qi_wizData.destination->fileSystem, formatCmd, UTIL_MAX_CMD_LENGTH);
    QI_ASSERT(ret && "GetFormatCommand");

    util_CommandOutput *cmd = util_commandOutputCapture(formatCmd);
    QI_FATAL(cmd, "Failed to obtain format command output");
    int result = cmd->returnCode;
    if (result != 0) {
        msg_formatFailed(qi_wizData.destination);
    }
    util_commandOutputDestroy(cmd);
    ad_progressBoxMultiUpdate(qi_wizData.progress, progressBarIndex, ++qi_wizData.preparationProgress);
    return result == 0;
}

static bool qi_installMountPartition(size_t progressBarIndex) {
    bool success = util_mountPartition(qi_wizData.destination);
    ad_progressBoxMultiUpdate(qi_wizData.progress, progressBarIndex, ++qi_wizData.preparationProgress);
    return success;
}

static bool qi_installUefi(size_t progressBarIndex) {
    bool success = true;

    success &= util_mkDir(inst_getTargetFilePath(qi_wizData.destination, "EFI/BOOT"), 0);
    success &= util_fileCopy(inst_getSourceFilePath(0, "csmwrap.efi"), inst_getTargetFilePath(qi_wizData.destination, "EFI/BOOT/BOOTIA32.EFI"));

    ad_progressBoxMultiUpdate(qi_wizData.progress, progressBarIndex, ++qi_wizData.preparationProgress);
    return success;
}

static bool qi_installUnpackGeneric(size_t progressBarIndex, const char *fileName) {
    MappedFile *file = mappedFile_open(inst_getSourceFilePath(qi_wizData.variantIndex, fileName),
         qi_wizData.readahead, qi_readErrorHandler);

    QI_FATAL (file != NULL, "Failed to open MappedFile for opening");
    
    bool success = qi_unpackGeneric(file, qi_wizData.destination->mountPath, progressBarIndex);
    mappedFile_close(file);
    return success;
}

static bool qi_installLba64(size_t progressBarIndex) {
    return qi_installUnpackGeneric(progressBarIndex, INST_LBA64_FILE);
}

static bool qi_installCregfix(size_t progressBarIndex) {
    return qi_installUnpackGeneric(progressBarIndex, INST_CREGFIX_FILE);
}

static bool qi_installDriversBase(size_t progressBarIndex) {
    return qi_installUnpackGeneric(progressBarIndex, INST_DRIVER_FILE);
}

static bool qi_installRegistry(size_t progressBarIndex) {
    bool skipLegacy = (QI_OPTION_YES == qi_configGet(o_skipLegacyDetection));
    const char *filename = skipLegacy ? INST_FASTPNP_FILE : INST_SLOWPNP_FILE;
    return qi_installUnpackGeneric(progressBarIndex, filename);
}

static bool qi_installCopyOSRoot(size_t progressBarIndex) {
    bool success = qi_unpackGeneric(qi_wizData.osRootFile, qi_wizData.destination->mountPath, progressBarIndex);
    mappedFile_close(qi_wizData.osRootFile);
    qi_wizData.osRootFile = NULL;
    return success;
}

static bool qi_installDriversExtra(size_t progressBarIndex) {
    return qi_copyFileTree("driver.ex", "driver.ex", progressBarIndex);
}

static bool qi_installCopyExtras(size_t progressBarIndex) {
    return qi_copyFileTree("extras", "extras", progressBarIndex);
}

static void qi_installExecuteIfEnabled(qi_OptionIdx index, qi_OptionFunc func, const char *footerText) {
    if (QI_OPTION_NO == qi_configGet(index) || qi_wizData.error) {
        return;
    }

    ad_setFooterText(footerText);
    if (!func(qi_configGetProgressBarIndex(index))) {
        qi_wizData.error = true;
        qi_wizData.errorIndex = index;
    }
    ad_clearFooter();
}

static qi_WizardAction qi_install(void) {
    // Start error-less
    qi_wizData.error = false;

    // Make the progress bars
    qi_wizData.progress = ad_progressBoxMultiCreate("Installing...",
        "Please wait while your OS is being installed:\n"
        "%s", qi_wizData.variantName);

    QI_FATAL(qi_wizData.progress != NULL, "Cannot allocate progress box UI");

    ad_progressBoxAddItem(qi_wizData.progress, "Disk & Patch Preparation",      0);     // ProgressBar index = 0
    ad_progressBoxAddItem(qi_wizData.progress, "Copy files (Operating System)", 0);     // ProgressBar index = 1
    ad_progressBoxAddItem(qi_wizData.progress, "Copy files (System Registry)",  0);     // ProgressBar index = 2

    size_t progressBarIndex = 3;
    
    qi_installAddToProgressBoxIfEnabled(&progressBarIndex, o_cregfix,                   "Copy Files (CREGFIX)");
    qi_installAddToProgressBoxIfEnabled(&progressBarIndex, o_lba64,                     "Copy Files (LBA64/GPT Support)");
    qi_installAddToProgressBoxIfEnabled(&progressBarIndex, o_installDriversBase,        "Copy Files (Base Drivers)");
    qi_installAddToProgressBoxIfEnabled(&progressBarIndex, o_installDriversExtra,       "Copy Files (Extended Drivers)");
    qi_installAddToProgressBoxIfEnabled(&progressBarIndex, o_copyExtras,                "Copy Files (Extras & Tools)");

    ad_progressBoxPaint(qi_wizData.progress);

    // The topmost progress bar must be updated with the maximum value, which is the amount of steps in the preparation
    ad_progressBoxSetMaxProgress(qi_wizData.progress, 0, qi_configGetPreparationStepCount());

    // Execute preparation steps
    qi_wizData.preparationProgress = 0;
    qi_installExecuteIfEnabled(o_writeMBRAndSetActive,  qi_installWriteMbrSetActive,    "Writing MBR & Setting Partition Active");
    qi_installExecuteIfEnabled(o_formatTargetPartition, qi_installFormat,               "Formatting Target Partition");
    qi_installExecuteIfEnabled(o_bootSector,            qi_installWriteBootSector,      "Writing Boot Sector");
    qi_installExecuteIfEnabled(o_mount,                 qi_installMountPartition,       "Mounting Target Partition");

    // Execute file copies
    qi_installExecuteIfEnabled(o_uefi,                  qi_installUefi,                 "Installing UEFI support");
    qi_installExecuteIfEnabled(o_baseOS,                qi_installCopyOSRoot,           "Copying operating system files...");
    qi_installExecuteIfEnabled(o_registry,              qi_installRegistry,             "Copying system registry...");
    qi_installExecuteIfEnabled(o_cregfix,               qi_installCregfix,              "Installing CREGFIX patch...");
    qi_installExecuteIfEnabled(o_lba64,                 qi_installLba64,                "Installing LBA64/GPT Disk support driver...");
    qi_installExecuteIfEnabled(o_installDriversBase,    qi_installDriversBase,          "Copying base driver library files...");
    qi_installExecuteIfEnabled(o_installDriversExtra,   qi_installDriversExtra,         "Copying extended driver library files...");
    qi_installExecuteIfEnabled(o_copyExtras,            qi_installCopyExtras,           "Copying extras folder (tools, drivers, updates)...");

    ad_progressBoxDestroy(qi_wizData.progress);
    qi_wizData.progress = NULL;

    // Any failing module here will cause the installation to be canceled completely, regardless of state. 
    if (qi_wizData.error) {
        msg_installError(qi_configGetLabel(qi_wizData.errorIndex));
        return WIZ_REDO_FROM_START;
    }

    return WIZ_NEXT;
}

static qi_WizardAction qi_thisIsTheEnd() {
    const char *postInstallOptions[] = { "Reboot", "Return to Main Menu", "Exit to Linux Shell" };

    int selection = ad_menuExecuteDirectly("Windows 9x QuickInstall: Success",  false,
        3, postInstallOptions,
        "The installation was successful!\n"
        "What would you like to do now?");

    switch (selection) {
        case 0: return WIZ_REBOOT;
        case 1: return WIZ_REDO_FROM_START; // Can't just do main menu here because we need to cleanup.
        case 2: return WIZ_EXIT_TO_SHELL;
        default: QI_FATAL(false, "Bad menu state");
                 return WIZ_EXIT_TO_SHELL;
    }
}

static void qi_exit(bool doReboot) {
    qi_cleanup();
    sync();
    system("clear");
    ad_deinit();
    if (doReboot) {
        reboot(RB_AUTOBOOT);
    }
}
bool qi_wizard() {
    qi_WizardAction state = WIZ_VARIANT_SELECT;
    qi_WizardAction result = WIZ_NEXT;

    while (true) {
        switch (state) {
            case WIZ_VARIANT_SELECT:
                result = qi_variantSelectAndStartPrebuffering(); break;
            case WIZ_MAIN_MENU:
                result = qi_mainMenu(); break;
            case WIZ_SELECT_PARTITION:
                result = qi_destinationSelect(); break;
            case WIZ_CONFIGURE:
                result = qi_config(); break;
            case WIZ_DO_INSTALL:
                result = qi_install(); break;
            case WIZ_DISKMGMT:
                result = qi_diskMgmtMenu(&qi_wizData); break;
            case WIZ_THE_END:
                result = qi_thisIsTheEnd(); break;
            case WIZ_REDO_FROM_START:
                qi_cleanup();
                result = WIZ_VARIANT_SELECT;
                break;
            case WIZ_REBOOT:
                qi_exit(true);
                QI_FATAL(false, "Failed to initiate reboot!");
                break;
            case WIZ_EXIT_TO_SHELL:
                msg_exitToShellInfo();
                return true;
            case WIZ_EXIT_ERROR:
                return false;
            default:
                QI_FATAL(false, "Inconsistent wizard state!");
        }
        

        switch (result) {
            case WIZ_BACK:          state--; break;
            case WIZ_NEXT:          state++; break;
            case WIZ_DO_NOTHING:    break; /* state = state :) */ 
            default:                state = result;
        }
    }
}


bool qi_main(int argc, char *argv[]) {
    ad_init(LUNMERCY_BACKTITLE);
    setlocale(LC_ALL, "CP437");
    // VGA BIOS pattern small square
    ad_progressBoxSetCharAndColor((char) 0xFE, COLOR_WHITE, COLOR_BLACK, COLOR_WHITE, COLOR_GREEN);
    
    // Installer must run as root
    if (!util_runningAsRoot()) {
        msg_notRunningAsRootError();
        ad_deinit();
        return false;
    }

    // If we have commandline parameters use them, otherwise use the environment.
    if (argc == 3) {
        inst_setSourceMedia(argv[1], argv[2]);
    } else if (getenv("CDROM") && getenv("CDDEV")) {
        inst_setSourceMedia(getenv("CDROM"), getenv("CDDEV"));
    } else {
        msg_environmentError();
        ad_deinit();
        return false;
    }

    memset(&qi_wizData, 0, sizeof(qi_wizData));

    bool success = qi_wizard();

    qi_exit(false);
    return success;
}
