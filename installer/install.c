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

typedef enum {
    INSTALL_OSROOT_VARIANT_SELECT,
    INSTALL_MAIN_MENU,
    INSTALL_PARTITION_WIZARD,
    INSTALL_SELECT_DESTINATION_PARTITION,
    INSTALL_FORMAT_PARTITION_PROMPT,
    INSTALL_MBR_ACTIVE_BOOT_PROMPT,
    INSTALL_REGISTRY_VARIANT_PROMPT,
    INSTALL_INTEGRATED_DRIVERS_PROMPT,
    INSTALL_DO_INSTALL,
} inst_InstallStep;



// -sizeof(int) because the fileno is not part of the descriptor read from the mercypak file
#define MERCYPAK_FILE_DESCRIPTOR_SIZE (sizeof(inst_MercyPakFileDescriptor) - sizeof(int))
// -sizeof(uint32_t) because the filesize is not part of the descriptor read from the mercypak v2 file
#define MERCYPAK_V2_FILE_DESCRIPTOR_SIZE ((MERCYPAK_FILE_DESCRIPTOR_SIZE) - sizeof(uint32_t))

#define MERCYPAK_V2_MAX_IDENTICAL_FILES (16)

#define MERCYPAK_V1_MAGIC "ZIEG"
#define MERCYPAK_V2_MAGIC "MRCY"

#define INST_CFDISK_CMD "cfdisk "

#define INST_SYSROOT_FILE "FULL.866"
#define INST_DRIVER_FILE  "DRIVER.866"
#define INST_SLOWPNP_FILE "SLOWPNP.866"
#define INST_FASTPNP_FILE "FASTPNP.866"

typedef enum {
    SETUP_ACTION_INSTALL = 0,
    SETUP_ACTION_PARTITION_WIZARD,
    SETUP_ACTION_EXIT_TO_SHELL,
} inst_SetupAction;

static inst_SetupAction inst_showMainMenu() {
    ad_Menu *menu = ad_menuCreate("Windows 9x QuickInstall: Main Menu", "Where do you want to go today(tm)?", true);
    QI_ASSERT(menu);

    ad_menuAddItemFormatted(menu, "[INSTALL] Install selected Operating System variant");
    ad_menuAddItemFormatted(menu, " [CFDISK] Partition hard disks");
    ad_menuAddItemFormatted(menu, "  [SHELL] Exit to minmal diagnostic Linux shell");

    int menuResult = ad_menuExecute(menu);

    QI_ASSERT(menuResult != AD_ERROR);

    if (menuResult == AD_CANCELED) /* Canceled = exit to shell for us */
        menuResult = (int) SETUP_ACTION_EXIT_TO_SHELL;

    ad_menuDestroy(menu);

    return (inst_SetupAction) menuResult;
}

/* 
    Shows OS Variant select. This is a bit peculiar because the dialog is not shown if
    there is only one OS variant choice. In that case it always returns "true".
    Otherwise it can return "false" if the user selected BACK.
*/
static bool inst_showOSVariantSelect(size_t *variantIndex, size_t *variantCount) {
    ad_Menu *menu = ad_menuCreate("Installation Variant", "Select the operating system variant you wish to install.", true);
    int menuResult;

    QI_ASSERT(menu);

    *variantCount = 0;

    // Find and get available OS variants.

    while (true) {
        char tmpVariantLabel[128];
        const char *tmpInfPath = inst_getSourceFilePath((*variantCount) + 1, "win98qi.inf");

        if (!util_fileExists(tmpInfPath)) {
            break;
        }

        if (!util_readFirstLineFromFileIntoBuffer(tmpInfPath, tmpVariantLabel, sizeof(tmpVariantLabel))) {
            ad_okBox("Error", false, "Error while reading file\n'%s'", tmpInfPath);
            break;
        }

        ad_menuAddItemFormatted(menu, "%zu: %s", (*variantCount) + 1, tmpVariantLabel);

        *variantCount += 1;
    }

    if (*variantCount > 1) {
        menuResult = ad_menuExecute(menu);
    } else {
        // Don't have to show a menu if we have no choice to do innit.
        menuResult = 0;
    }

    ad_menuDestroy(menu);

    QI_ASSERT(menuResult != AD_ERROR);

    if (menuResult == AD_CANCELED) {
        // BACK was pressed.
        return false;
    } else {
        *variantIndex = (size_t) menuResult + 1; // Just in case the directory listing is out of order for some reason...
        return true;   
    }    
}

