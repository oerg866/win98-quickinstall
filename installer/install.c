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
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/reboot.h>
#include <fcntl.h>
#include <sched.h>
#include <unistd.h>
#include <errno.h>
#include <utime.h>
#include <unistd.h>

#include "ringbuf.h"
#include "util.h"
#include "install_ui.h"


typedef enum {
    INSTALL_WELCOME = 0,
    INSTALL_PARTITION_WIZARD,
    INSTALL_SELECT_DESTINATION_PARTITION,
    INSTALL_FORMAT_PARTITION_CHECK,
    INSTALL_MBR_ACTIVE_BOOT_PROMPT,
    INSTALL_REGISTRY_VARIANT_PROMPT,
    INSTALL_INTEGRATED_DRIVERS_PROMPT,
    INSTALL_DO_INSTALL,
} inst_InstallStep;


// The file the dialog option will be printed to...
#define INST_DIALOG_OPTION_TMP_FILE "/tmp/0"

#define INST_CFDISK_CMD "cfdisk "
#define INST_COLS (74)

#define INST_SYSROOT_FILE "FULL.866"
#define INST_DRIVER_FILE  "DRIVER.866"
#define INST_SLOWPNP_FILE "SLOWPNP.866"
#define INST_FASTPNP_FILE "FASTPNP.866"

#define INST_CDROM_IO_SIZE (512*1024)
#define INST_DISK_IO_SIZE (512*1024)

/* Arguments for background OS data buffer thread */
typedef struct {
    char *filename;             // Filename to buffer
    ringbuf *buf;               // Ringbuffer to buffer into
    volatile bool *quitFlag;             // Pointer to quit flag
} inst_FillerThreadArgs;

/* Gets the absolute CDROM path of a file. */
static const char *inst_getCDFilePath(const char *filepath) {
    static char staticPathBuf[1024];
    const char *cdrompath = getenv("CDROM");
    assert(cdrompath);
    snprintf(staticPathBuf, sizeof(staticPathBuf), "%s/%s", cdrompath, filepath);
    return staticPathBuf;
}

/* Thread function for background OS data buffer thread */
static void *inst_fillerThreadFunc(void* threadParam) {
    inst_FillerThreadArgs *args = (inst_FillerThreadArgs*) threadParam;
    /* printf("FILLER THREAD STARTED filename %s ringbuf %p\n", args->filename, args->buf); */

    ringbuf *buf = args->buf;
    FILE *file = fopen(args->filename, "rb");
    assert(file);

    bool endOfFile = false;
    volatile bool *quit = args->quitFlag;

    size_t ringbufSpace;

    while (!endOfFile && !(*quit)) {        
        ringbufSpace = rb_space(buf);
        if (ringbufSpace >= INST_CDROM_IO_SIZE) {
            // We dipped below the filler threshold. Which means we must now fill all of it, in CDROM IO size multiples.
            size_t bytesToRead = ringbufSpace & (~INST_CDROM_IO_SIZE);
            rb_fread_write(buf, bytesToRead, file);
            if (ferror_unlocked(file)) {
                // ERROR
                printf("Read error!\n");
                perror(NULL);
                // TODO: Handle read error
                assert(false);
            } else if (feof_unlocked(file)) {
                // EOF
                endOfFile = true;
            }
        }
        sched_yield();
    }

    fclose(file);
    /* printf("THREAD DONE\n"); */
    pthread_exit(threadParam);
}


/* Stop the background thread buffering Win98 install files */
static void inst_stopFillerThread(pthread_t thread) {
    inst_FillerThreadArgs *ret = NULL;
    pthread_join(thread, (void*) &ret);

    // Free the parameters allocated previously
    if (ret) {
        free(ret->filename);
        free(ret);
    }
}

