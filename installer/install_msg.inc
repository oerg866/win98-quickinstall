/* Shows disclaimer text */
static inline void msg_disclaimer(void) {
    ad_textFileBox("DISCLAIMER", inst_getSourceFilePath(0, "install.txt"));
}

/* Tells the user he is trying to partition the install source disk */
static inline void msg_installationSourceDiskError(void) {
    ad_okBox("Attention", false, "The selected disk contains the installation source.\nIt can not be partitioned.");
}

/* Tells the user he is trying to install to the install source partition */
static inline void msg_sourcePartitionError(void) {
    ad_okBox("Attention", false, "The selected partition contains the installation source.\nIt cannot be the installation destination.");
}

/* Tells the user he is trying to install to a non-FAT partition */
static inline void msg_unsupportedFileSystemError(void) {
    ad_okBox("Attention", false, "The selected partition has an unsupported file system.\nIt cannot be the installation destination.");
}

/* Tells the user he is trying to install to a non-MBR hard drive*/
static inline void msg_destinationInvalidPartitionTable(void) {
    ad_okBox("Attention", false, 
        "The selected partition is on a disk with a non-MBR partition table\n"
        "(i.e. GPT or similar). It cannot be used as the destination.");
}

/* Tells the user he is trying to install to a computer without hard disks. */
static inline void msg_noHardDisksFoundError(void) {
    ad_okBox("Attention", false, "No hard disks found!\nPlease install a hard disk and try again!");
}

/* Tells the user about an oopsie trying to open a file for reading. */
static inline void msg_fileOpenError(size_t osRootIndex, const char *filename) {
    ad_okBox("Attention", false, 
        "ERROR: A problem occurred opening file for unpacking.\n"
        "OSRoot: '%u', file '%s'\n"
        "Error:  '%s' (%d)", 
        (unsigned) osRootIndex, filename, 
        strerror(errno), errno);
}

/* Shows warning about UEFI */
static inline void msg_uefiInfoBox(void) {
    ad_okBox("UEFI Information", false,
        "You have selected UEFI support to be enabled.\n"
        "This is a highly experimental feature.\n"
        "\n"
        "It is based on CSMWrap (github.com/FlyGoat/csmwrap)\n"
        "and allows Windows 9x/ME to boot on pure UEFI systems\n"
        "without the CSM (Compatibility Support Module).\n"
        "\n"
        "Due to the non-standard nature of this UEFI boot\n"
        "method, you may not be able to find the target disk\n"
        "as a boot option in your UEFI confugration menu.\n"
        "In this case, try manually adding a reference to this file:\n"
        "\n"
        "<TARGET PARTITION>/EFI/BOOT/BOOTIA32.EFI\n"
        "\n"
        "ATTTENTION: Ensure that the following conditions are met:\n"
        "- DISABLE Secure Boot\n"
        "- DISABLE Above 4G Decoding/Resizable BAR/Smart Access Memory\n"
        "- DISABLE your computer's CSM if available/enabled.\n");
}

/* Shows warning about CREGFIX */
static inline void msg_cregfixInfoBox(void) {
    ad_okBox("CREGFIX Information", false,
        "You have selected the CREGFIX patch to be applied.\n"
        "This is a feature that needs a modern CPU to work,\n"
        "fixing a bug present in most UEFI CSM implementations.\n"
        "\n"
        "It uses instructions not supported by older CPUs.\n"
        "Applying this patch on an older system will render the\n"
        "installation un-bootable.\n");
}

/* Shows warning about LBA64 */
static inline void msg_lba64InfoBox(void) {
    ad_okBox("LBA64 / GPT Information", false,
        "You have selected the 64-Bit LBA and GPT TSD support drivers to\n"
        "be installed.\n"
        "\n"
        "This is an experimental and potentially DANGEROUS feature,\n"
        "providing read/write access to disks bigger than two terabytes\n"
        "and disks using the GUID partition layout, including 4k native sector\n"
        "size disks, under Windows 9x\n"
        "\n"
        "Due to the boundary-breaking nature of these drivers, you must\n"
        "ensure that your disks, controllers and drivers support writing past\n"
        "the usual two terabytes barrier.\n"
        "\n"
        "YOU ARE RISKING DISK CORRUPTION OHTERWISE.\n");
}