/* On invalid/unsupported partition table, ask user if he wants to wipe it */
static void inst_invalidPartTableAskUserAndWipe(util_HardDisk *hdd) {
    if (msg_askNonMbrWarningWipeMBR(hdd)) {
        if (!util_wipePartitionTable(hdd)) {
            // No return value here since we can't do much and we'll launch cfdisk anyway
            msg_wipeMbrFailed();
        }
    }
}

/* Show partition wizard, returns true if user finished or false if he selected BACK. */
static void inst_showPartitionWizard(util_HardDiskArray **hddsPtr) {
    char cfdiskCmd[UTIL_MAX_CMD_LENGTH];
    int menuResult;

    while (1) {
        ad_Menu *menu = ad_menuCreate("Partition Wizard", 
            "Select the Hard Disk you wish to partition.\n"
            "An asterisk (*) means that this is the source media and\n"
            "cannot be altered.\n"
            "A question mark <?> means that this drive does not have a\n"
            "valid/known partition table type.", true);

        QI_ASSERT(menu);

        // At the beginning of every loop of the wizard, we need to refresh our disk list
        // and update the pointer of the caller
        util_hardDiskArrayDestroy(*hddsPtr);
        *hddsPtr = inst_getSystemHardDisks();
        QI_ASSERT(*hddsPtr != NULL);

        util_HardDiskArray *hdds = *hddsPtr;

        if (hdds->count == 0) {
            msg_noHardDisksFoundError();
            return;
        }

        for (size_t i = 0; i < hdds->count; i++) {
            bool hasPartitionTableType = (strlen(hdds->disks[i].tableType) > 0);
            ad_menuAddItemFormatted(menu, "%s [%s] (%llu MB) %s %s",
                hdds->disks[i].device,
                hdds->disks[i].model,
                hdds->disks[i].size / 1024ULL / 1024ULL,
                hasPartitionTableType ? hdds->disks[i].tableType : "<?>",
                inst_isInstallationSourceDisk(&hdds->disks[i]) ? "(*)" : ""
                );
        }
        
        ad_menuAddItemFormatted(menu, "%s", "[FINISHED]");

        menuResult = ad_menuExecute(menu);

        ad_menuDestroy(menu);

        QI_ASSERT(menuResult != AD_ERROR);

        if (menuResult == AD_CANCELED) // BACK was pressed.
            return;
        
        if (menuResult == (int) hdds->count)
            return;
        
        // Check if we're trying to partition the install source disk. If so, warn user and continue looping.
        if (inst_isInstallationSourceDisk(&hdds->disks[menuResult])) {
            msg_installationSourceDiskError();
            continue;
        }

        if (!util_stringEquals(hdds->disks[menuResult].tableType, "dos")) {
            inst_invalidPartTableAskUserAndWipe(&hdds->disks[menuResult]);
        }

        // Invoke cfdisk command for chosen drive.
        snprintf(cfdiskCmd, UTIL_MAX_CMD_LENGTH, "%s%s", INST_CFDISK_CMD, hdds->disks[menuResult].device);      
        system(cfdiskCmd);

        ad_restore();
        msg_reminderToFormatNewPartition();
    }
}

