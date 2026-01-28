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

/* Tells the user he is trying to install to a computer without hard disks. */
static inline void msg_noHardDisksFoundError(void) {
    ad_okBox("Attention", false, "No hard disks found!\nPlease install a hard disk and try again!");
}

/* Tells the user about an oopsie trying to open a file for reading. */
static inline void msg_fileOpenError(size_t osRootIndex, const char *filename) {
    ad_okBox("Attention", false, 
        "ERROR: A problem occured opening file for unpacking.\n"
        "OSRoot: '%u', file '%s'\n"
        "Error:  '%s' (%d)", 
        (unsigned) osRootIndex, filename, 
        strerror(errno), errno);
}

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
        "as a boot option in your UEFI confugration menu!\n"
        "\n"
        "ATTTENTION: Ensure that the following conditions are met:\n"
        "- DISABLE Secure Boot\n"
        "- DISABLE Above 4G Decoding/Resizable BAR/Smart Access Memory\n"
        "- DISABLE your computer's CSM if available/enabled.\n");
}

/* Shows invalid partition table message and asks user if he wants to wipe it */
static inline bool msg_askNonMbrWarningWipeMBR(util_HardDisk *hdd) {
    bool hasTableType = (strlen(hdd->tableType) > 0);
    int action = ad_yesNoBox("Hard Disk Partition Table Issue", true,
        "Selected disk: '%s'.\n"
        "Current partition table type (may be inacccurate): %s\n\n"
        "This disk does not contain a MBR / DOS style partition table.\n"
        "As a result, this disk will not work as an installation target.\n"
        "The existing partition table can be wiped, so that CFDISK can\n"
        "create a MBR / DOS style partition table in its place.\n"
        "Would you like to do this? (WARNING: THIS WILL CAUSE DATA LOSS!)",
        hdd->device,
        hasTableType ? hdd->tableType : "<?>");

    return (action == AD_YESNO_YES);
}

/*  Asks user if he wants to overwrite the MBR and set the partition active.
    Returns AD_YESNO_YES if so. AD_CANCELED means go back. */
static inline int msg_askOverwriteMBRAndSetActive(util_Partition *part) {
    return ad_yesNoBox("Confirm", true,
        "You have chosen the partition '%s'.\n"
        "Would you like to overwrite the Master Boot Record (MBR)\n"
        "and set the partition active (recommended)?", 
        part->device);
}

/*  Asks user if he wants to format selected partition.
    Returns AD_YESNO_YES if so. AD_CANCELED means go back. */
static inline int msg_askFormatPartition(util_Partition *part) {
    return ad_yesNoBox("Confirm", true,
        "You have chosen the partition '%s'.\n"
        "Would you like to format it before the installation (recommended)?\n",
        part->device);
}

/* Ask user if he wants to install driver package */
static inline int msg_askInstallDrivers(void) {
    return ad_yesNoBox("Selection", true, 
        "Would you like to install the base device drivers package?\n");
}

/* Ask user if he wants to skip legacy non-pnp detection. */
static inline int msg_askSkipPnpDetection(void) {
    return ad_yesNoBox("Selection", true,
        "Would you like to skip the legacy non-PnP detection phase?\n"
        "NOTE: Not recommended on ISA/VLB and early PCI machines.");
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
        "An error occured while unpacking '%s'.\n"
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

/* Show failure screen :( */
static inline void msg_failure(void) {
    ad_okBox("Error!", false,
        "There was a problem during installation! :(\n"
        "You can press ENTER to get to a shell and inspect the damage.");
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

static inline qi_PostInstallOption msg_askPostInstallAction(void) {
    const char *postInstallOptions[] = {
        "Reboot", "Return to Main Menu", "Exit to Linux Shell"
    };

    int selection = ad_menuExecuteDirectly("Windows 9x QuickInstall: Success",  false,
        (size_t) QI_POSTINSTALL_COUNT, postInstallOptions,
        "The installation was successful!\n"
        "What would you like to do now?");

    QI_ASSERT(selection < (int) QI_POSTINSTALL_COUNT);

    return (qi_PostInstallOption) selection;
}