/* Start the background thread buffering Win98 install files. 
   oldHandle can be NULL, point to a handle of an existing thread, which will be stopped before starting a new one.
   Filename is Mercypak file to buffer, relative to the BASE DIRECTORY OF THE INSTALL MEDIA
   quitFlag is a pointer to a bool that indicates when the buffering should stop.*/
static pthread_t inst_startFillerThread(pthread_t oldHandle, ringbuf *buf, const char *filename, bool *quitFlag) {
    pthread_t ret;
    inst_FillerThreadArgs *args = calloc(sizeof(inst_FillerThreadArgs), 1);
    
    assert(args);

    // Stop old thread if one was given
    if (oldHandle) {
        *quitFlag = true;
        inst_stopFillerThread(oldHandle);
    }

    rb_reset(buf);
    *quitFlag = false;
 
    args->buf = buf;
    args->filename = strdup(inst_getCDFilePath(filename));

    args->quitFlag = quitFlag;

    assert(util_fileExists(args->filename));

    if (pthread_create(&ret, NULL, inst_fillerThreadFunc, (void*)args) != 0)
        free(args);

    return ret;
}


/* Shows disclaimer text */
static void inst_showDisclaimer() {
    ui_showTextBox("DISCLAIMER", "/install.txt");
}

/* Shows welcome screen, returns false if user wants to exit to shell */
static bool inst_showWelcomeScreen() {
    int ret = ui_showYesNoCustomLabels(ui_ButtonLabelNext, ui_ButtonLabelExitToShell, "Welcome to Windows 98 QuickInstall!");
    return (ret == 0);
}

/* Checks if given hard disk contains the installation source */
static bool inst_isInstallationSourceDisk(util_HardDisk *disk) {
    char *installSourceDevice = getenv("CDDEV");
    assert(installSourceDevice);
    return (installSourceDevice && util_stringStartsWith(installSourceDevice, disk->device));
}

/* Checks if given partition is the installation source */
static bool inst_isInstallationSourcePartition(util_Partition *part) {
    char *installSourceDevice = getenv("CDDEV");
    assert(installSourceDevice);
    return (installSourceDevice && util_stringEquals(installSourceDevice, part->device));
}

/* Tells the user he is trying to partition the install source disk */
static void inst_showInstallationSourceDiskError() {
    ui_showMessageBox("The selected disk contains the installation source. It can not be partitioned.");
}

/* Tells the user he is trying to partition the install source partition */
static void inst_showInstallationSourcePartitionError() {
    ui_showMessageBox("The selected partition contains the installation source, it cannot be the installation destination.");
}

/* Show partition wizard, returns true if user finished or false if he selected BACK. */
static bool inst_showPartitionWizard(util_HardDiskArray *hdds) {
    char cfdiskCmd[UTIL_MAX_CMD_LENGTH];
    char **menuLabels;
    int menuResult;

    while (1) {
        menuLabels = ui_allocateDialogMenuLabelList(hdds->count + 1); // +1 for the FINISHED label.

        for (int i = 0; i < hdds->count; i++) {
            ui_setMenuLabelListEntry(menuLabels, 
                                     i, 
                                     ui_makeDialogMenuLabel("%s", hdds->disks[i].device), 
                                     ui_makeDialogMenuLabel("Drive: [%s] - Size: %llu MB %s", 
                                                                hdds->disks[i].model, 
                                                                hdds->disks[i].size / 1024ULL / 1024ULL,
                                                                inst_isInstallationSourceDisk(&hdds->disks[i]) ? "(*)" : ""
                                                                ));
        }
        
        ui_setMenuLabelListEntry(menuLabels, 
                                hdds->count, 
                                ui_makeDialogMenuLabel("%s", ui_ButtonLabelFinished), 
                                ui_makeDialogMenuLabel("%s", ui_EmptyString));

        menuResult = ui_showMenu("Select the Hard Disk you wish to partition", menuLabels, true);
        ui_destroyDialogMenuLabelList(menuLabels);

        if (menuResult == UI_MENU_CANCELED) // BACK was pressed.
            return false;
        
        assert(menuResult != UI_MENU_ERROR);
        
        if (menuResult == hdds->count)
            return true;
        
        // Check if we're trying to partition the install source disk. If so, warn user and continue looping.
        if (inst_isInstallationSourceDisk(&hdds->disks[menuResult])) {
            inst_showInstallationSourceDiskError();
            continue;
        }

        // Invoke cfdisk command for chosen drive.
        snprintf(cfdiskCmd, UTIL_MAX_CMD_LENGTH, "%s%s", INST_CFDISK_CMD, hdds->disks[menuResult].device);      
        system(cfdiskCmd);
    }
}