/* Shows partition selector. Returns pointer to the selected partition. A return value of NULL means the user wants to go back. */
static util_Partition *inst_showPartitionSelector(util_HardDiskArray *hdds) {
    int menuResult;
    util_Partition *result = NULL;

    QI_ASSERT(hdds);

    if (hdds->count == 0) {
        msg_noHardDisksFoundError();
        return NULL;
    }

    while (1) {
        ad_Menu *menu = ad_menuCreate("Installation Destination", 
            "Select the partition you wish to install to.\n"
            "An asterisk (*) means that this is the source media and\n"
            "cannot be used.", true);

        QI_ASSERT(menu);

        for (size_t disk = 0; disk < hdds->count; disk++) {
            util_HardDisk *harddisk = &hdds->disks[disk];

            for (size_t part = 0; part < harddisk->partitionCount; part++) {
                util_Partition *partition = &harddisk->partitions[part];

                ad_menuAddItemFormatted(menu, "%8s: (%s, %llu MB) on disk %s [%s] %s",
                    util_shortDeviceString(partition->device),
                    util_utilFilesystemToString(partition->fileSystem),
                    partition->size / 1024ULL / 1024ULL,
                    util_shortDeviceString(harddisk->device),
                    harddisk->model,
                    inst_isInstallationSourcePartition(partition) ? "(*)" : ""
                );
            }
        }

        if (ad_menuGetItemCount(menu) == 0) {
            ad_menuDestroy(menu);
            ad_okBox("Error", false, "No partitions were found! Partition your disk and try again!");
            return NULL;
        }

        menuResult = ad_menuExecute(menu);
        ad_menuDestroy(menu);
        
        if (menuResult < 0) { // User wants to go back
            return NULL;
        }

        result = util_getPartitionFromIndex(hdds, menuResult);
        QI_ASSERT(result);

        // Is this the installation source? If yes, show menu again.
        if (inst_isInstallationSourcePartition(result)) {
            msg_sourcePartitionError();
            continue;
        }
        
        if (result->fileSystem == fs_unsupported || result->fileSystem == fs_none) {
            msg_unsupportedFileSystemError();
            continue;
        }

        return result;
    }
}

/* Gets a MercyPak string (8 bit length + n chars) into dst. Must be a buffer of >= 256 bytes size. */
static inline bool inst_getMercyPakString(MappedFile *file, char *dst) {
    bool success;
    uint8_t count;
    success = mappedFile_getUInt8(file, &count);
    success &= mappedFile_read(file, (uint8_t*) dst, (size_t) (count));
    dst[(size_t) count] = 0x00;
    return success;
}

