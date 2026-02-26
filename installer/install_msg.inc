/* Shows disclaimer text */
static inline void msg_disclaimer(void) {
    ad_textFileBox("Welcome to Windows 9x/ME QuickInstall!", inst_getSourceFilePath(0, "install.txt"));
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
static inline void msg_unsupportedFileSystemError(util_FileSystem fs) {
    ad_okBox("Attention", false,
        "The file system of the selected partition is '%s'.\n"
        "\n"
        "Windows 9x cannot be installed on a file system of such type.\n"
        "Repartition your disk or select another partition with the\n"
        "FAT16 or FAT32 file format.",
        util_utilFilesystemToString(fs));
}

static inline void msg_extendedPartitionError(void) {
    ad_okBox("Attention", false, "The selected partition is an extended partition.\nIt cannot be the installation destination.");
}

static inline void msg_logicalPartitionError(void) {
    ad_okBox("Attention", false, "The selected partition is a logical partition.\nIt cannot be the installation destination.");
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
static inline int32_t msg_uefiInfoBox(void) {
    return ad_okBox("UEFI Information", true,
        "WARNING: UEFI support is a highly experimental feature!\n"
        "\n"
        "It is based on CSMWrap (github.com/FlyGoat/csmwrap) and allows Windows\n"
        "9x/ME to boot on pure UEFI systems without the CSM (Compatibility\n"
        "Support Module).\n"
        "\n"
        "Due to the non-standard nature of this method, you may not be able to\n"
        "find the target disk as a boot option in your UEFI confugration menu.\n"
        "In this case, try manually adding a reference to the following file:\n"
        "\n"
        "<TARGET PARTITION>/EFI/BOOT/BOOTIA32.EFI\n"
        "\n"
        "ATTTENTION: Ensure that the following conditions are met:\n"
        "- DISABLE Secure Boot\n"
        "- DISABLE Above 4G Decoding/Resizable BAR/Smart Access Memory\n"
        "- DISABLE your computer's CSM if available/enabled.\n"
        "\n"
        "Press 'OK' to continue or ESCAPE to return to the configuration menu.");
}

/* Shows warning about CREGFIX */
static inline int32_t msg_cregfixInfoBox(void) {
    return ad_okBox("CREGFIX Information", true,
        "You have selected the CREGFIX patch to be applied.\n"
        "\n"
        "This is a patch intended for very modern systems to fix a bug present\n"
        "in most UEFI CSM implementations.\n"
        "\n"
        "The patch for this is universal and should not cause any harm on\n"
        "unaffected systems, but it is still recommended to only install it\n"
        "if needed.\n"
        "\n"
        "Press 'OK' to continue or ESCAPE to return to the configuration menu.");
}

/* Shows warning about LBA64 */
static inline int32_t msg_lba64InfoBox(void) {
    return ad_okBox("LBA64 / GPT Information", true,
        "The addition of 64-Bit LBA and GPT TSD support drivers is an EXPERIMENTAL\n"
        "and potentially DANGEROUS feature.\n"
        "\n"
        "It provides read AND write access to disks bigger than 2 TB and/or using\n"
        "GUID partition table (GPT), including those with a 4KB native sector size.\n"
        "\n"
        "Due to the boundary-breaking nature of these, make sure that your disks,\n"
        "controllers and drivers support writing past these barriers.\n"
        "\n"
        "YOU ARE RISKING DISK CORRUPTION OHTERWISE.\n"
        "\n"
        "Note that any patched versions of DISKVSD.VXD will be overwritten, as they\n"
        "are incompatible.\n"
        "\n"
        "Press 'OK' to continue or ESCAPE to return to the configuration menu.");
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

static inline void msg_exitToShellInfo(void) {
    ad_okBox("Exit To Shell", false,
        "You are about to exit to the Linux shell.\n"
        "\n"
        "The shell can be used for diagnostics and file copying.\n"
        "You can 'mount' file systems, use 'lspci', 'lscpu', 'dd'\n"
        "and a few other essential utilities.\n"
        "\n"
        "Did you know that QuickInstall ships with THE tool to\n"
        "diagnose faulty 3dfx Voodoo II cards?\n"
        "\n"
        "Type 'witchery' to use it! (Thanks to Fagear!)\n"
        );
}

static const char msg_mainMenuText[] =
    "Welcome to the QuickInstall main menu!\n"
    "\n"
    "Please make a selection.\n"
    "Note: It is advised to partition a hard disk for use with\n"
    "Windows 98 using the '[CFDISK]' option before installation.\n"
    "\n"
    "Where do you want to go today(tm)?\n";
