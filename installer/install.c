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
#include <fcntl.h>
#include <sched.h>
#include <unistd.h>
#include <errno.h>
#include <utime.h>

#include "ringbuf.h"
#include "util.h"


typedef enum {
    INSTALL_WELCOME = 0,
    INSTALL_PARTITION_WIZARD,
    INSTALL_SELECT_DESTINATION_PARTITION,
    INSTALL_FORMAT_PARTITION_CHECK,
    INSTALL_INTEGRATED_DRIVERS_PROMPT,
    INSTALL_DO_INSTALL,
} inst_InstallStep;

#define INST_VERSION "0.1"
#define INST_COPYRIGHT "(C) 2023 Eric Voirin"
#define INST_BACKTITLE "--backtitle \"Win98SE QuickInstall -- Version " INST_VERSION " " INST_COPYRIGHT "\""

// The text for the "FINISHED" option in dialogs.
#define INST_DIALOG_FINISHED "< FINISHED >"
// The file the dialog option will be printed to...
#define INST_DIALOG_OPTION_TMP_FILE "/tmp/0"

#define INST_DIALOG_CMD "dialog "
#define INST_CFDISK_CMD "cfdisk "

/* Arguments for background OS data buffer thread */
typedef struct {
    char *filename;             // Filename to buffer
    ringbuf *buf;               // Ringbuffer to buffer into
    size_t scratchBufferSize;   // Scratch buffer / maximum chunk size
    bool *quitFlag;             // Pointer to quit flag
} inst_FillerThreadArgs;

/* Thread function for background OS data buffer thread */
static void* inst_fillerThreadFunc(void* threadParam) {
    inst_FillerThreadArgs *args = (inst_FillerThreadArgs*) threadParam;
    printf("FILLER THREAD STARTED filename %s ringbuf %p\n", args->filename, args->buf);

    ringbuf *buf = args->buf;
    int file = open(args->filename, O_RDONLY);
    assert(file);
    size_t fillerThreshold = buf->size / 4 * 3;
    size_t scratchBufferSize = args->scratchBufferSize;
    uint8_t *scratchBuffer = malloc(scratchBufferSize);

    bool endOfFile = false;
    bool *quit = args->quitFlag;

    assert(scratchBuffer);

    while (!endOfFile && !(*quit)) {        
        if (rb_avail(buf) < fillerThreshold) {
            // We dipped below the filler threshold. Which means we must now fill all of it
            size_t leftToFill = rb_space(buf);
            while (!endOfFile && leftToFill && !(*quit)) {
                size_t bytesToRead = MIN(scratchBufferSize, leftToFill);
                ssize_t bytesRead = read(file, scratchBuffer, bytesToRead);
                if (bytesRead < 0) {
                    // ERROR
                    printf("Read error!\n");
                    // TODO: Handle read error
                    assert(false);
                } else if (bytesRead < bytesToRead) {
                    // EOF
                    endOfFile = true;
                }
                leftToFill -= bytesRead;
                assert(rb_write(buf, scratchBuffer, bytesRead));
            }
        } 
    }

    close(file);
    printf("THREAD DONE\n");
    pthread_exit(threadParam);
}

/* Stop the background thread buffering Win98 install files. 
   Filename is Mercypak file to buffer
   scratchBufferSize is essentially maximum chunk sizes.
   quitFlag is a pointer to a bool that indicates when the buffering should stop.*/
