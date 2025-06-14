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

#include "anbui/anbui.h"


#define CD_FILE_PATH_ROOT   (0)

typedef enum {
    INSTALL_WELCOME = 0,
    INSTALL_OSROOT_VARIANT_SELECT,
    INSTALL_MAIN_MENU,
    INSTALL_PARTITION_WIZARD,
    INSTALL_SELECT_DESTINATION_PARTITION,
    INSTALL_FORMAT_PARTITION_PROMPT,
    INSTALL_MBR_ACTIVE_BOOT_PROMPT,
    INSTALL_REGISTRY_VARIANT_PROMPT,
    INSTALL_CREGFIX_PROMPT,
    INSTALL_INTEGRATED_DRIVERS_PROMPT,
    INSTALL_DO_INSTALL,
} inst_InstallStep;


#pragma pack(1)

typedef struct {
    uint8_t fileFlags;
    uint16_t fileDate;
    uint16_t fileTime;
    uint32_t fileSize;
    int32_t fileno;
} inst_MercyPakFileDescriptor;

#pragma pack()

// -sizeof(int) because the fileno is not part of the descriptor read from the mercypak file
#define MERCYPAK_FILE_DESCRIPTOR_SIZE (sizeof(inst_MercyPakFileDescriptor) - sizeof(int))
// -sizeof(uint32_t) because the filesize is not part of the descriptor read from the mercypak v2 file
#define MERCYPAK_V2_FILE_DESCRIPTOR_SIZE ((MERCYPAK_FILE_DESCRIPTOR_SIZE) - sizeof(uint32_t))

#define MERCYPAK_V2_MAX_IDENTICAL_FILES (16)

#define MERCYPAK_V1_MAGIC "ZIEG"
#define MERCYPAK_V2_MAGIC "MRCY"

#define INST_CFDISK_CMD "cfdisk "
#define INST_COLS (74)

#define INST_SYSROOT_FILE "FULL.866"
#define INST_DRIVER_FILE  "DRIVER.866"
#define INST_SLOWPNP_FILE "SLOWPNP.866"
#define INST_FASTPNP_FILE "FASTPNP.866"

#define INST_CREGFIX_FILE "/../../extras/CREGFIX.COM"

#define INST_CDROM_IO_SIZE (512*1024)
#define INST_DISK_IO_SIZE (512*1024)

static const char *cdrompath = NULL;    // Path to install source media
static const char *cdromdev = NULL;     // Block device for install source media
                                        // ^ initialized in install_main

/* Gets the absolute CDROM path of a file. 
   osVariantIndex is the index for the source variant, 0 means from the root. */
static const char *inst_getCDFilePath(size_t osVariantIndex, const char *filepath) {
    static char staticPathBuf[1024];
    if (osVariantIndex > 0) {
        snprintf(staticPathBuf, sizeof(staticPathBuf), "%s/osroots/%zu/%s", cdrompath, osVariantIndex, filepath);
    } else {
        snprintf(staticPathBuf, sizeof(staticPathBuf), "%s/%s", cdrompath, filepath);
    }
    return staticPathBuf;
}

/* Opens a file from the source media. osVariantIndex is the index for the source variant, 0 means from the root. */
static inline MappedFile *inst_openSourceFile(size_t osVariantIndex, const char *filename, size_t readahead) {
    return mappedFile_open(inst_getCDFilePath(osVariantIndex, filename), readahead);
}

/* Shows disclaimer text */
static inline void inst_showDisclaimer() {
    ad_textFileBox("DISCLAIMER", inst_getCDFilePath(0, "install.txt"));
}

/* Shows welcome screen, returns false if user wants to exit to shell */
static inline void inst_showWelcomeScreen() {
    ad_okBox("Welcome", false, "Welcome to Windows 9x QuickInstall!");
}

/* Checks if given hard disk contains the installation source */
static inline bool inst_isInstallationSourceDisk(util_HardDisk *disk) {
    return (util_stringStartsWith(cdromdev, disk->device));
}

/* Checks if given partition is the installation source */
static inline bool inst_isInstallationSourcePartition(util_Partition *part) {
    return (util_stringEquals(cdromdev, part->device));
}

/* Tells the user he is trying to partition the install source disk */
static inline void inst_showInstallationSourceDiskError() {
    ad_okBox("Attention", false, "The selected disk contains the installation source.\nIt can not be partitioned.");
}

