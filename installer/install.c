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
    INSTALL_PARTITION_WIZARD,
    INSTALL_SELECT_DESTINATION_PARTITION,
    INSTALL_FORMAT_PARTITION_PROMPT,
    INSTALL_MBR_ACTIVE_BOOT_PROMPT,
    INSTALL_REGISTRY_VARIANT_PROMPT,
    INSTALL_INTEGRATED_DRIVERS_PROMPT,
    INSTALL_DO_INSTALL,
} inst_InstallStep;


#pragma pack(1)

typedef struct {
    uint8_t fileFlags;
    uint16_t fileDate;
    uint16_t fileTime;
    uint32_t fileSize;
    int fileno;
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
static inline mappedFile *inst_openSourceFile(size_t osVariantIndex, const char *filename, size_t readahead) {
    return mappedFile_open(inst_getCDFilePath(osVariantIndex, filename), readahead);
}

/* Shows disclaimer text */
static inline void inst_showDisclaimer() {
    ad_textFileBoxDirect("DISCLAIMER", inst_getCDFilePath(0, "install.txt"));
}

/* Shows welcome screen, returns false if user wants to exit to shell */
static inline bool inst_showWelcomeScreen() {
    return (0 == ad_okBox("Welcome", false, "Welcome to Windows 9x QuickInstall!"));
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
    ad_okBox("Attention", false, "ERROR: A problem occured handling a file for this OS variant.\n(%d: %s)", errno, strerror(errno));
}

static inline util_HardDiskArray inst_getSystemHardDisks() {
    ad_setFooterText("Obtaining System Hard Disk Information...");
    util_HardDiskArray ret = util_getSystemHardDisks();
    ad_clearFooter();
    return ret;
}

/* 
    Shows OS Variant select. This is a bit peculiar because the dialog is not shown if
    there is only one OS variant choice. In that case it always returns "true".
    Otherwise it can return "false" if the user selected BACK.
*/
static bool inst_showOSVariantSelect(size_t *variantIndex, size_t *variantCount) {
    char osRootsDir[256];
    int menuResult;
    struct dirent *entry;
    struct stat st;
    ad_Menu *menu = ad_menuCreate("Installation Variant", "Select the operating system variant you wish to install.", true);

    QI_ASSERT(menu);

    // Get available OS variants.
    *variantCount = 0;

    snprintf(osRootsDir, sizeof(osRootsDir), "%s/osroots", cdrompath);

    DIR *dir = opendir(osRootsDir);
    QI_ASSERT(dir);

    while ((entry = readdir(dir)) != NULL) {
        size_t currentVariantIndex;
        char path[512];
        char currentVariantLabel[256];

        snprintf(path, sizeof(path), "%s/%s", osRootsDir, entry->d_name);

        if (stat(path, &st) == -1) {
            perror("stat");
            continue;
        }

        if (S_ISDIR(st.st_mode) && entry->d_name[0] != '.') {
            // Get the label from the "WIN98QI.INF" file.
            currentVariantIndex = atoi(entry->d_name);
            const char *win98qiInfPath = inst_getCDFilePath(currentVariantIndex, "win98qi.inf");

            QI_ASSERT(currentVariantIndex > 0 && util_fileExists(win98qiInfPath));
            util_readFirstLineFromFileIntoBuffer(win98qiInfPath, currentVariantLabel);      

            ad_menuAddItemFormatted(menu, "%zu: %s", currentVariantIndex, currentVariantLabel);

            *variantCount += 1;
        }
    }

    closedir(dir);

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
static bool inst_showPartitionWizard(util_HardDiskArray *hdds) {
    char cfdiskCmd[UTIL_MAX_CMD_LENGTH];
    int menuResult;

    QI_ASSERT(hdds);
    QI_ASSERT(hdds->count > 0);

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
            return false;
        
        if (menuResult == (int) hdds->count)
            return true;
        
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
        "An error occured while unpacking '%s'\n"
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
static inline bool inst_getMercyPakString(mappedFile *file, char *dst) {
    bool success;
    uint8_t count;
    success = mappedFile_getUInt8(file, &count);
    success &= mappedFile_read(file, (uint8_t*) dst, (size_t) (count));
    dst[(size_t) count] = 0x00;
    return success;
}

static bool inst_copyFiles(mappedFile *file, const char *installPath) {
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

    ad_ProgressBox *pbox = ad_progressBoxCreate("Windows 9x QuickInstall", "Creating Directories...", dirCount);

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

    pbox = ad_progressBoxCreate("Windows 9x QuickInstall", "Copying Files...", file->size);

    QI_ASSERT(pbox);

    if (mercypakV2) {
        /* Handle mercypak v2 pack file with redundant files optimized out */

        inst_MercyPakFileDescriptor *filesToWrite = calloc(MERCYPAK_V2_MAX_IDENTICAL_FILES, sizeof(inst_MercyPakFileDescriptor));
        uint8_t identicalFileCount = 0;

        QI_ASSERT(filesToWrite != NULL);

        for (uint32_t f = 0; f < fileCount;) {

            uint32_t fileSize;

            ad_progressBoxUpdate(pbox, file->pos);

            success &= mappedFile_getUInt8(file, &identicalFileCount);

            QI_ASSERT(identicalFileCount <= MERCYPAK_V2_MAX_IDENTICAL_FILES);

            for (uint32_t subFile = 0; subFile < identicalFileCount; subFile++) {
                success &= inst_getMercyPakString(file, destPathAppend);
                util_stringReplaceChar(destPathAppend, '\\', '/');

                filesToWrite[subFile].fileno = open(destPath,  O_WRONLY | O_CREAT | O_TRUNC);
                QI_ASSERT(filesToWrite[subFile].fileno >= 0);

                success &= mappedFile_read(file, &filesToWrite[subFile], MERCYPAK_V2_FILE_DESCRIPTOR_SIZE);
            }

            success &= mappedFile_getUInt32(file, &fileSize);

            for (uint32_t subFile = 0; subFile < identicalFileCount; subFile++) {
                bool advanceMappedFileAfterCopy = (subFile + 1) == identicalFileCount; // On the last file we advance the mappedfile after copying.

                success &= mappedFile_copyToFile(file, filesToWrite[subFile].fileno, fileSize, advanceMappedFileAfterCopy);

                success &= util_setDosFileTime(filesToWrite[subFile].fileno, filesToWrite[subFile].fileDate, filesToWrite[subFile].fileTime);
                success &= util_setDosFileAttributes(filesToWrite[subFile].fileno, filesToWrite[subFile].fileFlags);

                close(filesToWrite[subFile].fileno);
            }

            f += identicalFileCount;
        }

        free(filesToWrite);
    } else {

        inst_MercyPakFileDescriptor fileToWrite;

        for (uint32_t f = 0; f < fileCount; f++) {

            ad_progressBoxUpdate(pbox, file->pos);

            /* Mercypak file metadata (see mercypak.txt) */

            success &= inst_getMercyPakString(file, destPathAppend);    // First, filename string
            util_stringReplaceChar(destPathAppend, '\\', '/');          // DOS paths innit

            success &= mappedFile_read(file, &fileToWrite, MERCYPAK_FILE_DESCRIPTOR_SIZE);

            int outfd = open(destPath,  O_WRONLY | O_CREAT | O_TRUNC);
            QI_ASSERT(outfd >= 0);

            success &= mappedFile_copyToFile(file, outfd, fileToWrite.fileSize, true);

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
        snprintf(activateCmd, UTIL_MAX_CMD_LENGTH, "sfdisk --activate %s %d", part->parent->device, part->index);
        ad_runCommandBox("Activating partition...", activateCmd);
    }
    return success;
}

/* Show success screen. Ask user if he wants to reboot */
static inline bool inst_showSuccessAndAskForReboot() {
    // Returns TRUE (meaning reboot = true) if YES (0) happens. sorry for the confusion.
    return (0 == ad_yesNoBox("Reboot?", false,
        "Finished the Windows 98 Installation.\n"
        "Would you like to Reboot or exit to a shell?"));
}

/* Show failure screen :( */
static inline void inst_showFailMessage() {
    ad_okBox("Error!", false,
        "There was a problem during installation! :(\n"
        "You can press ENTER to get to a shell and inspect the damage.");
}

/* Asks user which version of the hardware detection scheme he wants */
static const char *inst_askUserForRegistryVariant(uint32_t osVariantIndex) {
    static const char fastpnp[] = "FASTPNP.866";
    static const char slowpnp[] = "SLOWPNP.866";
    const char *optionFiles[] = { fastpnp, slowpnp };
    const char *optionLabels[] = { 
        "Fast hardware detection, skipping most non-PNP devices.",
        "Full hardware detection, including ALL non-PNP devices."
    };

    // only do this if SLOWPNP exists, as windows 95 doesn't have a non-pnp init so we have to apply fastpnp anyway...
    QI_ASSERT(util_fileExists(inst_getCDFilePath(osVariantIndex, fastpnp)));
    if (!util_fileExists(inst_getCDFilePath(osVariantIndex, slowpnp)))
        return fastpnp;
    
    int menuResult = ad_menuExecuteDirectly("Select hardware detection method", true, 
        util_arraySize(optionLabels), optionLabels, 
        "Please select the hardware detection method to use.");

    // Cancelled?
    if (menuResult < 0) {
        return NULL;
    }

    QI_ASSERT(menuResult < (int) util_arraySize(optionLabels));
    return optionFiles[menuResult];
}

/* Main installer process. Assumes the CDROM environment variable is set to a path with valid install.txt, FULL.866 and DRIVER.866 files. */
bool inst_main() {
    mappedFile *sourceFile = NULL;
    size_t readahead = util_getProcSafeFreeMemory() / 2;
    util_HardDiskArray hda = { 0, NULL };
    const char *registryUnpackFile = NULL;
    util_Partition *destinationPartition = NULL;
    inst_InstallStep currentStep = INSTALL_WELCOME;
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

    cdrompath = getenv("CDROM");
    cdromdev = getenv("CDDEV");

    QI_ASSERT(cdrompath);
    QI_ASSERT(cdromdev);

 
    inst_showDisclaimer();

    while (!quit) {
        switch (currentStep) {
            case INSTALL_WELCOME:
                goToNext = inst_showWelcomeScreen();
                quit |= !goToNext; // First step so we will quit if user pressed exit to shell ...
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
            case INSTALL_PARTITION_WIZARD: {
                if (hda.disks == NULL)
                    hda = inst_getSystemHardDisks();

                if (hda.count == 0) {
                    inst_noHardDisksFoundError();
                    continue;
                }

                goToNext = inst_showPartitionWizard(&hda);
                util_hardDiskArrayDeinit(hda);
                hda = inst_getSystemHardDisks();
                break;
            }
            case INSTALL_SELECT_DESTINATION_PARTITION: {
                destinationPartition = inst_showPartitionSelector(&hda);
                goToNext = (destinationPartition != NULL);
                break;
            }
            case INSTALL_FORMAT_PARTITION_PROMPT: {
                // ask if partition should be formatted & do so if yes
                int answer = inst_formatPartitionDialog(destinationPartition);
                formatPartition = (answer == AD_YESNO_YES);
                goToNext = (answer != AD_CANCELED);
                break;
            }

            case INSTALL_MBR_ACTIVE_BOOT_PROMPT: {
                int answer = inst_askUserToOverwriteMBRAndSetActive(destinationPartition);
                setActiveAndDoMBR = (answer == AD_YESNO_YES);
                goToNext = (answer != AD_CANCELED);
                break;
            }

            case INSTALL_REGISTRY_VARIANT_PROMPT: {
                registryUnpackFile = inst_askUserForRegistryVariant(osVariantIndex);
                goToNext = (registryUnpackFile != NULL);
                break;
            }
  
            case INSTALL_INTEGRATED_DRIVERS_PROMPT: {
                // If driver package doesn't exist, we need not ínstall it
                if (util_fileExists(inst_getCDFilePath(osVariantIndex, INST_DRIVER_FILE))) {
                    int response = inst_showDriverPrompt();
                    installDrivers = (response == AD_YESNO_YES);
                    goToNext = (response != AD_CANCELED);
                } else {
                    installDrivers = false;
                }

                break;
            }

            case INSTALL_DO_INSTALL: {
                // Format partition
                if (formatPartition)
                    installSuccess = inst_formatPartition(destinationPartition);

                if (!installSuccess) {
                    inst_showFailedFormat(destinationPartition);
                    currentStep = INSTALL_SELECT_DESTINATION_PARTITION;
                    continue;
                }

                installSuccess = util_mountPartition(destinationPartition);
                // If mounting failed, we will display a message and go back after.

                if (!installSuccess) {
                    inst_showFailedMount(destinationPartition);
                    currentStep = INSTALL_SELECT_DESTINATION_PARTITION;
                    continue;
                }

                // sourceFile is already opened at this point for readahead prebuffering
                installSuccess = inst_copyFiles(sourceFile, destinationPartition->mountPath);
                mappedFile_close(sourceFile);

                if (!installSuccess) {
                    inst_showFailedCopy(INST_DRIVER_FILE);
                    currentStep = INSTALL_SELECT_DESTINATION_PARTITION;
                    continue;
                }

                // If the main data copy was successful, we move on to the driver file
                if (installSuccess && installDrivers) {
                    sourceFile = inst_openSourceFile(osVariantIndex, INST_DRIVER_FILE, readahead);
                    QI_ASSERT(sourceFile && "Failed to open driver file");
                    installSuccess = inst_copyFiles(sourceFile, destinationPartition->mountPath);
                    mappedFile_close(sourceFile);
                }

                if (!installSuccess) {
                    inst_showFailedCopy(INST_DRIVER_FILE);
                    currentStep = INSTALL_SELECT_DESTINATION_PARTITION;
                    continue;
                }

                // If driver data copy was successful, install registry for selceted hardware detection variant
                if (installSuccess) {
                    sourceFile = inst_openSourceFile(osVariantIndex, registryUnpackFile, readahead);
                    QI_ASSERT(sourceFile && "Failed to open registry file");
                    installSuccess = inst_copyFiles(sourceFile, destinationPartition->mountPath);
                    mappedFile_close(sourceFile);
                }

                if (!installSuccess) {
                    inst_showFailedCopy(registryUnpackFile);
                    currentStep = INSTALL_SELECT_DESTINATION_PARTITION;
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

    util_hardDiskArrayDeinit(hda);

    system("clear");

    if (doReboot) {
        reboot(RB_AUTOBOOT);
    }

    ad_deinit();

    return installSuccess;
}