/* Shows info that partition is mounted and asks if user wants to unmount and proceed */
static inline bool msg_askUnmountBeforeInstall(util_Partition *part) {
    int action = ad_yesNoBox("Partition Warning", true,
        "Selected partition: '%s'.\n"
        "\n"
        "The partition you selected is already mounted in the system.\n"
        "It may be in use - do you wish to unmount it and proceed?\n"
        "WARNING - this may lead to data loss if unwanted!",
        part->device);
    return (action == AD_YESNO_YES);
}


/* Shows invalid partition table message and asks user if he wants to wipe it */
static inline bool msg_askNonMbrWarningWipeMBR(util_HardDisk *hdd, const char *typeString) {
    int action = ad_yesNoBox("Hard Disk Partition Table Issue", true,
        "Selected disk: '%s'.\n"
        "Current partition table type (may be inacccurate): %s\n\n"
        "This disk does not contain a MBR / DOS style partition table.\n"
        "As a result, this disk will not work as an installation target.\n"
        "The existing partition table can be wiped, so that CFDISK can\n"
        "create a MBR / DOS style partition table in its place.\n"
        "Would you like to do this? (WARNING: THIS WILL CAUSE DATA LOSS!)",
        hdd->device,
        typeString);

    return (action == AD_YESNO_YES);
}

/* Show message box informing user that formatting failed. */
static inline void msg_formatFailed(util_Partition *part) {
    ad_okBox("Error", false,
        "The partition %s could not be formatted.\n"
        "The last recorded error was:\n"
        "'%s' (%d).\n"
        "There may be a disk problem.\n"
        "Try re-partitioning the disk or using another partition.\n"
        "Returning to the partition selector.",
        part->device, strerror(errno), errno);
}

/* Show message box informing user that mount failed*/
static inline void msg_mountFailed(util_Partition *part) {
    ad_okBox("Error", false,
        "The partition %s could not be accessed.\n"
        "The last recorded error was:\n"
        "'%s' (%d).\n"
        "There may be a disk problem.\n"
        "You can try formatting the partition.\n"
        "Returning to the partition selector.",
        part->device, strerror(errno), errno);
}

/* Show message box informing user that copying failed*/
static inline void msg_unpackFailed(const char *sourceFile) {
    ad_okBox("Error", false,
        "An error occurred while unpacking '%s'.\n"
        "The last recorded error was:\n"
        "'%s' (%d).\n"
        "There may be a disk problem.\n"
        "You can try a different source / destination disk.\n"
        "Returning to the partition selector.", 
        sourceFile, strerror(errno), errno);
}

/* Show message box informing user that wiping partition table failed */
static inline void msg_wipeMbrFailed(void) {
    ad_okBox("Error", false,
    "An error occurred trying to wipe the partition table:\n"
    "'%s' (%d)", strerror(errno), errno);
}

/* Show message box informing user that getting disk info failed */
static inline void msg_refreshDiskError(void) {
    ad_okBox("Error", false,
    "An error occurred trying to evaluate all disks in the system:\n"
    "'%s' (%d)", strerror(errno), errno);
}

/* Show message box informing user that the program is not running as root. */
static inline void msg_notRunningAsRootError(void) {
    ad_okBox("Fatal error!", false,
        "The installer must be run as root!");
}

static inline void msg_environmentError(void) {
    ad_okBox("Fatal error!", false,
        "To run the installer, you must either set the CDROM and CDDEV\n"
        "environment variables or use the command line parameters:\n"
        "lunmercy <source_path> <source_dev>\n\n"
        "Example: lunmercy /mount/install /dev/sr0");
}

/* Reminder to format any new created install partitions */
static inline void msg_reminderToFormatNewPartition(void) {
    ad_okBox("Attention", false,
        "Remember to answer 'yes' to the format prompt\n"
        "if you are installing on a partition you've just created!");
}


static inline void msg_directoryWarning(void) {
    ad_okBox("Attention", false,
        "Warning: Failed to create one or more directories!\n"
        "The last recorded error was:\n"
        "'%s' (%d)\n"
        "This may not be fatal. Continue at your own risk!",
        strerror(errno), errno);
}

static inline void msg_installError(const char *optionLabel) {
    ad_okBox("Error!", false,
        "A problem occurred during installation!\n"
        "Failed step:\n"
        "  '%s'\n"
        "The last recorded error was:\n"
        "  '%s' (%d)\n",
        optionLabel, strerror(errno), errno);
}