static bool inst_copyFiles(MappedFile *file, const char *installPath, const char *filePromptString) {
    char fileHeader[5] = {0};
    char *destPath = malloc(strlen(installPath) + 256 + 1);   // Full path of destination dir/file, the +256 is because mercypak strings can only be 255 chars max
    char *destPathAppend = destPath + strlen(installPath) + 1;  // Pointer to first char after the base install path in the destination path + 1 for the extra "/" we're gonna append
    bool mercypakV2 = false;

    sprintf(destPath, "%s/", installPath);

    uint32_t dirCount;
    uint32_t fileCount;
    bool success = true;

    success &= mappedFile_read(file, (uint8_t*) fileHeader, 4);
    QI_ASSERT(success && "fileHeader");
    success &= mappedFile_getUInt32(file, &dirCount);
    QI_ASSERT(success && "dirCount");
    success &= mappedFile_getUInt32(file, &fileCount);
    QI_ASSERT(success && "fileCount");

    /* printf("File header: %s, dirs %d files: %d\n", fileHeader, (int) dirCount, (int) fileCount); */

    /* Check if we're unpacking a V2 file, which does redundancy stuff. */

    if (util_stringEquals(fileHeader, MERCYPAK_V1_MAGIC)) {
        mercypakV2 = false;
    } else if (util_stringEquals(fileHeader, MERCYPAK_V2_MAGIC)) {
        mercypakV2 = true;
    } else {
        QI_ASSERT(false && "File header wrong");
        free(destPath);
        return false;
    }

    ad_ProgressBox *pbox = ad_progressBoxCreate("Windows 9x QuickInstall", dirCount, "Creating Directories (%s)...", filePromptString);

    QI_ASSERT(pbox);

    for (uint32_t d = 0; d < dirCount; d++) {
        uint8_t dirFlags;

        ad_progressBoxUpdate(pbox, d);

        success &= mappedFile_getUInt8(file, &dirFlags);
        success &= inst_getMercyPakString(file, destPathAppend);
        util_stringReplaceChar(destPathAppend, '\\', '/'); // DOS paths innit
        success &= (mkdir(destPath, dirFlags) == 0 || (errno == EEXIST));    // An error value is ok if the directory already exists. It means we can write to it. IT'S FINE.
    }

    ad_progressBoxDestroy(pbox);

    /*
     *  Extract and copy files from mercypak files
     */

    success = true;

    pbox = ad_progressBoxCreate("Windows 9x QuickInstall", mappedFile_getFileSize(file), "Copying Files (%s)...", filePromptString);

    QI_ASSERT(pbox);

    if (mercypakV2) {
        /* Handle mercypak v2 pack file with redundant files optimized out */

        inst_MercyPakFileDescriptor *filesToWrite = calloc(MERCYPAK_V2_MAX_IDENTICAL_FILES, sizeof(inst_MercyPakFileDescriptor));
        int *fileDescriptorsToWrite = calloc(MERCYPAK_V2_MAX_IDENTICAL_FILES, sizeof(int));
        uint8_t identicalFileCount = 0;

        QI_ASSERT(filesToWrite != NULL);

        for (uint32_t f = 0; f < fileCount;) {

            uint32_t fileSize;

            ad_progressBoxUpdate(pbox, mappedFile_getPosition(file));

            success &= mappedFile_getUInt8(file, &identicalFileCount);

            QI_ASSERT(identicalFileCount <= MERCYPAK_V2_MAX_IDENTICAL_FILES);

            for (uint32_t subFile = 0; subFile < identicalFileCount; subFile++) {
                success &= inst_getMercyPakString(file, destPathAppend);
                util_stringReplaceChar(destPathAppend, '\\', '/');

                fileDescriptorsToWrite[subFile] = open(destPath,  O_WRONLY | O_CREAT | O_TRUNC);
                QI_ASSERT(fileDescriptorsToWrite[subFile] >= 0);

                success &= mappedFile_read(file, &filesToWrite[subFile], MERCYPAK_V2_FILE_DESCRIPTOR_SIZE);
            }

            success &= mappedFile_getUInt32(file, &fileSize);

            success &= mappedFile_copyToFiles(file, identicalFileCount, fileDescriptorsToWrite, fileSize);

            for (uint32_t subFile = 0; subFile < identicalFileCount; subFile++) {
                success &= util_setDosFileTime(fileDescriptorsToWrite[subFile], filesToWrite[subFile].fileDate, filesToWrite[subFile].fileTime);
                success &= util_setDosFileAttributes(fileDescriptorsToWrite[subFile], filesToWrite[subFile].fileFlags);
                close(fileDescriptorsToWrite[subFile]);
            }

            f += identicalFileCount;
        }

        free(filesToWrite);
        free(fileDescriptorsToWrite);
    } else {

        inst_MercyPakFileDescriptor fileToWrite;

        for (uint32_t f = 0; f < fileCount; f++) {

            ad_progressBoxUpdate(pbox, mappedFile_getPosition(file));

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

    }

    ad_progressBoxDestroy(pbox);
    free(destPath);
    return success;
}

/* Inform user and setup boot sector and MBR. */
static bool inst_setupBootSectorAndMBR(util_Partition *part, bool setActiveAndDoMBR) {
    bool success = true;
    const util_BootSectorModifier *bsModifierList = NULL;
    // TODO: ui_showInfoBox("Setting up Master Boot Record and Boot sector...");
    QI_ASSERT(part != NULL);

    if (part->fileSystem == fs_fat16) {
        bsModifierList = __WIN98_FAT16_BOOT_SECTOR_MODIFIERS__;
    } else if (part->fileSystem == fs_fat32) {
        bsModifierList = __WIN98_FAT32_BOOT_SECTOR_MODIFIERS__;
    } else {
        QI_ASSERT(false && "invalid file system???");
    }

    success &= util_modifyAndwriteBootSectorToPartition(part, bsModifierList);
    if (setActiveAndDoMBR) {
        success &= util_writeMBRToDrive(part->parent, __MBR_WIN98__);
        char activateCmd[UTIL_MAX_CMD_LENGTH];
        snprintf(activateCmd, UTIL_MAX_CMD_LENGTH, "sfdisk --activate %s %zu", part->parent->device, part->indexOnParent);
        success &= (0 == ad_runCommandBox("Activating partition...", activateCmd));
    }
    return success;
}

/* Main installer process. Assumes the CDROM environment variable is set to a path with valid install.txt, FULL.866 and DRIVER.866 files. */
bool inst_main(int argc, char *argv[]) {
    MappedFile *sourceFile = NULL;
    size_t readahead = util_getProcSafeFreeMemory() * 6 / 10;
    util_HardDiskArray *hda = NULL;
    const char *registryUnpackFile = NULL;
    util_Partition *destinationPartition = NULL;
    inst_InstallStep currentStep = (inst_InstallStep) 0;
    size_t osVariantIndex = 0;

    bool installDrivers = false;
    bool formatPartition = false;
    bool setActiveAndDoMBR = false;
    bool quit = false;
    bool doReboot = false;
    bool installSuccess = true;
    bool goToNext = false;

    ad_init(LUNMERCY_BACKTITLE);

    setlocale(LC_ALL, "C.UTF-8");

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

    msg_disclaimer();

    while (!quit) {

        /* Basic concept:
         *  If goToNext is true, we advance one step, if it is false, we go back one step.
            (Can be circumvented by using "continue")
         *  */

        switch (currentStep) {
            case INSTALL_OSROOT_VARIANT_SELECT: {
                bool previousGoToNext = goToNext;
                size_t osVariantCount = 0;

                goToNext = inst_showOSVariantSelect(&osVariantIndex, &osVariantCount);

                if (osVariantCount == 1 && previousGoToNext == false) {
                    // If there's only one OS variant the variant select will just return "true"
                    // since the user doesn't get to press back we need to handle this here.
                    // so if the previous go to next was false we will go back outright.
                    goToNext = false;
                } else if (goToNext) {
                    if (sourceFile) {
                        mappedFile_close(sourceFile);
                        sourceFile = NULL;
                    }

                    sourceFile = inst_openSourceFile(osVariantIndex, INST_SYSROOT_FILE, readahead);

                    if (sourceFile == NULL) {
                        msg_fileOpenError(osVariantIndex, INST_SYSROOT_FILE);
                        continue;
                    }
                }

                break;
            }

            /* Main Menu:
             * Select Setup Action to execute */
            case INSTALL_MAIN_MENU: {
                switch (inst_showMainMenu()) {
                    case SETUP_ACTION_INSTALL:
                        currentStep = INSTALL_SELECT_DESTINATION_PARTITION;
                        continue;

                    case SETUP_ACTION_PARTITION_WIZARD:
                        currentStep = INSTALL_PARTITION_WIZARD;
                        continue;

                    case SETUP_ACTION_EXIT_TO_SHELL:
                        doReboot = false;
                        quit = true;
                        break;

                    default:
                        break;
                }

                continue;
            }

            /* Setup Action:
             * Partition wizard.
             * Select hard disk to partition. */
            case INSTALL_PARTITION_WIZARD: {
                // Go back to selector after this regardless what happens
                currentStep = INSTALL_MAIN_MENU;
                goToNext = false;

                inst_showPartitionWizard(&hda);
                continue;
            }

            /* Setup Action:
             * Start installation.
             * Select the destination partition */
            case INSTALL_SELECT_DESTINATION_PARTITION: {
                util_hardDiskArrayDestroy(hda);
                hda = inst_getSystemHardDisks();

                QI_ASSERT(hda != NULL);

                destinationPartition = inst_showPartitionSelector(hda);

                if (destinationPartition == NULL) {
                    // There was an error, the user canceled or we have no hard disks.
                    // Either way, we have to go back.
                    currentStep = INSTALL_MAIN_MENU;
                    continue;
                }

                goToNext = true;
                break;
            }

            /* Menu prompt:
             * Does user want to format the hard disk? */
            case INSTALL_FORMAT_PARTITION_PROMPT: {
                int answer = msg_askFormatPartition(destinationPartition);
                formatPartition = (answer == AD_YESNO_YES);
                goToNext = (answer != AD_CANCELED);
                break;
            }

            /* Menu prompt:
             * Does user want to update MBR and set the partition active? */
            case INSTALL_MBR_ACTIVE_BOOT_PROMPT: {
                int answer = msg_askOverwriteMBRAndSetActive(destinationPartition);
                setActiveAndDoMBR = (answer == AD_YESNO_YES);
                goToNext = (answer != AD_CANCELED);
                break;
            }


            /* Menu prompt:
             * Fast / Slow non-PNP HW detection? */
            case INSTALL_REGISTRY_VARIANT_PROMPT: {
                int answer = msg_askSkipPnpDetection();
                registryUnpackFile = (answer == AD_YESNO_YES) ? INST_FASTPNP_FILE
                                                              : INST_SLOWPNP_FILE;
                goToNext = (answer != AD_CANCELED);
                break;
            }

            /* Menu prompt:
             * Does the user want to install the base driver package? */
            case INSTALL_INTEGRATED_DRIVERS_PROMPT: {
                // It's optional, if the file doesn't exist, we don't have to ask
                if (util_fileExists(inst_getSourceFilePath(osVariantIndex, INST_DRIVER_FILE))) {
                    int response = msg_askInstallDrivers();
                    installDrivers = (response == AD_YESNO_YES);
                    goToNext = (response != AD_CANCELED);
                } else {
                    installDrivers = false;
                }

                break;
            }


            /* Do the actual install */
            case INSTALL_DO_INSTALL: {
                // Format partition
                if (formatPartition)
                    installSuccess = inst_formatPartition(destinationPartition);

                if (!installSuccess) {
                    msg_formatFailed(destinationPartition);
                    currentStep = INSTALL_MAIN_MENU;
                    continue;
                }

                installSuccess = util_mountPartition(destinationPartition);
                // If mounting failed, we will display a message and go back after.

                if (!installSuccess) {
                    msg_mountFailed(destinationPartition);
                    currentStep = INSTALL_MAIN_MENU;
                    continue;
                }

                // sourceFile is already opened at this point for readahead prebuffering
                installSuccess = inst_copyFiles(sourceFile, destinationPartition->mountPath, "Operating System");
                mappedFile_close(sourceFile);

                if (!installSuccess) {
                    msg_unpackFailed(INST_SYSROOT_FILE);
                    currentStep = INSTALL_MAIN_MENU;
                    continue;
                }

                // If the main data copy was successful, we move on to the driver file
                if (installSuccess && installDrivers) {
                    sourceFile = inst_openSourceFile(osVariantIndex, INST_DRIVER_FILE, readahead);
                    QI_ASSERT(sourceFile && "Failed to open driver file");
                    installSuccess = inst_copyFiles(sourceFile, destinationPartition->mountPath, "Driver Library");
                    mappedFile_close(sourceFile);
                }

                if (!installSuccess) {
                    msg_unpackFailed(INST_DRIVER_FILE);
                    currentStep = INSTALL_MAIN_MENU;
                    continue;
                }

                // If driver data copy was successful, install registry for selceted hardware detection variant
                if (installSuccess) {
                    sourceFile = inst_openSourceFile(osVariantIndex, registryUnpackFile, readahead);
                    QI_ASSERT(sourceFile && "Failed to open registry file");
                    installSuccess = inst_copyFiles(sourceFile, destinationPartition->mountPath, "Registry");
                    mappedFile_close(sourceFile);
                }

                if (!installSuccess) {
                    msg_unpackFailed(registryUnpackFile);
                    currentStep = INSTALL_MAIN_MENU;
                    continue;
                }

                util_unmountPartition(destinationPartition);

                // Final step: update MBR, boot sector and boot flag.
                if (installSuccess && setActiveAndDoMBR) {
                    installSuccess = inst_setupBootSectorAndMBR(destinationPartition, setActiveAndDoMBR);
                }

                if (!installSuccess) {
                    msg_failure();
                }

                // Determine what to do after installation
                switch (msg_askPostInstallAction()) {
                    case QI_POSTINSTALL_REBOOT:
                        doReboot = true;
                        quit = true;
                        break;
                    case QI_POSTINSTALL_MAIN_MENU:
                        currentStep = INSTALL_MAIN_MENU;
                        break;
                    case QI_POSTINSTALL_EXIT:
                        quit = true;
                        break;
                    default:
                        abort(); // Shouldn't arrive here.
                }

                quit = true;

                break;
            }

        }
        currentStep += goToNext ? 1 : -1;
    }

    // Flush filesystem writes clear screen yadayada...

    sync();

    util_hardDiskArrayDestroy(hda);

    system("clear");

    if (doReboot) {
        reboot(RB_AUTOBOOT);
    }

    ad_deinit();

    return installSuccess;
}