/* Shows partition selector. Returns pointer to the selected partition. A return value of NULL means the user wants to go back. */
static util_Partition *inst_showPartitionSelector(util_HardDiskArray *hdds) {
    char **menuLabels = NULL;
    int menuResult;
    util_Partition *result = NULL;
    char chosenOption[256] = "/dev/";

    while (1) {
        menuLabels = ui_allocateDialogMenuLabelList(0);
        
        assert(menuLabels);

        for (size_t disk = 0; disk < hdds->count; disk++) {
            util_HardDisk *harddisk = &hdds->disks[disk];

            for (size_t part = 0; part < harddisk->partitionCount; part++) {
                util_Partition *partition = &harddisk->partitions[part];

                ui_addDialogMenuLabelToList(&menuLabels, 
                    ui_makeDialogMenuLabel("%s", util_shortDeviceString(partition->device)),
                    ui_makeDialogMenuLabel("(%s, %llu MB) on disk %s [%s] %s", 
                        util_utilFilesystemToString(partition->fileSystem),
                        partition->size / 1024ULL / 1024ULL,
                        util_shortDeviceString(harddisk->device),
                        harddisk->model,
                        inst_isInstallationSourcePartition(partition) ? "(*)" : ""
                        ));
            }
        }

        if (ui_getMenuLabelListItemCount(menuLabels) == 0) {
            ui_showMessageBox("No partitions were found! Partition your disk and try again!");
            ui_destroyDialogMenuLabelList(menuLabels);
            return NULL;
        }

        menuResult = ui_showMenu("Select the partition you wish to install to.", menuLabels, true);

        strncat(chosenOption, ui_getMenuResultString(), sizeof(chosenOption) - strlen(chosenOption));

        if (menuResult == UI_MENU_CANCELED) { // BACK was pressed.
            return NULL;
        }

        result = util_getPartitionFromDevicestring(hdds, chosenOption);
        assert(result);

        ui_destroyDialogMenuLabelList(menuLabels);

        // Is this the installation source? If yes, show menu again.
        if (inst_isInstallationSourcePartition(result)) {
            inst_showInstallationSourcePartitionError();
            continue;
        }

        return result;
    }
}

/* Asks user if he wants to format selected partition. Returns true if so. */
static bool inst_formatPartitionDialog(util_Partition *part) {
    char *prompt = ui_makeDialogMenuLabel("You have chosen the partition '%s'. Would you like to format it before the installation (recommended)?", part->device);
    int choice = ui_showYesNoCustomLabels(ui_ButtonLabelYes, ui_ButtonLabelNo, prompt);
    bool ret = true;
    char formatCmd[UTIL_MAX_CMD_LENGTH];
    free(prompt);
    if (choice == 0) {
        ret = util_getFormatCommand(part, part->fileSystem, formatCmd, UTIL_MAX_CMD_LENGTH);
        assert(ret);
        ret = (ui_runCommand("Formatting partition...", formatCmd) == 0);
        return ret;
    } else {
        return true;
    }
}


/* Asks user if he wants to overwrite the MBR and set the partition active. Returns true if so. */
static bool inst_askUserToOverwriteMBRAndSetActive(util_Partition *part) {
    char *prompt = ui_makeDialogMenuLabel("You have chosen the partition '%s'. Would you like to overwrite the Master Boot Record (MBR) and set the partition active (recommended)?", part->device);
    int ret = ui_showYesNoCustomLabels(ui_ButtonLabelYes, ui_ButtonLabelNo, prompt);
    free(prompt);
    return (ret == 0);
}

