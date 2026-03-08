#include "install.h"
#include "util.h"
#include "qi_assert.h"
#include "anbui/anbui.h"

#include <errno.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

// Structure describing a PCI DEVICE with vendor, device and IRQ.
typedef struct {
    struct {
        uint16_t busId : 8;
        uint16_t devId : 5;
        uint16_t funId : 3;
    } ;
    uint16_t vendor;
    uint16_t device;
    uint16_t irq;
} qi_PciDevice;

// Structure containing list of devices
typedef struct {
    size_t deviceCount;
    qi_PciDevice *items;
} qi_PciDeviceList;

// Structure describing a quirk check.
// They return true if the quirk exists.
typedef struct {
    bool (*check)(qi_PciDeviceList *list);  // Function to check for the quirk. 
                                            // Returns TRUE if quirk is not found/relevant.
                                            // FALSE if quirk is problematic.
    const char *info;                       // Message to display when the problem has been detected.
} qi_HwQuirk;

// Add a device to a PCI device list.
static void qi_pciDeviceListAdd(qi_PciDeviceList *list, const qi_PciDevice *dev) {
    list->deviceCount++;
    list->items = realloc(list->items, list->deviceCount * sizeof(qi_PciDevice));
    QI_FATAL(list->items != NULL, "Failed to allocate PCI device list");
    list->items[list->deviceCount - 1] = *dev;
}

// Destroy/free PCI device list.
static void qi_pciDeviceListDestroy(qi_PciDeviceList *list) {
    if (list != NULL && list->items != NULL) {
        free (list->items);
    }
    list->deviceCount = 0;
    list->items = NULL;
}

// Populate device list with all PCI devices in the system
static bool qi_pciDeviceListPopulate(qi_PciDeviceList *list) {
    char line[1024];
    FILE *f = fopen("/proc/bus/pci/devices", "r");

    list->deviceCount = 0;

    // This should only happen when there is no PCI bus.
    // We may be installing on a VLB only system so this is NOT
    // an error.
    if (f == NULL) return true;

    while (fgets(line, sizeof(line), f)) {
        unsigned int busDevfn, vendorDevice, irq;

        if (sscanf(line, "%x %x %x", &busDevfn, &vendorDevice, &irq) != 3) {
            continue;
        }

        qi_PciDevice dev = {
            .busId  = (busDevfn >> 8) & 0xFF,
            .devId  = (busDevfn >> 3) & 0x1F,
            .funId  = (busDevfn >> 0) & 0x07,
            .vendor = (vendorDevice >> 16) & 0xFFFF,
            .device = (vendorDevice >>  0) & 0xFFFF,
            .irq    = irq,
        };

        qi_pciDeviceListAdd(list, &dev);
    }

    fclose(f);
    return true;
}

// Get the device with given ven / dev (first one that is found)
qi_PciDevice *qi_pciDeviceGet(qi_PciDeviceList *list, uint16_t ven, uint16_t dev) {
    for (size_t i = 0; i < list->deviceCount; i++) {
        if (list->items[i].vendor == ven && list->items[i].device == dev) {
            return &list->items[i];
        }
    }
    return NULL;
}

// Check if a device with given vendor/device ID exists
bool qi_pciDeviceExists(qi_PciDeviceList *list, uint16_t ven, uint16_t dev) {
    return NULL != qi_pciDeviceGet(list, ven, dev);
}

// Get IRQ of a device. returns -1 if device doesn't exist
int32_t qi_pciDeviceIrq(qi_PciDeviceList *list, uint16_t ven, uint16_t dev) {
    qi_PciDevice *pd = qi_pciDeviceGet(list, ven, dev);
    return pd != NULL ? (int32_t) pd->irq : -1;
}

// Check presence of ICH5 SATA controller with USB controller on the same IRQ
bool quirk_i875Sata(qi_PciDeviceList *list) {
    int32_t sataIrq = qi_pciDeviceIrq(list, 0x8086, 0x24D1);
    // If ICH5 SATA Ctrl isn't found it might be 24DF instead.
    if (sataIrq < 0) sataIrq = qi_pciDeviceIrq(list, 0x8086, 0x24DF);
    // Device Not found -> it's all good
    if (sataIrq < 0) return true;
    // We have the device, check for shared IRQs...
    return  sataIrq != qi_pciDeviceIrq(list, 0x8086, 0x24D2) &&
            sataIrq != qi_pciDeviceIrq(list, 0x8086, 0x24D4) &&
            sataIrq != qi_pciDeviceIrq(list, 0x8086, 0x24D7) &&
            sataIrq != qi_pciDeviceIrq(list, 0x8086, 0x24DE) &&
            sataIrq != qi_pciDeviceIrq(list, 0x8086, 0x24DD);
}

// All the known hardware quirks
static const qi_HwQuirk quirks[] = {
    { quirk_i875Sata,   "You are running an Intel-Chipset motherboard with an early integrated SATA\n"
                        "controller in 'enhanced' mode and have the USB controller(s) enabled.\n\n"
                        "They share an IRQ and due to a BIOS bug, it will likely cause Windows 9x to\n"
                        "HANG on boot or during installation.\n\n"
                        "Please either switch the SATA controller to IDE Primary or Secondary mode\n"
                        "or disable the USB Controller." },
};

bool inst_doHardwareQuirks(void) {
    qi_PciDeviceList devices = {0};

    if (!qi_pciDeviceListPopulate(&devices)) {
        qi_pciDeviceListDestroy(&devices);
        ad_okBox("ERROR", false, "Failed to obtain PCI device list.");
        return false;
    }

    // Go through all known hardware quirks and check them.
    // If one returns FALSE, the user is asked for confirmation if he wants to continue.
    bool ret = true;

    for (size_t i = 0; i < util_arraySize(quirks); i++) {
        if (quirks[i].check(&devices) == false) {
            int32_t action = ad_yesNoBox("Known Hardware Conflict Detected!", true,
                "Found a hardware combination with a known problem:\n\n"
                "%s\n\n"
                "Do you acknowledge this and wish to continue at your own risk?", 
                quirks[i].info);

            if (action == AD_YESNO_NO || action == AD_CANCELED) {
                ret = false;
                break;
            }
        }
    }

    qi_pciDeviceListDestroy(&devices);
    return ret;
}