/* Tells the user he is trying to install to the install source partition */
static inline void inst_showInstallationSourcePartitionError() {
    ad_okBox("Attention", false, "The selected partition contains the installation source.\nIt cannot be the installation destination.");
}

/* Tells the user he is trying to install to a non-FAT partition */
static inline void inst_showUnsupportedFileSystemError() {
    ad_okBox("Attention", false, "The selected partition has an unsupported file system.\nIt cannot be the installation destination.");
}

/* Tells the user he is trying to install to a computer without hard disks. */
static inline void inst_noHardDisksFoundError() {
    ad_okBox("Attention", false, "No hard disks found!\nPlease install a hard disk and try again!");
}

/* Tells the user about an oopsie trying to open a file for reading. */
static inline void inst_showFileError() {
    ad_okBox("Attention", false, "ERROR: A problem occurred handling a file for this OS variant.\n(%d: %s)", errno, strerror(errno));
}

static inline util_HardDiskArray *inst_getSystemHardDisks() {
    ad_setFooterText("Obtaining System Hard Disk Information...");
    util_HardDiskArray *ret = util_getSystemHardDisks();
    ad_clearFooter();
    return ret;
}

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
        char tmpInfPath[256];
        char tmpVariantLabel[128];

        snprintf(tmpInfPath, sizeof(tmpInfPath), "%s/osroots/%zu/win98qi.inf", cdrompath, (*variantCount) + 1); // Variant index starts at 1

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

/* Show partition wizard, returns true if user finished or false if he selected BACK. */
static void inst_showPartitionWizard(util_HardDiskArray *hdds) {
    char cfdiskCmd[UTIL_MAX_CMD_LENGTH];
    int menuResult;

    QI_ASSERT(hdds);

    if (hdds->count == 0) {
        inst_noHardDisksFoundError();
        return;
    }

    while (1) {
        ad_Menu *menu = ad_menuCreate("Partition Wizard", "Select the Hard Disk you wish to partition.", true);

        QI_ASSERT(menu);

        for (size_t i = 0; i < hdds->count; i++) {
            ad_menuAddItemFormatted(menu, "%s [%s] - Size: %llu MB %s",
                hdds->disks[i].device,
                hdds->disks[i].model,
                hdds->disks[i].size / 1024ULL / 1024ULL,
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
            inst_showInstallationSourceDiskError();
            continue;
        }

        // Invoke cfdisk command for chosen drive.
        snprintf(cfdiskCmd, UTIL_MAX_CMD_LENGTH, "%s%s", INST_CFDISK_CMD, hdds->disks[menuResult].device);      
        system(cfdiskCmd);

        ad_restore();
        ad_okBox("Attention", false,
            "Remember to answer 'yes' to the format prompt\n"
            "if you are installing on a partition you've just created!");
    }
}

/* Shows partition selector. Returns pointer to the selected partition. A return value of NULL means the user wants to go back. */
static util_Partition *inst_showPartitionSelector(util_HardDiskArray *hdds) {
    int menuResult;
    util_Partition *result = NULL;

    QI_ASSERT(hdds);

    if (hdds->count == 0) {
        inst_noHardDisksFoundError();
        return NULL;
    }

    while (1) {
        ad_Menu *menu = ad_menuCreate("Installation Destination", "Select the partition you wish to install to.", true);

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
            inst_showInstallationSourcePartitionError();
            continue;
        }
        
        if (result->fileSystem == fs_unsupported || result->fileSystem == fs_none) {
            inst_showUnsupportedFileSystemError();
            continue;
        }

        return result;
    }
}

/* Asks user if he wants to format selected partition. Returns true if so. */
static inline int inst_formatPartitionDialog(util_Partition *part) {
    return ad_yesNoBox("Confirm", true,
        "You have chosen the partition '%s'.\n"
        "Would you like to format it before the installation (recommended)?\n",
        part->device);
}

static bool inst_formatPartition(util_Partition *part) {
    char formatCmd[UTIL_MAX_CMD_LENGTH];
    bool ret = util_getFormatCommand(part, part->fileSystem, formatCmd, UTIL_MAX_CMD_LENGTH);
    QI_ASSERT(ret && "GetFormatCommand");
    return (0 == ad_runCommandBox("Formatting partition...", formatCmd));
}