/* Show message box informing user that mount failed. */
static void inst_showFailedMount(util_Partition *part) {
    char *prompt = ui_makeDialogMenuLabel("The partition %s could not be accessed. "
        "There may be a disk problem. You can try formatting the partition. Returning to the partition selector.", 
        part->device);
    ui_showMessageBox(prompt);
    free(prompt);
}

/* Ask user if he wants to install extra driver package */
static bool inst_showDriverPrompt() {
    int ret = ui_showYesNoCustomLabels(ui_ButtonLabelYes, ui_ButtonLabelNo, "Would you like to install the integrated device drivers?");
    return (ret == 0);
}

/* Gets a MercyPak string (8 bit length + n chars) into dst. Must be a buffer of >= 256 bytes size. */
static inline bool inst_getMercyPakString(ringbuf *buf, char *dst) {
    bool success;
    uint8_t count;
    success = rb_getUInt8(buf, &count);
    success &= rb_read(buf, (uint8_t*) dst, (size_t) (count));
    dst[(size_t) count] = 0x00;
    return success;
}

static bool inst_copyFiles(const char *installPath, ringbuf *buf) {
    char fileHeader[5] = {0};
    char *destPath = malloc(strlen(installPath) + 256 + 1);   // Full path of destination dir/file, the +256 is because mercypak strings can only be 255 chars max
    char *destPathAppend = destPath + strlen(installPath) + 1;  // Pointer to first char after the base install path in the destination path + 1 for the extra "/" we're gonna append

    sprintf(destPath, "%s/", installPath);

    uint32_t dirCount;
    uint32_t fileCount;
    bool success = true;

    success &= rb_read(buf, (uint8_t*) fileHeader, 4);
    success &= rb_getUInt32(buf, &dirCount);
    success &= rb_getUInt32(buf, &fileCount);

    /* printf("File header: %s, dirs %d files: %d\n", fileHeader, (int) dirCount, (int) fileCount); */

    assert(success);

    if (!util_stringEquals(fileHeader, "ZIEG")) {
        assert(false && "File header wrong");
        free(destPath);
        return false;
    }

    ui_progressBoxInit("Creating Directories...");

    for (uint32_t d = 0; d < dirCount; d++) {
        uint8_t dirFlags;

        ui_progressBoxUpdate(d, dirCount);

        success &= rb_getUInt8(buf, &dirFlags);
        success &= inst_getMercyPakString(buf, destPathAppend);
        util_stringReplaceChar(destPathAppend, '\\', '/'); // DOS paths innit
        success &= (mkdir(destPath, dirFlags) == 0 || (errno == EEXIST));    // An error value is ok if the directory already exists. It means we can write to it. IT'S FINE.
    }

    ui_progressBoxDeinit();

    /*
     *  Extract and copy files from mercypak files
     */


    success = true;

    ui_progressBoxInit("Copying Files...");

    for (uint32_t f = 0; f < fileCount; f++) {
        uint8_t fileFlags;
        uint16_t fileDate;
        uint16_t fileTime;
        uint32_t fileSize;

        ui_progressBoxUpdate(f, fileCount);

        /* Mercypak file metadata (see mercypak.txt) */

        success &= inst_getMercyPakString(buf, destPathAppend); // First, filename string
        util_stringReplaceChar(destPathAppend, '\\', '/');      // DOS paths innit 
        success &= rb_getUInt8(buf, &fileFlags);                // DOS Flags byte 
        success &= rb_getUInt16(buf, &fileDate);                // DOS Date
        success &= rb_getUInt16(buf, &fileTime);                // DOS Timestamp
        success &= rb_getUInt32(buf, &fileSize);                // File size

        FILE *file = fopen(destPath, "wb");
        success &= (file != NULL);
        
        uint32_t leftToWrite = fileSize;

        while (leftToWrite) {
            size_t bytesToWrite = MIN(INST_DISK_IO_SIZE, leftToWrite);
            
            if (!rb_read_fwrite(buf, bytesToWrite, file)) {
                printf("WRITE ERROR! Errno: %d\n", errno);
                assert(false);
                abort();
            } 

            leftToWrite -= bytesToWrite;
        }
        
        success &= util_setDosFileTime(fileno_unlocked(file), fileDate, fileTime);
        success &= util_setDosFileAttributes(fileno_unlocked(file), fileFlags);

        fclose(file);
    }

    ui_progressBoxDeinit();

    free(destPath);
    return success;
}

