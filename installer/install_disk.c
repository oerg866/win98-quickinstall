#include "install.h"
#include "util.h"
#include "qi_assert.h"
#include "anbui/anbui.h"

#include <errno.h>

#include "install_msg.inc"

// Wrapper to set the footer text to show what's going on
static bool qi_wipePartitionTableUi(util_HardDisk *hdd) {
    ad_setFooterText("Wiping partition table...");
    bool result = util_wipePartitionTable(hdd);
    ad_clearFooter();
    return result;
}

//On invalid/unsupported partition table, ask user if he wants to wipe it
static void qi_invalidPartTableAskUserAndWipe(util_HardDisk *hdd) {
    int action = ad_yesNoBox("Hard Disk Partition Table Issue", true,
        "Selected disk: '%s'.\n"
        "Current partition table type (may be inacccurate): %s\n\n"
        "This disk does not contain a MBR / DOS style partition table.\n"
        "As a result, this disk will not work as an installation target.\n"
        "The existing partition table can be wiped, so that CFDISK can\n"
        "create a MBR / DOS style partition table in its place.\n"
        "Would you like to do this? (WARNING: THIS WILL CAUSE DATA LOSS!)",
        hdd->device,
        inst_getTableTypeString(hdd));


    if (action != AD_YESNO_YES) {
        // User doesn't want to wipe? get out
        return;
    }

    if (!qi_wipePartitionTableUi(hdd)) {
        // No return value here since we can't do much and we'll launch cfdisk anyway
        msg_wipeMbrFailed();
    }
}

// User selected disk to have wiped partition table. Ask him if he's REALLY sure.
// And then do it :)
static void qi_askUserAndWipePartitionTable(util_HardDisk *hdd) {
    int action = ad_yesNoBox("Wipe Hard Disk Partition Table", true,
        "Selected disk: '%s'.\n"
        "Current partition table type (may be inacccurate): %s\n\n"
        "You have selected this disk to have its partition table wiped.\n"
        "This may be useful in rare cases where buggy/leftover partition\n"
        "entries cause the Windows 9x bootloaded to malfunction.\n"
        "\n"
        "This disk must then be partitioned using the partitioning menu.\n"
        "\n"
        "WARNING: THIS WILL CAUSE DATA LOSS! ARE YOU ABSOLUTELY SURE?",
        hdd->device,
        inst_getTableTypeString(hdd));

    if (action != AD_YESNO_YES) {
        // User doesn't want to wipe? get out
        return;
    }

    if (!qi_wipePartitionTableUi(hdd)) {
        // No return value here since we can't do much.
        msg_wipeMbrFailed();
    }
}


// helper macro for the return code for partition table wiping
#define QI_DISKMGMT_WIPE_PARTITIONS (AD_F_KEY(9-1))

// Executes the disk management menu in a loop until a selection is made that is recognized as a valid action
// This can be:
// AD_F_KEY(8) - F9 was pressed, wipe partition table
// AD_CANCELED - Menu was cancelled
// Any value from 0 to X - the selected disk index.
// The exit label at the end of the menu is converted to AD_CANCELED for code simplicity.
static int32_t qi_diskMgmtMenuExecute(ad_Menu *menu) {
    while (1) {
            int menuResult = ad_menuExecute(menu);
            int itemCount = (int) ad_menuGetItemCount(menu);
            // Check for the operations we support

            if (menuResult == QI_DISKMGMT_WIPE_PARTITIONS)  return menuResult;
            if (menuResult == AD_CANCELED)                  return menuResult;
            if (menuResult == (itemCount - 1))              return AD_CANCELED;
            if (menuResult >= 0)                            return menuResult;

            QI_FATAL(menuResult != AD_ERROR, "Invalid menu state");
    }
}

qi_WizardAction qi_diskMgmtMenu(qi_InstallContext *ctx) {
    char menuPrompt[512];
    snprintf(menuPrompt, sizeof(menuPrompt),
            "An asterisk (*) identifies the installation source. It cannot be altered.\n"
            "A question mark <?> depicts an invalid/unknown partition table type.\n"
            "\n"
            "Additional options: F9 - Wipe partition table (DANGER)\n\n"
            "%s", inst_getDiskMenuHeader());

    char cfdiskCmd[UTIL_MAX_CMD_LENGTH];

    QI_FATAL(ctx != NULL, "InstallContext invalid");

    while (1) {
        // At the beginning of every loop of the wizard, we need to refresh our disk list
        // and update the pointer of the caller
        if (!qi_refreshDisks(ctx)) {
            msg_refreshDiskError();
            return WIZ_MAIN_MENU;
        } else if (ctx->hda->count == 0) {
            msg_noHardDisksFoundError();
            return WIZ_MAIN_MENU;
        }

        ad_Menu *menu = ad_menuCreate(
            "Partition Wizard - Select the Hard Disk you wish to partition.",
            menuPrompt,
            true, true);

        QI_ASSERT(menu);

        // This menu is created with F keys enabled, so we will get an F key return code in this case.

        for (size_t i = 0; i < ctx->hda->count; i++) {
            ad_menuAddItemFormatted(menu, inst_getDiskMenuString(&ctx->hda->disks[i]));
        }        
        ad_menuAddItemFormatted(menu, " [Back]");

        int menuResult = qi_diskMgmtMenuExecute(menu);
        size_t selectedItem = ad_menuGetSelectedItem(menu);
        ad_menuDestroy(menu);

        // Back / ESC pressed?
        if (menuResult == AD_CANCELED) {
            break;
        }

        util_HardDisk *selectedDisk = &ctx->hda->disks[selectedItem];

        // Wipe partition table?
        if (menuResult == QI_DISKMGMT_WIPE_PARTITIONS) {
            qi_askUserAndWipePartitionTable(selectedDisk);
            continue;
        }

        // If we are here, the user actually selected a disk with ENTER.
            
        // Check if we're trying to partition the install source disk. If so, warn user and continue looping.
        if (inst_isInstallationSourceDisk(selectedDisk)) {
            msg_installationSourceDiskError();
            continue;
        }

        if (!util_stringEquals(selectedDisk->tableType, "dos")) {
            qi_invalidPartTableAskUserAndWipe(selectedDisk);
        }

        // Invoke cfdisk command for chosen drive.
        snprintf(cfdiskCmd, UTIL_MAX_CMD_LENGTH, "cfdisk %s", selectedDisk->device);

        system(cfdiskCmd);

        ad_restore();
    }

    msg_reminderToFormatNewPartition();
    return WIZ_MAIN_MENU;
}

// Refreshes the internal hard disk array.
bool qi_refreshDisks(qi_InstallContext *ctx) {
    QI_FATAL(ctx != NULL, "Invalid installation context state");

    ad_setFooterText("Obtaining System Hard Disk Information...");
    if (ctx->hda != NULL) {
        util_hardDiskArrayDestroy(ctx->hda);
    }
    ctx->hda = util_getSystemHardDisks();
    ctx->destination = NULL;
    ad_clearFooter();
    return ctx->hda != NULL;
}