/* Asks user if he wants to overwrite the MBR and set the partition active. Returns true if so. */
static inline int inst_askUserToOverwriteMBRAndSetActive(util_Partition *part) {
    return ad_yesNoBox("Confirm", true,
        "You have chosen the partition '%s'.\n"
        "Would you like to overwrite the Master Boot Record (MBR)\n"
        "and set the partition active (recommended)?", 
        part->device);
}

/* Show message box informing user that formatting failed. */
static inline void inst_showFailedFormat(util_Partition *part) {
    ad_okBox("Error", false,
        "The partition %s could not be formatted.\n"
        "The last recorded error was: '%s'.\n"
        "There may be a disk problem.\n"
        "Try re-partitioning the disk or using another partition.\n"
        "Returning to the partition selector.",
        part->device, strerror(errno));
}

/* Show message box informing user that mount failed*/
static inline void inst_showFailedMount(util_Partition *part) {
    ad_okBox("Error", false,
        "The partition %s could not be accessed.\n"
        "The last recorded error was: '%s'.\n"
        "There may be a disk problem.\n"
        "You can try formatting the partition.\n"
        "Returning to the partition selector.",
        part->device, strerror(errno));
}

/* Show message box informing user that copying failed*/
static inline void inst_showFailedCopy(const char *sourceFile) {
    ad_okBox("Error", false,
        "An error occurred while unpacking '%s'\n"
        "for this operating system variant.\n"
        "The last recorded error was: '%s'.\n"
        "There may be a disk problem.\n"
        "You can try a different source / destination disk.\n"
        "Returning to the partition selector.", 
        sourceFile,
        strerror(errno));
}


/* Ask user if he wants to install driver package */
static inline int inst_showDriverPrompt() {
    return ad_yesNoBox("Selection", true, "Would you like to install the integrated device drivers?");
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

    /*
        TODO: ERROR HANDLING
     */

    ad_progressBoxDestroy(pbox);

    free(destPath);
    return success;
}

/* Inform user and setup boot sector and MBR. */
static bool inst_setupBootSectorAndMBR(util_Partition *part, bool setActiveAndDoMBR) {
    bool success = true;
    // TODO: ui_showInfoBox("Setting up Master Boot Record and Boot sector...");
    success &= util_writeWin98BootSectorToPartition(part);
    if (setActiveAndDoMBR) {
        success &= util_writeWin98MBRToDrive(part->parent);
        char activateCmd[UTIL_MAX_CMD_LENGTH];
        snprintf(activateCmd, UTIL_MAX_CMD_LENGTH, "sfdisk --activate %s %zu", part->parent->device, part->indexOnParent);
        success &= (0 == ad_runCommandBox("Activating partition...", activateCmd));
    }
    return success;
}

/* Show success screen. Ask user if he wants to reboot */
static inline bool inst_showSuccessAndAskForReboot() {
    // Returns TRUE (meaning reboot = true) if YES (0) happens. sorry for the confusion.
    ad_Menu *menu = ad_menuCreate("Windows 9x QuickInstall: Success", 
        "The installation was successful.\n"
        "Would you like to reboot or exit to a shell?", 
        false);

    QI_ASSERT(menu);

    ad_menuAddItemFormatted(menu, "Reboot");
    ad_menuAddItemFormatted(menu, "Exit to shell");

    int menuResult = ad_menuExecute(menu);

    ad_menuDestroy(menu);

    return menuResult == 0;
}

/* Show failure screen :( */
static inline void inst_showFailMessage() {
    ad_okBox("Error!", false,
        "There was a problem during installation! :(\n"
        "You can press ENTER to get to a shell and inspect the damage.");
}

/* Asks user which version of the hardware detection scheme he wants */
static const char *inst_askUserForRegistryVariant(void) {
    const char *optionFiles[] = { 
        "FASTPNP.866", 
        "SLOWPNP.866"
    };
    const char *optionLabels[] = { 
        "Fast hardware detection, skipping most non-PNP devices.",
        "Full hardware detection, including ALL non-PNP devices."
    };

    int menuResult = ad_menuExecuteDirectly("Select hardware detection method", true, 
        util_arraySize(optionLabels), optionLabels, 
        "Please select the hardware detection method to use.");

    if (menuResult == AD_CANCELED) {
        return NULL;
    }

    QI_ASSERT(menuResult < (int) util_arraySize(optionLabels));
    return optionFiles[menuResult];
}