/* Inform user and setup boot sector and MBR. */
static bool inst_setupBootSectorAndMBR(util_Partition *part, bool setActiveAndDoMBR) {
    bool success = true;
    ui_showInfoBox("Setting up Master Boot Record and Boot sector...");
    success &= util_writeWin98BootSectorToPartition(part);
    if (setActiveAndDoMBR) {
        success &= util_writeWin98MBRToDrive(part->parent);
        char activateCmd[UTIL_MAX_CMD_LENGTH];
        snprintf(activateCmd, UTIL_MAX_CMD_LENGTH, "sfdisk --activate %s %d", part->parent->device, part->index);
        ui_runCommand("Activating partition...", activateCmd);
    }
    return success;
}

/* Show success screen. Ask user if he wants to reboot */
static bool inst_showSuccessAndAskForReboot() {
    // Returns TRUE (meaning reboot = true) if YES (0) happens. sorry for the confusion.
    int ret = ui_showYesNoCustomLabels(ui_ButtonLabelReboot, ui_ButtonLabelExitToShell, "Finished the Windows 98 Installation. Would you like to Reboot or exit to a shell?");
    return (ret == 0);
}

/* Show failure screen :( */
static void inst_showFailMessage() {
    ui_showMessageBox("There was a problem during installation! :( You can press OK to get to a shell and inspect the damage.");    
}

/* Asks user which version of the hardware detection scheme he wants */
static const char *inst_askUserForRegistryVariant() {
    static const char fastpnp[] = "FASTPNP.866";
    static const char slowpnp[] = "SLOWPNP.866";
    static const char *options[] = { fastpnp, slowpnp };
    char **menuLabels;

    // only do this if SLOWPNP exists, as windows 95 doesn't have a non-pnp init so we have to apply fastpnp anyway...
    assert(util_fileExists(inst_getCDFilePath(fastpnp)));
    if (!util_fileExists(inst_getCDFilePath(slowpnp)))
        return fastpnp;
    
    menuLabels = ui_allocateDialogMenuLabelList(0);

    ui_addDialogMenuLabelToList(&menuLabels, 
                                ui_makeDialogMenuLabel("fast"),
                                ui_makeDialogMenuLabel("Fast hardware detection, skipping most non-PNP devices."));

    printf("%s %s\n", menuLabels[0], menuLabels[1]);

    ui_addDialogMenuLabelToList(&menuLabels, 
                                ui_makeDialogMenuLabel("slow"),
                                ui_makeDialogMenuLabel("Full hardware detection including non-PNP devices."));

    int menuResult = ui_showMenu("Select hardware detection method...", menuLabels, false);
    ui_destroyDialogMenuLabelList(menuLabels);
    
    assert(menuResult >= 0 && menuResult < 2);
    return options[menuResult];
}

