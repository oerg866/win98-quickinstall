#include "util.h"

#define QI_OPTION_YES               (0)
#define QI_OPTION_NO                (1)
#define QI_MAX_OPTIONS_PER_CONFIG   (2)

typedef enum {
    o_writeMBRAndSetActive = 0,
    o_formatTargetPartition,
    o_bootSector,
    o_skipLegacyDetection,
    o_installDriversBase,
    o_installDriversExtra,
    o_copyExtras,
    o_cregfix,
    o_lba64,
    o_uefi,
    QI_OPTIONIDX_MAX,
    o_baseOS, // Base OS copy, this is always enabled and this is just a hack to make the install code a bit nicer
    o_mount, // Mounting partition, ditto
    o_registry, // copy system registry, ditto
} qi_OptionIdx;

typedef struct {
    qi_OptionIdx idx;
    const char *prompt;
    size_t optionCount;
    const char *optionStrings[QI_MAX_OPTIONS_PER_CONFIG];
    size_t selected;
    size_t progressBarIdx;
    bool partOfPreparation;
} qi_Option;

static qi_Option qi_options[] = {
    {
        o_writeMBRAndSetActive,
        "Write Master Boot Record and set partition active", 2,
        { "[YES]", "[NO ]" },
        QI_OPTION_YES, 0, true
    },
    {
        o_bootSector,
        "Write new boot sector to the destination partition", 2,
        { "[YES]", "[NO ]", },
        QI_OPTION_YES, 0, true
    },
    {
        o_formatTargetPartition,
        "Format target partition", 2,
        { "[YES]", "[NO ]", },
        QI_OPTION_YES, 0, true
    },
    { 
        o_skipLegacyDetection,  
        "Skip legacy non-PnP hardware detection phase", 2,
        { "[YES]", "[NO ]", },
        QI_OPTION_YES, 0, false
    },

    {
        o_installDriversBase,
        "Install integrated driver library", 2,
        { "[YES]", "[NO ]", },
        QI_OPTION_YES, 0, false
    },
    {
        o_installDriversExtra,
        "Copy extended driver library (DRIVER.EX)", 2,
        { "[YES]", "[NO ]", },
        QI_OPTION_NO, 0, false
    },
    {
        o_copyExtras,
        "Copy extras folder (big drivers, tools, updates...)", 2,
        { "[YES]", "[NO ]", },
        QI_OPTION_NO, 0, false
    },
    {
        o_cregfix,
        "Apply CREGFIX patch for modern CPUs (by SweetLow)", 2,
        { "[YES]", "[NO ]", },
        QI_OPTION_NO, 0, false
    },
    {
        o_lba64,
        "Install 64-Bit LBA (> 2 TB) and GPT disk support (By SweetLow)", 2,
        { "[YES]", "[NO ]", },
        QI_OPTION_NO, 0, false
    },
    {
        o_uefi,
        "Install UEFI support (*extremely* experimental)", 2,
        { "[YES]", "[NO ]", },
        QI_OPTION_NO, 0, true
    },
    {
        o_baseOS,
        "Install operating system", 1,
        { "[YES]" },
        QI_OPTION_YES, 1, false
    },
    {
        o_mount,
        "Mount destination partition", 1,
        { "[YES]" },
        QI_OPTION_YES, 0, true
    },
    {
        o_registry,
        "Copy system registry", 1,
        { "[YES]" },
        QI_OPTION_YES, 2, false
    },
};

#define QI_OPTION_ARRAY_SIZE (util_arraySize(qi_options))