static inline int *inst_showCregfixInstallPrompt(int addCregfix) {
    int answer = ad_yesNoBox("Cregfix", true,
        "Do you want to install CREGFIX and add it to AUTOEXEC.BAT?\n"
        "This will fix booting the new Win9x installation on\n"
        "systems with some Ryzen and all Intel 13th gen and later processors.\n"
        "It is recommended to do so for these processors.\n"
        "For other processors, it is not needed and can be skipped.\n");
}

static bool inst_addCregfixToAutoexec(const char *installPath) {
    char autoexecPath[1024];
    snprintf(autoexecPath, sizeof(autoexecPath), "%s/AUTOEXEC.BAT", installPath);

    // Check if AUTOEXEC.BAT exists
    if (!util_fileExists(autoexecPath)) {
        ad_okBox("Error", false, "AUTOEXEC.BAT not found in the installation path.\n"
            "Cannot add CREGFIX to AUTOEXEC.BAT.");
        return false;
    }

    // Read existing contents
    FILE *autoexecFile = fopen(autoexecPath, "r");
    char *buffer = NULL;
    size_t length = 0;
    bool hasCregfix = false;

    if (autoexecFile) {
        fseek(autoexecFile, 0, SEEK_END);
        length = ftell(autoexecFile);
        fseek(autoexecFile, 0, SEEK_SET);

        buffer = malloc(length + 1);
        if (!buffer) {
            fclose(autoexecFile);
            ad_okBox("Error", false, "Memory allocation failed while modifying AUTOEXEC.BAT.");
            return false;
        }

        fread(buffer, 1, length, autoexecFile);
        buffer[length] = '\0';

        if (strstr(buffer, "CREGFIX.COM")) {
            hasCregfix = true;
        }

        fclose(autoexecFile);
    }

    // Rewrite with CREGFIX at the top
    autoexecFile = fopen(autoexecPath, "w");
    if (!autoexecFile) {
        ad_okBox("Error", false, "Failed to open AUTOEXEC.BAT for writing.");
        free(buffer);
        return false;
    }

    // Write header and CREGFIX line
    fprintf(autoexecFile, "@echo off\n");
    if (!hasCregfix) {
        fprintf(autoexecFile, "C:\\CREGFIX\\CREGFIX.COM\n");
    }

    // Restore original content, skipping duplicates
    if (buffer) {
        char *line = strtok(buffer, "\r\n");
        while (line) {
            if (strstr(line, "@echo off") == NULL && strstr(line, "CREGFIX.COM") == NULL) {
                fprintf(autoexecFile, "%s\n", line);
            }
            line = strtok(NULL, "\r\n");
        }
        free(buffer);
    }

    fclose(autoexecFile);
    return true;
}

