/*
 * OS Installer implementation for Windows 9x
 * (C) 2025 Eric Voirin (oerg866@googlemail.com)
 */

#include "install.h"

#include <stdlib.h>

#include "qi_assert.h"
#include "mappedfile.h"
#include "util.h"
#include "version.h"

#include "anbui/anbui.h"

static const char *s_registryUnpackFile = NULL;

/* Asks user which version of the hardware detection scheme he wants */
static const char *win9x_askUserForHWDetectionVariant(void) {
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

bool win9x_checkPartition(inst_Context *inst) {
    QI_ASSERT(inst != NULL);

    if (inst->destinationPartition->fileSystem == fs_fat16 ||  inst->destinationPartition->fileSystem == fs_fat32) {
        return true;
    } else {
        return false;
    }
}

bool win9x_prepareInstall(inst_Context *inst) {
    QI_ASSERT(inst != NULL);

    s_registryUnpackFile = win9x_askUserForHWDetectionVariant();

    if (s_registryUnpackFile == NULL) {
        return false;
    }
    
    return true;
}

inst_InstallStatus win9x_doInstall(inst_Context *inst) {
    bool installSuccess = false;
    
    QI_ASSERT(inst != NULL);
    QI_ASSERT(inst->destinationPartition != NULL);

    // partition is already mounted at this point
    // sourceFile is already opened at this point for readahead prebuffering
    installSuccess = inst_copyFiles(inst->sourceFile, inst->destinationPartition->mountPath, "Operating System");
    mappedFile_close(inst->sourceFile);

    if (!installSuccess) {
        return INST_FILE_COPY_ERROR;
    }

    // If driver data copy was successful, install registry for selceted hardware detection variant
    inst->sourceFile = inst_openSourceFile(inst->osVariantIndex, s_registryUnpackFile, inst->readaheadSize);
    QI_ASSERT(inst->sourceFile && "Failed to open registry file");
    installSuccess = inst_copyFiles(inst->sourceFile, inst->destinationPartition->mountPath, "Registry");
    mappedFile_close(inst->sourceFile);

    if (!installSuccess) {
        return INST_FILE_COPY_ERROR;
    }

    return INST_OK;
}

#include "mbr_boot_win98.h"

const inst_OSInstaller inst_win9xInstaller = {
    win9x_checkPartition,
    win9x_prepareInstall,
    win9x_doInstall,
    &__WIN98_FAT16_BOOT_SECTOR_MODIFIER_LIST__,
    &__WIN98_FAT32_BOOT_SECTOR_MODIFIER_LIST__,
};