static pthread_t inst_startFillerThread(ringbuf *buf, const char *filename, size_t scratchBufferSize, bool *quitFlag) {
    pthread_t ret;
    
    inst_FillerThreadArgs *args = calloc(sizeof(inst_FillerThreadArgs), 1);
    assert(args);
    args->buf = buf;
    args->filename = strdup(filename);
    args->scratchBufferSize = scratchBufferSize;
    args->quitFlag = quitFlag;

    if (pthread_create(&ret, NULL, inst_fillerThreadFunc, (void*)args) != 0)
        free(args);
    return ret;
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

/* Shows disclaimer text */
static void inst_showDisclaimer() {
    char dialogCmd[UTIL_MAX_CMD_LENGTH];
    snprintf(dialogCmd, UTIL_MAX_CMD_LENGTH, "dialog --exit-label \"Next\" " INST_BACKTITLE " --textbox %s/install.txt 20 75", getenv("CDROM"));
    system(dialogCmd);
}

/* Shows welcome screen, returns false if user wants to exit to shell */
static bool inst_showWelcomeScreen() {
    int ret = system("dialog --yes-label \"Next\" --no-label \"Exit To Shell\" " INST_BACKTITLE " --yesno \"Welcome to Windows 98 QuickInstall!\" 0 0");
    return (ret == 0);
}

/* Show partition wizard, returns true if user finished or false if he selected BACK. */
static bool inst_showPartitionWizard(util_HardDiskArray *hdds) {
    char dialogCmd[UTIL_MAX_CMD_LENGTH];
    char *dialogCmdPos;

    while (1) {
        dialogCmdPos = dialogCmd;
        dialogCmdPos += snprintf(dialogCmdPos, UTIL_MAX_CMD_LENGTH - (dialogCmdPos - dialogCmd), "%s %s --ok-label \"Next\" --cancel-label \"Back\" --menu 'Select the drive to partition:' 0 0 0 ", INST_DIALOG_CMD, INST_BACKTITLE);
        for (int i = 0; i < hdds->count; i++) // Fill the dialog command with all the system disks
            dialogCmdPos += snprintf(dialogCmdPos, UTIL_MAX_CMD_LENGTH - (dialogCmdPos - dialogCmd), "\"%s\" \"Drive: [%s] - Size: %llu MB\" ", hdds->disks[i].device, hdds->disks[i].model, hdds->disks[i].size / 1024ULL / 1024ULL);
        dialogCmdPos += snprintf(dialogCmdPos, UTIL_MAX_CMD_LENGTH - (dialogCmdPos - dialogCmd), "\"%s\" \"\" 2>%s", INST_DIALOG_FINISHED, INST_DIALOG_OPTION_TMP_FILE);

        if (system(dialogCmd) != 0) // BACK was pressed.
            return false;

        char chosenOption[256];
        util_readFirstLineFromFileIntoBuffer(INST_DIALOG_OPTION_TMP_FILE, chosenOption);
        
        // Invoke cfdisk command for chosen drive. the lookup will fail (reutnr SIZE_MAX) if BACK was chosen!
        size_t selectedDisk = util_getHardDiskArrayIndexFromDevicestring(hdds, chosenOption);
        if (selectedDisk == SIZE_MAX) return true;   // "FINISHED" was chosen.

        snprintf(dialogCmd, UTIL_MAX_CMD_LENGTH, "%s%s", INST_CFDISK_CMD, hdds->disks[selectedDisk].device);      
        system(dialogCmd);
    }
}

/* Shows partition selector. Returns pointer to the selected partition. A return value of NULL means the user wants to go back. */
static util_Partition *inst_showPartitionSelector(util_HardDiskArray *hdds) {
    char dialogCmd[UTIL_MAX_CMD_LENGTH];
    char *dialogCmdPos;
    util_Partition *result = NULL;

    dialogCmdPos = dialogCmd;
    dialogCmdPos += snprintf(dialogCmdPos, UTIL_MAX_CMD_LENGTH - (dialogCmdPos - dialogCmd), "%s %s --ok-label \"Next\" --cancel-label \"Back\" --menu 'Select the partition to install to:' 0 0 0 ", INST_DIALOG_CMD, INST_BACKTITLE);
    for (size_t disk = 0; disk < hdds->count; disk++) { // Fill the dialog command with all the system disks
        util_HardDisk *harddisk = &hdds->disks[disk];

        for (size_t part = 0; part < harddisk->partitionCount; part++) {
            util_Partition *partition = &harddisk->partitions[part];
            dialogCmdPos += snprintf(dialogCmdPos, UTIL_MAX_CMD_LENGTH - (dialogCmdPos - dialogCmd), "\"%s\" \"(%s, %llu MB) on disk %s [%s]\" ", 
                util_shortDeviceString(partition->device),
                util_utilFilesystemToString(partition->fileSystem),
                partition->size / 1024ULL / 1024ULL,
                util_shortDeviceString(harddisk->device),
                harddisk->model);
        }
    }
    dialogCmdPos += snprintf(dialogCmdPos, UTIL_MAX_CMD_LENGTH - (dialogCmdPos - dialogCmd), "2>%s", INST_DIALOG_OPTION_TMP_FILE);

    if (system(dialogCmd) != 0) // User pessed BACK
        return NULL;

    char chosenOption[256] = "/dev/";
    util_readFirstLineFromFileIntoBuffer(INST_DIALOG_OPTION_TMP_FILE, util_endOfString(chosenOption));
    result = util_getPartitionFromDevicestring(hdds, chosenOption);
    assert(result);
    return result;
}

/* Asks user if he wants to format selected partition. Returns true if so. */
static bool inst_askUserToFormatPartition(util_Partition *part) {
    char dialogCmd[UTIL_MAX_CMD_LENGTH];
    snprintf(dialogCmd, UTIL_MAX_CMD_LENGTH, "dialog " INST_BACKTITLE " --yesno "
        "\"You have chosen the partition '%s'. Would you like to format it before the installation (recommended)?\" 0 0",
        part->device);
    int ret = system(dialogCmd);
    return (ret == 0);
}

/* Show message box informing user that mount failed. */
static void inst_showFailedMount(util_Partition *part) {
    char dialogCmd[UTIL_MAX_CMD_LENGTH];
    snprintf(dialogCmd, UTIL_MAX_CMD_LENGTH, "dialog " INST_BACKTITLE " --msgbox \"The partition %s could not be accessed. "
        "There may be a disk problem. You can try formatting the partition. Returning to the partition selector.\" 0 0", 
        part->device);
    system(dialogCmd);
}

/* Ask user if he wants to install extra driver package */
static bool inst_showDriverPrompt() {
    int ret = system("dialog " INST_BACKTITLE " --yesno \"Would you like to install the integrated device drivers?\" 0 0");
    return (ret == 0);
}


/* Shows "Creating directories" text... TODO: Make actual progress bar using libdialog */
static void inst_showDirProgress() {
    system("dialog " INST_BACKTITLE " --infobox \"Creating directories...\" 0 0");
}

/* Shows "Copying files" text... TODO: Make actual progress bar using libdialog */
static void inst_showFileProgress() {
    system("dialog " INST_BACKTITLE " --infobox \"Copying files...\" 0 0");
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

static bool inst_copyFiles(const char *installPath, ringbuf *buf, size_t scratchBufferSize) {
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

    printf("File header: %s, dirs %d files: %d\n", fileHeader, (int) dirCount, (int) fileCount);

    assert(success);

    if (!util_stringEquals(fileHeader, "ZIEG")) {
        assert(false && "File header wrong");
        free(destPath);
        return false;
    }

    inst_showDirProgress();

    for (uint32_t d = 0; d < dirCount; d++) {
        uint8_t dirFlags;
        success &= rb_getUInt8(buf, &dirFlags);
        success &= inst_getMercyPakString(buf, destPathAppend);
        util_stringReplaceChar(destPathAppend, '\\', '/'); // DOS paths innit
        success &= (mkdir(destPath, dirFlags) == 0 || (errno == EEXIST));    // An error value is ok if the directory already exists. It means we can write to it. IT'S FINE.
    }

    /*
     *  Extract and copy files from mercypak files
     */

    inst_showFileProgress();

    uint8_t *scratchBuffer = malloc(scratchBufferSize);
    assert(scratchBuffer);

    success = true;

    for (uint32_t f = 0; f < fileCount; f++) {
        uint8_t fileFlags;
        uint16_t fileDate;
        uint16_t fileTime;
        uint32_t fileSize;

        /* Mercypak file metadata (see mercypak.txt) */

        success &= inst_getMercyPakString(buf, destPathAppend); // First, filename string
        util_stringReplaceChar(destPathAppend, '\\', '/');      // DOS paths innit 
        success &= rb_getUInt8(buf, &fileFlags);                // DOS Flags byte 
        success &= rb_getUInt16(buf, &fileDate);                // DOS Date
        success &= rb_getUInt16(buf, &fileTime);                // DOS Timestamp
        success &= rb_getUInt32(buf, &fileSize);                // File size

        int file = open(destPath, O_WRONLY | O_CREAT | O_TRUNC);
        success &= (file >= 0);
        
        uint32_t leftToWrite = fileSize;

        while (leftToWrite) {
            // scratchBufferSize is max chunk size, make sure not to write too much though
            size_t bytesToWrite = MIN(scratchBufferSize, leftToWrite);
            success &= rb_read(buf, scratchBuffer, bytesToWrite);
            ssize_t bytesWritten = write(file, scratchBuffer, bytesToWrite);
           
            if (bytesWritten < bytesToWrite) {
                printf("WRITE ERROR! Errno: %d\n", errno);
                assert(false);
                abort();
            } 

            leftToWrite -= bytesWritten;
        }
        
        success &= util_setDosFileTime(file, fileDate, fileTime);
        success &= util_setDosFileAttributes(file, fileFlags);

        close(file);
    }

    free(scratchBuffer);
    free(destPath);
    return success;
}

/* Inform user and setup boot sector and MBR. TODO: PROBABLY ASK USER IF HE WANTS TO OVERWRITE MBR? */
static bool inst_setupBootSectorAndMBR(util_Partition *part) {
    bool success = true;
    system("dialog " INST_BACKTITLE " --infobox \"Setting up Master Boot Record and Boot sector...\" 0 0");
    success &= util_writeWin98BootSectorToPartition(part);
    success &= util_writeWin98MBRToDrive(part->parent);
    return success;
}

/* Show success screen. Ask user if he wants to reboot */
static bool inst_showSuccessAndAskForReboot() {
    // Returns TRUE (meaning reboot = true) if YES (0) happens. sorry for the confusion.
    return system("dialog --yes-label \"Reboot\" --no-label \"Exit to Shell\" " INST_BACKTITLE " --yesno \"Finished the Windows 98 Installation. Would you like to Reboot or exit to a shell?\" 0 0") == 0;
}

/* Show failure screen :( */
static void inst_showFailMessage() {
    system("dialog " INST_BACKTITLE " --msgbox \"There was a problem during installation! :( You can press OK to get to a shell and inspect the damage.\" 0 0");    
}

/* Main installer process. Assumes the CDROM environment variable is set to a path with valid install.txt, FULL.866 and DRIVER.866 files. */
bool inst_main() {
    // Important mem stuff!
    uint64_t freeMemory = util_getProcSafeFreeMemory();
    uint64_t ringBufferSize = MIN(0x20000000ULL, freeMemory/4ULL*3ULL); // Maximum 512MB
    uint64_t scratchBufferSize = MIN(0x080000ULL, (freeMemory - ringBufferSize) / 4ULL); // Maximum 4MB. Divided by 4 so we can allocate 2.
    ringbuf *buf = rb_init((size_t) ringBufferSize);
    util_HardDiskArray hda = { 0, NULL };

    util_Partition *destinationPartition = NULL;
    bool installDrivers;

    inst_InstallStep currentStep = INSTALL_WELCOME;
    bool quit = false;
    bool reboot = false;
    bool installSuccess = true;
    bool goToNext;

    assert(getenv("CDROM"));

    // At the start, we initiate the filler thread so the actual OS data to install starts buffering in the background
    char installDataFilename[256];
    snprintf(installDataFilename, sizeof(installDataFilename), "%s/%s", getenv("CDROM"), "FULL.866");
    pthread_t fillerThreadHandle = inst_startFillerThread(buf, installDataFilename, scratchBufferSize, &quit);

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
                // ask if partition should be formatted
                if (inst_askUserToFormatPartition(destinationPartition)) {
                    // If yes, we will try to format the partition and then mount it
                    util_formatPartition(destinationPartition, destinationPartition->fileSystem);
                } 

                bool mountOK = util_mountPartition(destinationPartition);
                // If mounting failed, we will display a message and go back after.

                if (!mountOK)
                    inst_showFailedMount(destinationPartition);
                goToNext = mountOK;

                break;
            }
  
            case INSTALL_INTEGRATED_DRIVERS_PROMPT: {
                installDrivers = inst_showDriverPrompt();
                goToNext = true;
                break;
            }

            case INSTALL_DO_INSTALL: {
                installSuccess = inst_copyFiles(destinationPartition->mountPath, buf, scratchBufferSize);

                quit = true;
                
                if (installSuccess && installDrivers) {
                    // If the main data copy was successful, we stop the filler and restart it with the driver file.
                    inst_stopFillerThread(fillerThreadHandle);
                    rb_reset(buf);
                    quit = false;
                    snprintf(installDataFilename, sizeof(installDataFilename), "%s/%s", getenv("CDROM"), "DRIVER.866");
                    fillerThreadHandle = inst_startFillerThread(buf, installDataFilename, scratchBufferSize, &quit);
                    installSuccess = inst_copyFiles(destinationPartition->mountPath, buf, scratchBufferSize);
                }

                util_unmountPartition(destinationPartition);
                installSuccess &= inst_setupBootSectorAndMBR(destinationPartition);

                if (installSuccess) {                    
                    reboot = inst_showSuccessAndAskForReboot();
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

    if (reboot) {
        system("reboot");
    }

    return installSuccess;
}