/* Main installer process. Assumes the CDROM environment variable is set to a path with valid install.txt, FULL.866 and DRIVER.866 files. */
bool inst_main() {
    MappedFile *sourceFile = NULL;
    size_t readahead = util_getProcSafeFreeMemory() * 6 / 10;
    util_HardDiskArray *hda = NULL;
    const char *registryUnpackFile = NULL;
    util_Partition *destinationPartition = NULL;
    inst_InstallStep currentStep = INSTALL_WELCOME;
    size_t osVariantIndex = 0;

    bool addCregfix = false;
    bool installDrivers = false;
    bool formatPartition = false;
    bool setActiveAndDoMBR = false;
    bool quit = false;
    bool doReboot = false;
    bool installSuccess = true;
    bool goToNext = false;

    ad_init(LUNMERCY_BACKTITLE);

    setlocale(LC_ALL, "C.UTF-8");

    cdrompath = getenv("CDROM");
    cdromdev = getenv("CDDEV");

    QI_ASSERT(cdrompath);
    QI_ASSERT(cdromdev);

 
    inst_showDisclaimer();

    while (!quit) {

        /* Basic concept:
         *  If goToNext is true, we advance one step, if it is false, we go back one step.
            (Can be circumvented by using "continue")
         *  */

        switch (currentStep) {
            case INSTALL_WELCOME:
                inst_showWelcomeScreen();
                goToNext = true;
                break;

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
                        inst_showFileError();
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

                util_hardDiskArrayDestroy(hda);
                hda = inst_getSystemHardDisks();

                QI_ASSERT(hda != NULL);

                inst_showPartitionWizard(hda);
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
                int answer = inst_formatPartitionDialog(destinationPartition);
                formatPartition = (answer == AD_YESNO_YES);
                goToNext = (answer != AD_CANCELED);
                break;
            }

            /* Menu prompt:
             * Does user want to update MBR and set the partition active? */
            case INSTALL_MBR_ACTIVE_BOOT_PROMPT: {
                int answer = inst_askUserToOverwriteMBRAndSetActive(destinationPartition);
                setActiveAndDoMBR = (answer == AD_YESNO_YES);
                goToNext = (answer != AD_CANCELED);
                break;
            }


            /* Menu prompt:
             * Fast / Slow non-PNP HW detection? */
            case INSTALL_REGISTRY_VARIANT_PROMPT: {
                registryUnpackFile = inst_askUserForRegistryVariant();
                goToNext = (registryUnpackFile != NULL);
                break;
            }

            /* Menu prompt:
             * Install CREGFIX and add to AUTOEXEC.BAT? */
            case INSTALL_CREGFIX_PROMPT: {
                int answer = inst_showCregfixInstallPrompt();
                addCregfix = (answer == AD_YESNO_YES);
                goToNext = (answer != AD_CANCELED);
                break;
            }

            /* Menu prompt:
             * Does the user want to install the base driver package? */
            case INSTALL_INTEGRATED_DRIVERS_PROMPT: {
                // It's optional, if the file doesn't exist, we don't have to ask
                if (util_fileExists(inst_getCDFilePath(osVariantIndex, INST_DRIVER_FILE))) {
                    int response = inst_showDriverPrompt();
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
                    inst_showFailedFormat(destinationPartition);
                    currentStep = INSTALL_MAIN_MENU;
                    continue;
                }

                installSuccess = util_mountPartition(destinationPartition);
                // If mounting failed, we will display a message and go back after.

                if (!installSuccess) {
                    inst_showFailedMount(destinationPartition);
                    currentStep = INSTALL_MAIN_MENU;
                    continue;
                }

                // sourceFile is already opened at this point for readahead prebuffering
                installSuccess = inst_copyFiles(sourceFile, destinationPartition->mountPath, "Operating System");
                mappedFile_close(sourceFile);

                if (!installSuccess) {
                    inst_showFailedCopy(INST_DRIVER_FILE);
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
                    inst_showFailedCopy(INST_DRIVER_FILE);
                    currentStep = INSTALL_MAIN_MENU;
                    continue;
                }

                // If driver data copy was successful, install registry for selected hardware detection variant
                if (installSuccess) {
                    sourceFile = inst_openSourceFile(osVariantIndex, registryUnpackFile, readahead);
                    QI_ASSERT(sourceFile && "Failed to open registry file");
                    installSuccess = inst_copyFiles(sourceFile, destinationPartition->mountPath, "Registry");
                    mappedFile_close(sourceFile);
                }

                if (!installSuccess) {
                    inst_showFailedCopy(registryUnpackFile);
                    currentStep = INSTALL_MAIN_MENU;
                    continue;
                }

                // If registry copy was successful, install CREGFIX to fix booting on some Ryzen and all Intel 13th gen and later systems if requested
                if (installSuccess && addCregfix) {
                    sourceFile = inst_openSourceFile(osVariantIndex, INST_CREGFIX_FILE, readahead);
                    QI_ASSERT(sourceFile && "Failed to open CREGFIX file");
                    // Check if CREGFIX file was copied, if not, kick the user back to the main menu
                    installSuccess = inst_copyFiles(sourceFile, destinationPartition->mountPath, "CREGFIX");
                    mappedFile_close(sourceFile);
                    // Check if CREGFIX was added to AUTOEXEC.BAT, if not, kick the user back to the main menu
                    installSuccess = inst_addCregfixToAutoexec(destinationPartition->mountPath);
                }

                if (!installSuccess) {
                    inst_showFailedCopy(INST_CREGFIX_FILE);
                    currentStep = INSTALL_MAIN_MENU;
                    continue;
                }

                util_unmountPartition(destinationPartition);

                // Final step: update MBR, boot sector and boot flag.
                if (installSuccess && setActiveAndDoMBR) {
                    installSuccess = inst_setupBootSectorAndMBR(destinationPartition, setActiveAndDoMBR);
                }

                if (installSuccess) {                    
                    doReboot = inst_showSuccessAndAskForReboot();
                } else {
                    inst_showFailMessage();
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