/* Main installer process. Assumes the CDROM environment variable is set to a path with valid install.txt, FULL.866 and DRIVER.866 files. */
bool inst_main() {
    // Important mem stuff!
    uint64_t freeMemory = util_getProcSafeFreeMemory() / 2ULL;
    uint64_t ringBufferSize = MIN(0x20000000ULL, freeMemory*3ULL/4ULL); // Maximum 512MB
    ringbuf *buf = rb_init((size_t) ringBufferSize);
    util_HardDiskArray hda = { 0, NULL };
    const char *registryUnpackFile;
    util_Partition *destinationPartition = NULL;
    bool installDrivers;
    bool setActiveAndDoMBR;

    inst_InstallStep currentStep = INSTALL_WELCOME;
    bool quit = false;
    bool doReboot = false;
    bool installSuccess = true;
    bool goToNext = false;
    
    printf("\nfreeMemory: %llu\n ringBufferSize: %llu\n", freeMemory, ringBufferSize);

    ui_init();

    assert(getenv("CDROM"));

    // At the start, we initiate the filler thread so the actual OS data to install starts buffering in the background
    pthread_t fillerThreadHandle = inst_startFillerThread(NULL, buf, INST_SYSROOT_FILE, &quit);

    while (!quit) {
        switch (currentStep) {
            case INSTALL_WELCOME:
                inst_showDisclaimer();
                goToNext = inst_showWelcomeScreen();
                quit |= !goToNext; // First step so we will quit if user pressed exit to shell ...
                break;
            case INSTALL_PARTITION_WIZARD: {
                if (hda.disks == NULL)
                    hda = util_getSystemHardDisks();

                goToNext = inst_showPartitionWizard(&hda);
                util_hardDiskArrayDeinit(hda);
                hda = util_getSystemHardDisks();
                break;
            }
            case INSTALL_SELECT_DESTINATION_PARTITION: {
                destinationPartition = inst_showPartitionSelector(&hda);
                goToNext = (destinationPartition != NULL);
                break;
            }
            case INSTALL_FORMAT_PARTITION_CHECK: {
                // ask if partition should be formatted & do so if yes
                bool formatOK = inst_formatPartitionDialog(destinationPartition);
                
                // if formatting failed, this is SUPER bad (like, really very bad) so we assert
                assert(formatOK && "\nError during formatting!!");

                bool mountOK = util_mountPartition(destinationPartition);
                // If mounting failed, we will display a message and go back after.

                if (!mountOK)
                    inst_showFailedMount(destinationPartition);

                goToNext = mountOK;

                break;
            }

            case INSTALL_MBR_ACTIVE_BOOT_PROMPT: {
                setActiveAndDoMBR = inst_askUserToOverwriteMBRAndSetActive(destinationPartition);
                goToNext = true;
                break;
            }

            case INSTALL_REGISTRY_VARIANT_PROMPT: {
                registryUnpackFile = inst_askUserForRegistryVariant();
                goToNext = true;
                break;
            }
  
            case INSTALL_INTEGRATED_DRIVERS_PROMPT: {
                installDrivers = inst_showDriverPrompt();
                goToNext = true;
                break;
            }

            case INSTALL_DO_INSTALL: {
                installSuccess = inst_copyFiles(destinationPartition->mountPath, buf);
                
                if (installSuccess && installDrivers) {
                    // If the main data copy was successful, we stop the filler and restart it with the driver file.
                    fillerThreadHandle = inst_startFillerThread(fillerThreadHandle, buf, INST_DRIVER_FILE, &quit);
                    installSuccess = inst_copyFiles(destinationPartition->mountPath, buf);
                }

                if (installSuccess) {
                    // Install registry for selceted hardware detection variant
                    fillerThreadHandle = inst_startFillerThread(fillerThreadHandle, buf, registryUnpackFile, &quit);
                    installSuccess = inst_copyFiles(destinationPartition->mountPath, buf);
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

    // Flush filesystem writes clear screen and stop the buffering thread at the end...

    sync();
    system("clear");

    inst_stopFillerThread(fillerThreadHandle);
    
    rb_destroy(buf);

    if (doReboot) {
        reboot(RB_AUTOBOOT);
    }

    ui_deinit();

    return installSuccess;
}