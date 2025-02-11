/*
 * OS Installer implementation for Windows XP
 * (C) 2025 Eric Voirin (oerg866@googlemail.com)
 */

#include "install.h"

#include <stdlib.h>

#include "qi_assert.h"
#include "mappedfile.h"
#include "util.h"
#include "version.h"

#include "anbui/anbui.h"

typedef void (*winxp_HALProcessFunc)(inst_Context *inst);

typedef struct {
    const char *key;
    const char *valueName;
    const char *valueData;
} winxp_HALRegistryEntry;

typedef struct {
    const char                     *halName;        // Name of this HAL
    const char                     *halInfSection;  // Registry HW enum entry
    const char                     *halDeviceId;    // Registry HW enum entry
    const char                     *haldllFile;     // HAL.dll name
    const char                     *ntoskrnlFile;   // NTOSKRNL.exe name
    const char                     *ntkrnlpaFile;   // NTKRNLPA.exe name
    const winxp_HALRegistryEntry   *regEntries;
} winxp_HAL;

#define WINXP_REG_HEADER "Windows Registry Editor Version 5.00"

#define WINXP_QI_DLL_SEARCH_PATH "/_qi/"

#define WINXP_SYSTEM_HIVE "/WINDOWS/system32/config/SYSTEM"
#define WINXP_SYSTEM_HIVE_PREFIX "\"HKEY_LOCAL_MACHINE\\SYSTEM\""

#define WINXP_REG_HAL_CLASS_CONTROL "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Control\\Class\\{4D36E966-E325-11CE-BFC1-08002BE10318}\\0000"

// HKEY_LOCAL_MACHINE\SYSTEM\ControlSet001\Services\ACPI]

static const winxp_HAL *s_systemHAL = NULL;

#define HAL_REG_ENTRY_END_OF_LIST { NULL, NULL, NULL }

static const winxp_HALRegistryEntry winxp_HALRegistryEntries_ACPI[] = {
/*    { WINXP_REG_HAL_CLASS_CONTROL, "DeviceDesc",           "\"Advanced Configuration and Power Interface (ACPI) PC\"" },
    { WINXP_REG_HAL_CLASS_CONTROL, "InfSection",           "\"ACPIPIC_UP_HAL\"" },
    { WINXP_REG_HAL_CLASS_CONTROL, "MatchingDeviceId",     "\"acpipic_up\"" }, */

    { WINXP_REG_HAL_CLASS_CONTROL, "CoInstallers32", "-"},

    { "-HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Control\\Class\\{D45B1C18-C8FA-11D1-9F77-0000F805F530}\\0000", NULL, NULL },

    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Control\\Class\\{4D36E96B-E325-11CE-BFC1-08002BE10318}\\0000", "MatchingDeviceId", "\"*pnp0303\"" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Control\\Class\\{4D36E96B-E325-11CE-BFC1-08002BE10318}\\0000", "DriverDesc", "\"Standard 101/102-Key or Microsoft Natural PS/2 Keyboard\"" },

    { "-HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Control\\DeviceClasses\\{378de44c-56ef-11d1-bc8c-00a0c91405dd}", NULL, NULL },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Control\\DeviceClasses\\{378de44c-56ef-11d1-bc8c-00a0c91405dd}", NULL, NULL },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Control\\DeviceClasses\\{378de44c-56ef-11d1-bc8c-00a0c91405dd}\\##?#ACPI#PNP0F03#4&1d401fb5&0#{378de44c-56ef-11d1-bc8c-00a0c91405dd}", "DeviceInstance", "\"ACPI\\\\PNP0F03\\\\4&1d401fb5&0\"" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Control\\DeviceClasses\\{378de44c-56ef-11d1-bc8c-00a0c91405dd}\\##?#ACPI#PNP0F03#4&1d401fb5&0#{378de44c-56ef-11d1-bc8c-00a0c91405dd}\\#", "SymbolicLink", "\"\\\\\\\\?\\\\ACPI#PNP0F03#4&1d401fb5&0#{378de44c-56ef-11d1-bc8c-00a0c91405dd}\"" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Control\\DeviceClasses\\{378de44c-56ef-11d1-bc8c-00a0c91405dd}\\##?#ACPI#PNP0F03#4&1d401fb5&0#{378de44c-56ef-11d1-bc8c-00a0c91405dd}\\#\\Control", "Linked", "dword:00000001" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Control\\DeviceClasses\\{378de44c-56ef-11d1-bc8c-00a0c91405dd}\\##?#ACPI#PNP0F03#4&1d401fb5&0#{378de44c-56ef-11d1-bc8c-00a0c91405dd}\\Control", "ReferenceCount", "dword:00000001" },

    { "-HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Control\\DeviceClasses\\{884b96c3-56ef-11d1-bc8c-00a0c91405dd}", NULL, NULL },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Control\\DeviceClasses\\{884b96c3-56ef-11d1-bc8c-00a0c91405dd}", NULL, NULL },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Control\\DeviceClasses\\{884b96c3-56ef-11d1-bc8c-00a0c91405dd}\\##?#ACPI#PNP0303#4&1d401fb5&0#{884b96c3-56ef-11d1-bc8c-00a0c91405dd}", "DeviceInstance", "\"ACPI\\\\PNP0303\\\\4&1d401fb5&0\"" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Control\\DeviceClasses\\{884b96c3-56ef-11d1-bc8c-00a0c91405dd}\\##?#ACPI#PNP0303#4&1d401fb5&0#{884b96c3-56ef-11d1-bc8c-00a0c91405dd}\\#", "SymbolicLink", "\"\\\\\\\\?\\\\ACPI#PNP0303#4&1d401fb5&0#{884b96c3-56ef-11d1-bc8c-00a0c91405dd}\"" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Control\\DeviceClasses\\{884b96c3-56ef-11d1-bc8c-00a0c91405dd}\\##?#ACPI#PNP0303#4&1d401fb5&0#{884b96c3-56ef-11d1-bc8c-00a0c91405dd}\\#\\Control", "Linked", "dword:00000001" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Control\\DeviceClasses\\{884b96c3-56ef-11d1-bc8c-00a0c91405dd}\\##?#ACPI#PNP0303#4&1d401fb5&0#{884b96c3-56ef-11d1-bc8c-00a0c91405dd}\\Control", "ReferenceCount", "dword:00000001" },

    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Control\\PnP", "DisableFirmwareMapper", "dword:00000000" },

    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Control\\PnP\\PciIrqRouting", "$PIROffset",       "-" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Control\\PnP\\PciIrqRouting", "Status",           "-" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Control\\PnP\\PciIrqRouting", "TableStatus",      "-" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Control\\PnP\\PciIrqRouting", "MiniportStatus",   "-" },
    
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Services\\atapi\\Parameters", "LegacyDetection",  "dword:00000000" },

    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Control\\CriticalDeviceDatabase\\*pnp0000",           "ClassGUID",    "\"{4D36E97D-E325-11CE-BFC1-08002BE10318}\"" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Control\\CriticalDeviceDatabase\\*pnp0100",           "ClassGUID",    "\"{4D36E97D-E325-11CE-BFC1-08002BE10318}\"" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Control\\CriticalDeviceDatabase\\*pnp0200",           "ClassGUID",    "\"{4D36E97D-E325-11CE-BFC1-08002BE10318}\"" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Control\\CriticalDeviceDatabase\\*pnp0200",           "ClassGUID",    "\"{4D36E97D-E325-11CE-BFC1-08002BE10318}\"" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Control\\CriticalDeviceDatabase\\*pnp0303",           "Service",      "\"i8042prt\"" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Control\\CriticalDeviceDatabase\\*pnp0303",           "ClassGUID",    "\"{4D36E96B-E325-11CE-BFC1-08002BE10318}\"" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Control\\CriticalDeviceDatabase\\*pnp0a08",           "Service",      "\"pci\"" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Control\\CriticalDeviceDatabase\\*pnp0a08",           "ClassGUID",    "\"{4D36E97D-E325-11CE-BFC1-08002BE10318}\"" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Control\\CriticalDeviceDatabase\\*pnp0c08",           "Service",      "\"ACPI\"" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Control\\CriticalDeviceDatabase\\*pnp0c08",           "ClassGUID",    "\"{4D36E97D-E325-11CE-BFC1-08002BE10318}\"" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Control\\CriticalDeviceDatabase\\*pnp0f03",           "Service",      "\"i8042prt\"" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Control\\CriticalDeviceDatabase\\*pnp0f03",           "ClassGUID",    "\"{4D36E96F-E325-11CE-BFC1-08002BE10318}\"" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Control\\CriticalDeviceDatabase\\acpi#fixedbutton",   "ClassGUID",    "\"{4D36E97D-E325-11CE-BFC1-08002BE10318}\"" },

    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Services\\ACPI", "Start",        "dword:00000000" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Services\\ACPI", "DisplayName",  "\"Microsoft ACPI Driver\"" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Services\\ACPI", "ImageName",    "hex(2):73,00,79,00,73,00,74,00,65,00,6d,00,33,00,32,00,5c,00,44,00,52,00,49,00,56,00,45,00,52,00,53,00,5c,00,41,00,43,00,50,00,49,00,2e,00,73,00,79,00,73,00,00,00" },

    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Services\\ACPI\\Enum", "0",              "\"ACPI_HAL\\\\PNP0C08\\\\0\"" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Services\\ACPI\\Enum", "Count",          "dword:00000001" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Services\\ACPI\\Enum", "NextInstance",   "dword:00000001" },

    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Services\\Kbdclass\\Enum", "0", "\"ACPI\\\\PNP0303\\\\4&1d401fb5&0\"" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Services\\Mouclass\\Enum", "0", "\"ACPI\\\\PNP0F03\\\\4&1d401fb5&0\"" },

    { "-HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Services\\NtApm\\Security", NULL, NULL },
    { "-HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Services\\NtApm\\Enum", NULL, NULL },
    { "-HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Services\\NtApm", NULL, NULL },
    
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Services\\PCI\\Enum", "0", "\"ACPI\\\\PNP0A03\\\\0\"" },
    
    { "-HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\PCI_HAL", NULL, NULL },
    { "-HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\Root\\NTAPM", NULL, NULL },
    { "-HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\Root\\PCI_HAL", NULL, NULL },

    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI_HAL\\PNP0C08\\0", "Capabilities",   "dword:00000030" },

    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI_HAL\\PNP0C08\\0", "HardwareID",     "hex(7):41,00,43,00,50,00,49,00,5f,00,48,00,41,00,4c,00,5c,00,50,00,4e,00,50,00,30,00,43,00,30,00,38,00,00,00,2a,00,50,00,4e,00,50,00,30,00,43,00,30,00,38,00,00,00,00,00" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI_HAL\\PNP0C08\\0", "Service",        "\"ACPI\"" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI_HAL\\PNP0C08\\0", "ConfigFlags",    "dword:00000000" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI_HAL\\PNP0C08\\0", "ParentIdPrefix", "\"2&daba3ff&0\"" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI_HAL\\PNP0C08\\0", "ClassGUID",      "\"{4D36E97D-E325-11CE-BFC1-08002BE10318}\"" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI_HAL\\PNP0C08\\0", "Class",          "\"System\"" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI_HAL\\PNP0C08\\0", "Driver",         "\"{4D36E97D-E325-11CE-BFC1-08002BE10318}\\\\0005\"" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI_HAL\\PNP0C08\\0", "Mfg",            "\"Microsoft\"" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI_HAL\\PNP0C08\\0", "DeviceDesc",     "\"Microsoft ACPI-Compliant System\"" },

/*TODO
[HKEY_LOCAL_MACHINE\SYSTEM\ControlSet001\Enum\Root]

[HKEY_LOCAL_MACHINE\SYSTEM\ControlSet001\Enum\Root\ACPI_HAL]

[HKEY_LOCAL_MACHINE\SYSTEM\ControlSet001\Enum\Root\ACPI_HAL\0000]
"ConfigFlags"=dword:00000000
"Legacy"=dword:00000000
"DeviceReported"=dword:00000001
"Service"="\\Driver\\ACPI_HAL"
"HardwareID"=hex(7):61,00,63,00,70,00,69,00,61,00,70,00,69,00,63,00,5f,00,6d,\
  00,70,00,00,00,00,00
"CompatibleIDs"=hex(7):44,00,45,00,54,00,45,00,43,00,54,00,45,00,44,00,49,00,\
  6e,00,74,00,65,00,72,00,6e,00,61,00,6c,00,5c,00,41,00,43,00,50,00,49,00,5f,\
  00,48,00,41,00,4c,00,00,00,44,00,45,00,54,00,45,00,43,00,54,00,45,00,44,00,\
  5c,00,41,00,43,00,50,00,49,00,5f,00,48,00,41,00,4c,00,00,00,00,00
"Capabilities"=dword:00000000
"ClassGUID"="{4D36E966-E325-11CE-BFC1-08002BE10318}"
"Class"="Computer"
"Driver"="{4D36E966-E325-11CE-BFC1-08002BE10318}\\0000"
"Mfg"="(Standard computers)"
"DeviceDesc"="ACPI Multiprocessor PC"

[HKEY_LOCAL_MACHINE\SYSTEM\ControlSet001\Enum\Root\ACPI_HAL\0000\LogConf]

[HKEY_LOCAL_MACHINE\SYSTEM\ControlSet001\Enum\Root\ACPI_HAL\0000\Control]
"DeviceReported"=dword:00000001*/


{ "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0303\\4&1d401fb5&0", "Capabilities", "dword:00000020" },
{ "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0303\\4&1d401fb5&0", "HardwareID",   "hex(7):41,00,43,00,50,00,49,00,5c,00,50,00,4e,00,50,00,30,00,33,00,30,00,33,00,00,00,2a,00,50,00,4e,00,50,00,30,00,33,00,30,00,33,00,00,00,00,00" },
{ "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0303\\4&1d401fb5&0", "Service",      "\"i8042prt\"" },
{ "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0303\\4&1d401fb5&0", "ClassGUID",    "\"{4D36E96B-E325-11CE-BFC1-08002BE10318}\"" },
{ "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0303\\4&1d401fb5&0", "ConfigFlags",  "dword:00000000" },
{ "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0303\\4&1d401fb5&0", "Driver",       "\"{4D36E96B-E325-11CE-BFC1-08002BE10318}\\\\0000\"" },
{ "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0303\\4&1d401fb5&0", "Class",        "\"Keyboard\"" },
{ "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0303\\4&1d401fb5&0", "Mfg",          "\"(Standard keyboards)\"" },
{ "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0303\\4&1d401fb5&0", "DeviceDesc",   "\"Standard 101/102-Key or Microsoft Natural PS/2 Keyboard\"" },

{ "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0303\\4&1d401fb5&0\\Device Parameters", "FirmwareIdentified",      "dword:00000001" },
{ "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0303\\4&1d401fb5&0\\Device Parameters", "Migrated",                "dword:00000001" },
{ "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0303\\4&1d401fb5&0\\Device Parameters", "KeyboardDataQueueSize",   "dword:00000064" },
{ "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0303\\4&1d401fb5&0\\Device Parameters", "PollStatusIterations",    "dword:00000001" },

{ "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0303\\4&1d401fb5&0\\LogConf", "BasicConfigVector", "hex(a):88,00,00,00,0f,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,01,00,00,00,01,00,01,00,03,00,00,00,00,01,01,00,11,00,00,00,01,00,00,00,00,00,00,00,60,00,00,00,00,00,00,00,60,00,00,00,00,00,00,00,00,01,01,00,11,00,00,00,01,00,00,00,00,00,00,00,64,00,00,00,00,00,00,00,64,00,00,00,00,00,00,00,00,02,01,00,01,00,00,00,01,00,00,00,01,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00" },
{ "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0303\\4&1d401fb5&0\\LogConf", "BootConfig",        "hex(8):01,00,00,00,0f,00,00,00,00,00,00,00,01,00,01,00,03,00,00,00,01,01,11,00,60,00,00,00,00,00,00,00,01,00,00,00,01,01,11,00,64,00,00,00,00,00,00,00,01,00,00,00,02,01,01,00,01,00,00,00,01,00,00,00,ff,ff,ff,ff" },

{ "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0303\\4&1d401fb5&0\\Control", "ActiveService",         "\"Kbdclass\"" },
{ "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0303\\4&1d401fb5&0\\Control", "FilteredConfigVector",  "hex(a):a8,00,00,00,0f,00,00,00,00,00,00,00,00,00,00,00,26,00,00,00,50,00,72,00,6f,00,63,00,01,00,00,00,01,00,01,00,04,00,00,00,01,80,03,00,00,00,00,00,01,00,00,00,69,00,6e,00,67,00,00,00,41,00,4d,00,44,00,20,00,52,00,79,00,01,01,01,80,11,00,00,00,01,00,00,00,01,00,00,00,60,00,00,00,00,00,00,00,60,00,00,00,00,00,00,00,01,01,01,80,11,00,00,00,01,00,00,00,01,00,00,00,64,00,00,00,00,00,00,00,64,00,00,00,00,00,00,00,01,02,01,80,01,00,00,00,01,00,00,00,01,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00" },
{ "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0303\\4&1d401fb5&0\\Control", "AllocConfig",           "hex(8):01,00,00,00,0f,00,00,00,00,00,00,00,00,00,00,00,03,00,00,00,01,01,11,00,60,00,00,00,00,00,00,00,01,00,00,00,01,01,11,00,64,00,00,00,00,00,00,00,01,00,00,00,02,01,01,00,01,00,00,00,01,00,00,00,ff,ff,ff,ff" },

{ "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0303\\4&1d401fb5&0", "Capabilities", "dword:00000020" },
{ "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0303\\4&1d401fb5&0", "HardwareID",   "hex(7):41,00,43,00,50,00,49,00,5c,00,50,00,4e,00,50,00,30,00,33,00,30,00,33,00,00,00,2a,00,50,00,4e,00,50,00,30,00,33,00,30,00,33,00,00,00,00,00" },
{ "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0303\\4&1d401fb5&0", "Service",      "\"i8042prt\"" },
{ "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0303\\4&1d401fb5&0", "ClassGUID",    "\"{4D36E96B-E325-11CE-BFC1-08002BE10318}\"" },
{ "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0303\\4&1d401fb5&0", "ConfigFlags",  "dword:00000000" },
{ "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0303\\4&1d401fb5&0", "Driver",       "\"{4D36E96B-E325-11CE-BFC1-08002BE10318}\\\\0000\"" },
{ "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0303\\4&1d401fb5&0", "Class",        "\"Keyboard\"" },
{ "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0303\\4&1d401fb5&0", "Mfg",          "\"(Standard keyboards)\"" },
{ "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0303\\4&1d401fb5&0", "DeviceDesc",   "\"Standard 101/102-Key or Microsoft Natural PS/2 Keyboard\"" },

{ "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0F03\\4&1d401fb5&0", "Capabilities",   "dword:00000020" },
{ "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0F03\\4&1d401fb5&0", "HardwareID",     "hex(7):41,00,43,00,50,00,49,00,5c,00,50,00,4e,00,50,00,30,00,46,00,30,00,33,00,00,00,2a,00,50,00,4e,00,50,00,30,00,46,00,30,00,33,00,00,00,00,00" },
{ "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0F03\\4&1d401fb5&0", "Service",        "\"i8042prt\"" },
{ "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0F03\\4&1d401fb5&0", "ClassGUID",      "\"{4D36E96F-E325-11CE-BFC1-08002BE10318}\"" },
{ "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0F03\\4&1d401fb5&0", "ConfigFlags",    "dword:00000000" },
{ "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0F03\\4&1d401fb5&0", "Driver",         "\"{4D36E96F-E325-11CE-BFC1-08002BE10318}\\\\0000\"" },
{ "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0F03\\4&1d401fb5&0", "Class",          "\"Mouse\"" },
{ "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0F03\\4&1d401fb5&0", "Mfg",            "\"Microsoft\"" },
{ "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0F03\\4&1d401fb5&0", "DeviceDesc",     "\"Microsoft PS/2 Mouse\"" },

{ "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0F03\\4&1d401fb5&0\\Device Parameters", "FirmwareIdentified",      "dword:00000001" },
{ "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0F03\\4&1d401fb5&0\\Device Parameters", "Migrated",                "dword:00000001" },
{ "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0F03\\4&1d401fb5&0\\Device Parameters", "EnableWheelDetection",    "dword:00000002" },
{ "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0F03\\4&1d401fb5&0\\Device Parameters", "MouseDataQueueSize",      "dword:00000064" },
{ "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0F03\\4&1d401fb5&0\\Device Parameters", "MouseResolution",         "dword:00000003" },
{ "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0F03\\4&1d401fb5&0\\Device Parameters", "MouseSynchIn100ns",       "dword:01312d00" },
{ "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0F03\\4&1d401fb5&0\\Device Parameters", "SampleRate",              "dword:00000064" },
{ "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0F03\\4&1d401fb5&0\\Device Parameters", "WheelDetectionTimeout",   "dword:000005dc" },
{ "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0F03\\4&1d401fb5&0\\Device Parameters", "MouseInitializePolled",   "dword:00000000" },

{ "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0F03\\4&1d401fb5&0\\LogConf", "BasicConfigVector", "hex(a):48,00,00,00,0f,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,01,00,00,00,01,00,01,00,01,00,00,00,00,02,01,00,01,00,00,00,0c,00,00,00,0c,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00" },
{ "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0F03\\4&1d401fb5&0\\LogConf", "BootConfig",        "hex(8):01,00,00,00,0f,00,00,00,00,00,00,00,01,00,01,00,01,00,00,00,02,01,01,00,0c,00,00,00,0c,00,00,00,ff,ff,ff,ff" },

{ "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0F03\\4&1d401fb5&0\\Control", "ActiveService",         "\"Mouclass\"" },
{ "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0F03\\4&1d401fb5&0\\Control", "FilteredConfigVector",  "hex(a):68,00,00,00,0f,00,00,00,00,00,00,00,00,00,00,00,02,00,00,00,01,00,00,00,49,6d,61,67,01,00,00,00,01,00,01,00,02,00,00,00,01,80,03,00,00,00,00,00,01,00,00,00,33,00,32,00,5c,00,44,00,52,00,49,00,56,00,45,00,52,00,53,00,01,02,01,80,01,00,00,00,0c,00,00,00,0c,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00" },
{ "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0F03\\4&1d401fb5&0\\Control", "AllocConfig",           "hex(8):01,00,00,00,0f,00,00,00,00,00,00,00,00,00,00,00,01,00,00,00,02,01,01,00,0c,00,00,00,0c,00,00,00,ff,ff,ff,ff" },

/*


[HKEY_LOCAL_MACHINE\SYSTEM\ControlSet001\Enum\ACPI\PNP0A03\0]
"Capabilities"=dword:00000030
"HardwareID"=hex(7):41,00,43,00,50,00,49,00,5c,00,50,00,4e,00,50,00,30,00,41,00,30,00,33,00,00,00,2a,00,50,00,4e,00,50,00,30,00,41,00,30,00,33,00,00,00,00,00
"Service"="pci"
"ConfigFlags"=dword:00000000
"ParentIdPrefix"="3&267a616a&0"
"ClassGUID"="{4D36E97D-E325-11CE-BFC1-08002BE10318}"
"Class"="System"
"Driver"="{4D36E97D-E325-11CE-BFC1-08002BE10318}\\0007"
"UINumberDescFormat"="PCI Slot %1!u!"
"Mfg"="(Standard system devices)"
"DeviceDesc"="PCI bus"

[HKEY_LOCAL_MACHINE\SYSTEM\ControlSet001\Enum\ACPI\PNP0A03\0\Device Parameters]
"FirmwareIdentified"=dword:00000001

[HKEY_LOCAL_MACHINE\SYSTEM\ControlSet001\Enum\ACPI\PNP0A03\0\LogConf]
"BasicConfigVector"=hex(a):68,02,00,00,0f,00,00,00,00,00,00,00,00,00,00,00,00,\
  00,00,00,00,00,00,00,00,00,00,00,01,00,00,00,01,00,01,00,12,00,00,00,00,06,\
  03,00,00,00,00,00,00,01,00,00,00,00,00,00,ff,00,00,00,00,00,00,00,00,00,00,\
  00,00,00,00,00,00,81,03,00,00,00,00,00,00,01,00,00,00,00,00,00,ff,00,00,00,\
  00,00,00,00,00,00,00,00,00,00,00,00,00,01,03,00,20,00,00,00,f8,0c,00,00,01,\
  00,00,00,00,00,00,00,00,00,00,00,f7,0c,00,00,00,00,00,00,00,81,03,00,20,00,\
  00,00,f8,0c,00,00,01,00,00,00,00,00,00,00,00,00,00,00,f7,0c,00,00,00,00,00,\
  00,00,81,00,00,00,60,00,00,01,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,\
  00,00,00,00,00,00,00,00,00,81,00,00,00,60,00,00,01,00,00,00,00,00,00,00,00,\
  00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,01,03,00,20,00,00,00,00,f3,\
  00,00,01,00,00,00,00,0d,00,00,00,00,00,00,ff,ff,00,00,00,00,00,00,00,81,03,\
  00,20,00,00,00,00,f3,00,00,01,00,00,00,00,0d,00,00,00,00,00,00,ff,ff,00,00,\
  00,00,00,00,00,81,00,00,00,60,00,00,01,00,00,00,00,0d,00,00,00,00,00,00,00,\
  00,00,00,00,00,00,00,00,00,00,00,00,81,00,00,00,60,00,00,01,00,00,00,00,0d,\
  00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,03,03,00,20,00,00,\
  00,00,00,02,00,01,00,00,00,00,00,0a,00,00,00,00,00,ff,ff,0b,00,00,00,00,00,\
  00,81,03,00,20,00,00,00,00,00,02,00,01,00,00,00,00,00,0a,00,00,00,00,00,ff,\
  ff,0b,00,00,00,00,00,00,81,00,00,00,60,00,00,03,00,00,00,00,00,0a,00,00,00,\
  00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,81,00,00,00,60,00,00,03,00,00,\
  00,00,00,0a,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,03,03,00,\
  20,00,00,00,00,00,00,ee,01,00,00,00,00,00,00,10,00,00,00,00,ff,ff,ff,fd,00,\
  00,00,00,00,81,03,00,20,00,00,00,00,00,00,ee,01,00,00,00,00,00,00,10,00,00,\
  00,00,ff,ff,ff,fd,00,00,00,00,00,81,00,00,00,60,00,00,03,00,00,00,00,00,00,\
  10,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,81,00,00,00,60,00,00,\
  03,00,00,00,00,00,00,10,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00
"BootConfig"=hex(8):01,00,00,00,0f,00,00,00,00,00,00,00,01,00,01,00,12,00,00,\
  00,06,03,00,00,00,00,00,00,00,01,00,00,00,00,00,00,81,03,00,00,00,01,00,00,\
  00,00,00,00,ff,00,00,00,01,03,20,00,00,00,00,00,00,00,00,00,f8,0c,00,00,81,\
  03,20,00,f8,0c,00,00,01,00,00,00,00,00,00,00,81,00,00,60,01,00,00,00,00,00,\
  00,00,00,00,00,00,81,00,00,60,01,00,00,00,00,00,00,00,00,00,00,00,01,03,20,\
  00,00,0d,00,00,00,00,00,00,00,f3,00,00,81,03,20,00,00,f3,00,00,01,00,00,00,\
  00,0d,00,00,81,00,00,60,01,00,00,00,00,0d,00,00,00,00,00,00,81,00,00,60,01,\
  00,00,00,00,0d,00,00,00,00,00,00,03,03,20,00,00,00,0a,00,00,00,00,00,00,00,\
  02,00,81,03,20,00,00,00,02,00,01,00,00,00,00,00,0a,00,81,00,00,60,03,00,00,\
  00,00,00,0a,00,00,00,00,00,81,00,00,60,03,00,00,00,00,00,0a,00,00,00,00,00,\
  03,03,20,00,00,00,00,10,00,00,00,00,00,00,00,ee,81,03,20,00,00,00,00,ee,01,\
  00,00,00,00,00,00,10,81,00,00,60,03,00,00,00,00,00,00,10,00,00,00,00,81,00,\
  00,60,03,00,00,00,00,00,00,10,00,00,00,00

[HKEY_LOCAL_MACHINE\SYSTEM\ControlSet001\Enum\ACPI\PNP0A03\0\Control]
"ActiveService"="PCI"
"FilteredConfigVector"=hex(a):d0,04,00,00,0f,00,00,00,00,00,00,00,00,00,00,00,\
  00,00,00,80,00,00,00,00,6c,06,4e,04,02,00,00,00,01,00,01,00,13,00,00,00,01,\
  80,03,00,00,00,00,00,01,00,00,00,50,00,63,00,69,00,30,32,34,37,35,33,48,02,\
  00,00,00,00,00,00,01,06,03,80,00,00,00,00,00,01,00,00,00,00,00,00,ff,00,00,\
  00,00,00,00,00,00,00,00,00,00,00,00,00,00,81,03,00,00,00,00,00,00,01,00,00,\
  00,00,00,00,ff,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,01,01,03,80,20,\
  00,00,00,f8,0c,00,00,01,00,00,00,00,00,00,00,00,00,00,00,f7,0c,00,00,00,00,\
  00,00,00,81,03,00,20,00,00,00,f8,0c,00,00,01,00,00,00,00,00,00,00,00,00,00,\
  00,f7,0c,00,00,00,00,00,00,00,81,00,00,00,60,00,00,01,00,00,00,00,00,00,00,\
  00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,81,00,00,00,60,00,00,01,\
  00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,01,01,\
  03,80,20,00,00,00,00,f3,00,00,01,00,00,00,00,0d,00,00,00,00,00,00,ff,ff,00,\
  00,00,00,00,00,00,81,03,00,20,00,00,00,00,f3,00,00,01,00,00,00,00,0d,00,00,\
  00,00,00,00,ff,ff,00,00,00,00,00,00,00,81,00,00,00,60,00,00,01,00,00,00,00,\
  0d,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,81,00,00,00,60,\
  00,00,01,00,00,00,00,0d,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,\
  00,01,03,03,80,20,00,00,00,00,00,02,00,01,00,00,00,00,00,0a,00,00,00,00,00,\
  ff,ff,0b,00,00,00,00,00,00,81,03,00,20,00,00,00,00,00,02,00,01,00,00,00,00,\
  00,0a,00,00,00,00,00,ff,ff,0b,00,00,00,00,00,00,81,00,00,00,60,00,00,03,00,\
  00,00,00,00,0a,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,81,00,\
  00,00,60,00,00,03,00,00,00,00,00,0a,00,00,00,00,00,00,00,00,00,00,00,00,00,\
  00,00,00,00,01,03,03,80,20,00,00,00,00,00,00,ee,01,00,00,00,00,00,00,10,00,\
  00,00,00,ff,ff,ff,fd,00,00,00,00,00,81,03,00,20,00,00,00,00,00,00,ee,01,00,\
  00,00,00,00,00,10,00,00,00,00,ff,ff,ff,fd,00,00,00,00,00,81,00,00,00,60,00,\
  00,03,00,00,00,00,00,00,10,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,\
  00,81,00,00,00,60,00,00,03,00,00,00,00,00,00,10,00,00,00,00,00,00,00,00,00,\
  00,00,00,00,00,00,00,01,00,01,00,12,00,00,00,00,06,03,00,00,00,00,00,00,01,\
  00,00,00,00,00,00,ff,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,81,03,\
  00,00,00,00,00,00,01,00,00,00,00,00,00,ff,00,00,00,00,00,00,00,00,00,00,00,\
  00,00,00,00,00,01,03,00,20,00,00,00,f8,0c,00,00,01,00,00,00,00,00,00,00,00,\
  00,00,00,f7,0c,00,00,00,00,00,00,00,81,03,00,20,00,00,00,f8,0c,00,00,01,00,\
  00,00,00,00,00,00,00,00,00,00,f7,0c,00,00,00,00,00,00,00,81,00,00,00,60,00,\
  00,01,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,\
  00,81,00,00,00,60,00,00,01,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,\
  00,00,00,00,00,00,00,00,01,03,00,20,00,00,00,00,f3,00,00,01,00,00,00,00,0d,\
  00,00,00,00,00,00,ff,ff,00,00,00,00,00,00,00,81,03,00,20,00,00,00,00,f3,00,\
  00,01,00,00,00,00,0d,00,00,00,00,00,00,ff,ff,00,00,00,00,00,00,00,81,00,00,\
  00,60,00,00,01,00,00,00,00,0d,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,\
  00,00,00,00,81,00,00,00,60,00,00,01,00,00,00,00,0d,00,00,00,00,00,00,00,00,\
  00,00,00,00,00,00,00,00,00,00,00,03,03,00,20,00,00,00,00,00,02,00,01,00,00,\
  00,00,00,0a,00,00,00,00,00,ff,ff,0b,00,00,00,00,00,00,81,03,00,20,00,00,00,\
  00,00,02,00,01,00,00,00,00,00,0a,00,00,00,00,00,ff,ff,0b,00,00,00,00,00,00,\
  81,00,00,00,60,00,00,03,00,00,00,00,00,0a,00,00,00,00,00,00,00,00,00,00,00,\
  00,00,00,00,00,00,00,81,00,00,00,60,00,00,03,00,00,00,00,00,0a,00,00,00,00,\
  00,00,00,00,00,00,00,00,00,00,00,00,00,00,03,03,00,20,00,00,00,00,00,00,ee,\
  01,00,00,00,00,00,00,10,00,00,00,00,ff,ff,ff,fd,00,00,00,00,00,81,03,00,20,\
  00,00,00,00,00,00,ee,01,00,00,00,00,00,00,10,00,00,00,00,ff,ff,ff,fd,00,00,\
  00,00,00,81,00,00,00,60,00,00,03,00,00,00,00,00,00,10,00,00,00,00,00,00,00,\
  00,00,00,00,00,00,00,00,00,00,81,00,00,00,60,00,00,03,00,00,00,00,00,00,10,\
  00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00
"AllocConfig"=hex(8):01,00,00,00,0f,00,00,00,00,00,00,00,00,00,00,00,12,00,00,\
  00,06,03,00,00,00,00,00,00,00,01,00,00,00,00,00,00,81,01,00,00,00,01,00,00,\
  00,00,00,00,ff,00,00,00,01,03,20,00,00,00,00,00,00,00,00,00,f8,0c,00,00,81,\
  01,20,00,f8,0c,00,00,01,00,00,00,00,00,00,00,81,01,00,60,01,00,00,00,00,00,\
  00,00,00,00,00,00,81,01,00,60,01,00,00,00,00,00,00,00,00,00,00,00,01,03,20,\
  00,00,0d,00,00,00,00,00,00,00,f3,00,00,81,01,20,00,00,f3,00,00,01,00,00,00,\
  00,0d,00,00,81,01,00,60,01,00,00,00,00,0d,00,00,00,00,00,00,81,01,00,60,01,\
  00,00,00,00,0d,00,00,00,00,00,00,03,03,20,00,00,00,0a,00,00,00,00,00,00,00,\
  02,00,81,01,20,00,00,00,02,00,01,00,00,00,00,00,0a,00,81,01,00,60,03,00,00,\
  00,00,00,0a,00,00,00,00,00,81,01,00,60,03,00,00,00,00,00,0a,00,00,00,00,00,\
  03,03,20,00,00,00,00,10,00,00,00,00,00,00,00,ee,81,01,20,00,00,00,00,ee,01,\
  00,00,00,00,00,00,10,81,01,00,60,03,00,00,00,00,00,00,10,00,00,00,00,81,01,\
  00,60,03,00,00,00,00,00,00,10,00,00,00,00

[HKEY_LOCAL_MACHINE\SYSTEM\ControlSet001\Enum\ACPI\PNP0F03]

[HKEY_LOCAL_MACHINE\SYSTEM\ControlSet001\Enum\ACPI\PNP0F03\4&1d401fb5&0]
"Capabilities"=dword:00000020
"HardwareID"=hex(7):41,00,43,00,50,00,49,00,5c,00,50,00,4e,00,50,00,30,00,46,\
  00,30,00,33,00,00,00,2a,00,50,00,4e,00,50,00,30,00,46,00,30,00,33,00,00,00,\
  00,00
"Service"="i8042prt"
"ClassGUID"="{4D36E96F-E325-11CE-BFC1-08002BE10318}"
"ConfigFlags"=dword:00000000
"Driver"="{4D36E96F-E325-11CE-BFC1-08002BE10318}\\0000"
"Class"="Mouse"
"Mfg"="Microsoft"
"DeviceDesc"="Microsoft PS/2 Mouse"

[HKEY_LOCAL_MACHINE\SYSTEM\ControlSet001\Enum\ACPI\PNP0F03\4&1d401fb5&0\Device Parameters]
"FirmwareIdentified"=dword:00000001
"Migrated"=dword:00000001
"EnableWheelDetection"=dword:00000002
"MouseDataQueueSize"=dword:00000064
"MouseResolution"=dword:00000003
"MouseSynchIn100ns"=dword:01312d00
"SampleRate"=dword:00000064
"WheelDetectionTimeout"=dword:000005dc
"MouseInitializePolled"=dword:00000000

[HKEY_LOCAL_MACHINE\SYSTEM\ControlSet001\Enum\ACPI\PNP0F03\4&1d401fb5&0\LogConf]
"BasicConfigVector"=hex(a):48,00,00,00,0f,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,01,00,00,00,01,00,01,00,01,00,00,00,00,02,01,00,01,00,00,00,0c,00,00,00,0c,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00
"BootConfig"=hex(8):01,00,00,00,0f,00,00,00,00,00,00,00,01,00,01,00,01,00,00,00,02,01,01,00,0c,00,00,00,0c,00,00,00,ff,ff,ff,ff

[HKEY_LOCAL_MACHINE\SYSTEM\ControlSet001\Enum\ACPI\PNP0F03\4&1d401fb5&0\Control]
"ActiveService"="Mouclass"
"FilteredConfigVector"=hex(a):68,00,00,00,0f,00,00,00,00,00,00,00,00,00,00,00,02,00,00,00,01,00,00,00,49,6d,61,67,01,00,00,00,01,00,01,00,02,00,00,00,01,80,03,00,00,00,00,00,01,00,00,00,33,00,32,00,5c,00,44,00,52,00,49,00,56,00,45,00,52,00,53,00,01,02,01,80,01,00,00,00,0c,00,00,00,0c,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00
"AllocConfig"=hex(8):01,00,00,00,0f,00,00,00,00,00,00,00,00,00,00,00,01,00,00,00,02,01,01,00,0c,00,00,00,0c,00,00,00,ff,ff,ff,ff

[HKEY_LOCAL_MACHINE\SYSTEM\ControlSet001\Enum\ACPI_HAL]

[HKEY_LOCAL_MACHINE\SYSTEM\ControlSet001\Enum\ACPI_HAL\PNP0C08]

[HKEY_LOCAL_MACHINE\SYSTEM\ControlSet001\Enum\ACPI_HAL\PNP0C08\0]
"Capabilities"=dword:00000030
"HardwareID"=hex(7):41,00,43,00,50,00,49,00,5f,00,48,00,41,00,4c,00,5c,00,50,00,4e,00,50,00,30,00,43,00,30,00,38,00,00,00,2a,00,50,00,4e,00,50,00,30,00,43,00,30,00,38,00,00,00,00,00
"Service"="ACPI"
"ConfigFlags"=dword:00000000
"ParentIdPrefix"="2&daba3ff&0"
"ClassGUID"="{4D36E97D-E325-11CE-BFC1-08002BE10318}"
"Class"="System"
"Driver"="{4D36E97D-E325-11CE-BFC1-08002BE10318}\\0005"
"Mfg"="Microsoft"
"DeviceDesc"="Microsoft ACPI-Compliant System"

[HKEY_LOCAL_MACHINE\SYSTEM\ControlSet001\Enum\ACPI_HAL\PNP0C08\0\LogConf]
"BasicConfigVector"=hex(a):48,00,00,00,0f,00,00,00,ff,ff,ff,ff,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,01,00,00,00,01,00,01,00,01,00,00,00,00,02,03,00,00,00,00,00,09,00,00,00,09,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00
"BootConfig"=hex(8):01,00,00,00,0f,00,00,00,ff,ff,ff,ff,01,00,01,00,01,00,00,00,02,03,00,00,09,00,00,00,09,00,00,00,ff,ff,ff,ff

[HKEY_LOCAL_MACHINE\SYSTEM\ControlSet001\Enum\ACPI_HAL\PNP0C08\0\Control]
"ActiveService"="ACPI"
"FilteredConfigVector"=hex(a):68,00,00,00,0f,00,00,00,ff,ff,ff,ff,00,00,00,00,03,00,00,00,01,00,00,00,42,61,73,65,01,00,00,00,01,00,01,00,02,00,00,00,01,80,03,00,00,00,00,00,01,00,00,00,04,00,00,00,05,00,00,00,06,00,00,00,07,00,00,00,08,00,00,00,01,02,03,80,00,00,00,00,09,00,00,00,09,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00
"AllocConfig"=hex(8):01,00,00,00,0f,00,00,00,ff,ff,ff,ff,00,00,00,00,01,00,00,00,02,03,00,00,09,00,00,00,09,00,00,00,ff,ff,ff,ff

*/
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI_HAL\\PNP0C08\\0\\LogConf", "BasicConfigVector",    "hex(a):48,00,00,00,0f,00,00,00,ff,ff,ff,ff,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,01,00,00,00,01,00,01,00,01,00,00,00,00,02,03,00,00,00,00,00,09,00,00,00,09,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI_HAL\\PNP0C08\\0\\LogConf", "BootConfig",           "hex(8):01,00,00,00,0f,00,00,00,ff,ff,ff,ff,01,00,01,00,01,00,00,00,02,03,00,00,09,00,00,00,09,00,00,00,ff,ff,ff,ff" },

    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI_HAL\\PNP0C08\\0\\Control", "ActiveService",           "\"ACPI\"" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI_HAL\\PNP0C08\\0\\Control", "FilteredConfigVector",    "hex(a):68,00,00,00,0f,00,00,00,ff,ff,ff,ff,00,00,00,00,03,00,00,00,01,00,00,00,42,61,73,65,01,00,00,00,01,00,01,00,02,00,00,00,01,80,03,00,00,00,00,00,01,00,00,00,04,00,00,00,05,00,00,00,06,00,00,00,07,00,00,00,08,00,00,00,01,02,03,80,00,00,00,00,09,00,00,00,09,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI_HAL\\PNP0C08\\0\\Control", "AllocConfig",             "hex(8):01,00,00,00,0f,00,00,00,ff,ff,ff,ff,00,00,00,00,01,00,00,00,02,03,00,00,09,00,00,00,09,00,00,00,ff,ff,ff,ff" },

    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0303\\4&1d401fb5&0", "Capabilities",   "dword:00000020" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0303\\4&1d401fb5&0", "HardwareID",     "hex(7):41,00,43,00,50,00,49,00,5c,00,50,00,4e,00,50,00,30,00,33,00,30,00,33,00,00,00,2a,00,50,00,4e,00,50,00,30,00,33,00,30,00,33,00,00,00,00,00" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0303\\4&1d401fb5&0", "Service",        "\"i8042prt\"" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0303\\4&1d401fb5&0", "ClassGUID",      "\"{4D36E96B-E325-11CE-BFC1-08002BE10318}\"" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0303\\4&1d401fb5&0", "ConfigFlags",    "dword:00000000" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0303\\4&1d401fb5&0", "Class",          "\"Keyboard\"" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0303\\4&1d401fb5&0", "Driver",         "\"{4D36E96B-E325-11CE-BFC1-08002BE10318}\\\\0000\"" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0303\\4&1d401fb5&0", "Mfg",            "\"Standard keyboards\"" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0303\\4&1d401fb5&0", "DeviceDesc",     "\"Standard 101/102-Key or Microsoft Natural PS/2 Keyboard\"" },

    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0303\\4&1d401fb5&0\\Device Parameters", "FirmwareIdentified",      "dword:00000001" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0303\\4&1d401fb5&0\\Device Parameters", "Migrated",                "dword:00000001" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0303\\4&1d401fb5&0\\Device Parameters", "KeyboardDataQueueSize",   "dword:00000064" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0303\\4&1d401fb5&0\\Device Parameters", "PollStatusIterations",    "dword:00000001" },

    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0303\\4&1d401fb5&0\\LogConf", "BasicConfigVector",    "hex(a):88,00,00,00,0f,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,01,00,00,00,01,00,01,00,03,00,00,00,00,01,01,00,11,00,00,00,01,00,00,00,00,00,00,00,60,00,00,00,00,00,00,00,60,00,00,00,00,00,00,00,00,01,01,00,11,00,00,00,01,00,00,00,00,00,00,00,64,00,00,00,00,00,00,00,64,00,00,00,00,00,00,00,00,02,01,00,01,00,00,00,01,00,00,00,01,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum\\ACPI\\PNP0303\\4&1d401fb5&0\\LogConf", "BootConfig",           "hex(8):01,00,00,00,0f,00,00,00,00,00,00,00,01,00,01,00,03,00,00,00,01,01,11,00,60,00,00,00,00,00,00,00,01,00,00,00,01,01,11,00,64,00,00,00,00,00,00,00,01,00,00,00,02,01,01,00,01,00,00,00,01,00,00,00,ff,ff,ff,ff" },

    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum", "NextParentID.daba3ff.2",  "dword:00000001" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum", "NextParentID.267a616a.3", "dword:00000001" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum", "NextParentID.1d401fb5.4", "dword:00000001" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum", "NextParentID.2f42c713.4", "dword:00000001" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum", "NextParentID.24d6eb65.4", "dword:00000001" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum", "NextParentID.6a987e4.4",  "dword:00000001" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum", "NextParentID.30a96598.1", "dword:00000001" },

    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum", "NextParentID.ebb567f.2",  "-" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum", "NextParentID.b8ac1d3.3",  "-" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum", "NextParentID.29fb9706.3", "-" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum", "NextParentID.13cefd14.3", "-" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Enum", "NextParentID.30a96598.1", "-" },

    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Services\\i8042prt", "Start", "dword:00000000" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Services\\i8042prt\\Enum", "0", "\"ACPI\\\\PNP0303\\\\4&1d401fb5&0\"" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Services\\i8042prt\\Enum", "1", "\"ACPI\\\\PNP0F03\\\\4&1d401fb5&0\"" },

    { "-HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Control\\ApmLegalHal", NULL, NULL },
    { "-HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Control\\Biosinfo\\PNPBios", NULL, NULL },

    { "HKEY_LOCAL_MACHINE\\SYSTEM\\Select", "Current",   "dword:00000001" },
    { "HKEY_LOCAL_MACHINE\\SYSTEM\\Select", "Default",   "dword:00000001" },

    HAL_REG_ENTRY_END_OF_LIST
};

static const winxp_HALRegistryEntry winxp_HALRegistryEntries_APM[] = {
    HAL_REG_ENTRY_END_OF_LIST
}; // TODO

static winxp_HALRegistryEntry winxp_HALRegistryEntries_runtimeData[] = {
    { WINXP_REG_HAL_CLASS_CONTROL, "DriverDesc",           NULL },
    { WINXP_REG_HAL_CLASS_CONTROL, "InfSection",           NULL },
    { WINXP_REG_HAL_CLASS_CONTROL, "MatchingDeviceId",     NULL },
    HAL_REG_ENTRY_END_OF_LIST
};

static const winxp_HAL winxp_supportedHALS[] = {
    { "\"Standard PC\"",                                            "\"E_ISA_UP_HAL\"",     "\"e_isa_up\"",     "hal.dll",      "ntoskrnl.exe", "ntkrnlpa.exe", NULL },
    { "\"Advanced Configuration and Power Interface (ACPI) PC\"",   "\"ACPIPIC_UP_HAL\"",   "\"acpipic_up\"",   "halacpi.dll",  "ntoskrnl.exe", "ntkrnlpa.exe", NULL },
    { "\"ACPI Uniprocessor PC\"",                                   "\"ACPIAPIC_UP_HAL\"",  "\"acpiapic_up\"",  "halaacpi.dll", "ntoskrnl.exe", "ntkrnlpa.exe", winxp_HALRegistryEntries_ACPI },
    { "\"ACPI Multiprocessor PC\"",                                 "\"ACPIAPIC_MP_HAL\"",  "\"acpiapic_mp\"",  "halmacpi.dll", "ntkrnlmp.exe", "ntkrpamp.exe", winxp_HALRegistryEntries_ACPI },
    { "\"MPS Uniprocessor PC\"",                                    "\"MPS_UP_HAL\"",       "\"mps_up\"",       "halapic.dll",  "ntoskrnl.exe", "ntkrnlpa.exe", winxp_HALRegistryEntries_APM },
    { "\"MPS Multiprocessor PC\"",                                  "\"MPS_MP_HAL\"",       "\"mps_mp\"",       "halmps.dll",   "ntkrnlmp.exe", "ntkrpamp.exe", winxp_HALRegistryEntries_APM },
    { "\"Compaq SystemPro Multiprocessor or 100%% Compatible\"",    "\"SYSPRO_MP_HAL\"",    "\"syspro_mp\"",    "halsp.dll",    "ntkrnlmp.exe", "ntkrpamp.exe", NULL },
};

static bool winxp_processHKLMSystemList(const char *mountPath, const winxp_HALRegistryEntry *reg) {
    FILE *tmpReg = fopen("/tmp/tmp.reg", "w");

    char tmpCmd[1024] = { 0, };

    QI_ASSERT(tmpReg != NULL);

    fprintf(tmpReg, WINXP_REG_HEADER "\n\n");

    while(reg->key != NULL){
        fprintf(tmpReg, "[%s]\n", reg->key);

        if (reg->valueData != NULL) {
            fprintf(tmpReg, "\"%s\"=%s\n", reg->valueName, reg->valueData);
        }

        fprintf(tmpReg, "\n");

        reg++;
    }

    fclose(tmpReg);

    snprintf(tmpCmd, sizeof(tmpCmd), "reged.static -I %s" WINXP_SYSTEM_HIVE " " WINXP_SYSTEM_HIVE_PREFIX " /tmp/tmp.reg -C 2>&1", mountPath);

    int result = ad_runCommandBox("Updating HAL registry...", tmpCmd);

    ad_okBox("QI Debug", false, "DEBUG: Registry result: %d", result);

    return result == 0 || result == 2; // Not exactly sure why 2 but that's how it is...
}

/*
static bool winxp_regSetString(const inst_Context *inst, const char *key, const char *subKey, const char *value) {
    char tmpVal[1024] = { 0, };
    snprintf(tmpVal, sizeof(tmpVal), "\"%s\"", value);
    return winxp_regSetRaw(inst, key, subKey, tmpVal);
}
static bool winxp_regSetDWORD(const inst_Context *inst, const char *key, const char *subKey, uint32_t value) {
    char tmpVal[1024] = { 0, };
    snprintf(tmpVal, sizeof(tmpVal), "dword:%x", value);
    return winxp_regSetRaw(inst, key, subKey, tmpVal);
}
static bool winxp_doesQIFileExist(const inst_Context *inst, const char *file) {
    char tmpPath[1024] = { 0, };
    snprintf(tmpPath, sizeof(tmpPath), "%s" WINXP_QI_DLL_SEARCH_PATH "%s", inst->destinationPartition->mountPath, file);
    return util_fileExists(tmpPath);
}
*/

static inline void winxp_displayHALRegistryUpdateError() {
    ad_okBox("Windows XP HAL Update", false,
       "Error updating the registry for the new HAL!\n"
        "\n"
        "The default HAL will remain in place!");
}

static inline void winxp_displayHALFileCopyError() {
    ad_okBox("Windows XP HAL Update", false,
       "Error updating the registry for the new HAL!\n"
        "\n"
        "The default HAL will remain in place!");
}

static bool winxp_updateHAL(const inst_Context *inst, const winxp_HAL *hal) {
//    const char halKey[] = "HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Control\\Class\\{4D36E966-E325-11CE-BFC1-08002BE10318}";
    char tmpDst[1024] = { 0, };
    bool regSuccess = true;
    bool copySuccess = true;

    winxp_HALRegistryEntries_runtimeData[0].valueData = hal->halName;
    winxp_HALRegistryEntries_runtimeData[1].valueData = hal->halInfSection;
    winxp_HALRegistryEntries_runtimeData[2].valueData = hal->halDeviceId;

    regSuccess &= winxp_processHKLMSystemList(inst->destinationPartition->mountPath, winxp_HALRegistryEntries_runtimeData);

    if (hal->regEntries) {
        regSuccess &= winxp_processHKLMSystemList(inst->destinationPartition->mountPath, hal->regEntries);
    }

    if (!regSuccess) {
        winxp_displayHALRegistryUpdateError(hal);
    }

    ad_setFooterText("Copying HAL system files...");

    snprintf(tmpDst, sizeof(tmpDst), "%s/windows/system32/hal.dll", inst->destinationPartition->mountPath);
    copySuccess &= util_copyFile(inst_getCDFilePath(inst->osVariantIndex, hal->haldllFile), tmpDst);

    snprintf(tmpDst, sizeof(tmpDst), "%s/windows/system32/ntoskrnl.exe", inst->destinationPartition->mountPath);
    copySuccess &= util_copyFile(inst_getCDFilePath(inst->osVariantIndex, hal->ntoskrnlFile), tmpDst);

    snprintf(tmpDst, sizeof(tmpDst), "%s/windows/system32/ntkrnlpa.exe", inst->destinationPartition->mountPath);
    copySuccess &= util_copyFile(inst_getCDFilePath(inst->osVariantIndex, hal->ntkrnlpaFile), tmpDst);

    ad_clearFooter();

    if (!copySuccess) {
        winxp_displayHALFileCopyError(hal);
    }

    return regSuccess && copySuccess;
}

static inline bool winxp_isValidHAL(const inst_Context *inst, const winxp_HAL *hal) {
    return util_fileExists(inst_getCDFilePath(inst->osVariantIndex, hal->haldllFile))
        && util_fileExists(inst_getCDFilePath(inst->osVariantIndex, hal->ntoskrnlFile))
        && util_fileExists(inst_getCDFilePath(inst->osVariantIndex, hal->ntkrnlpaFile));
}

/* Asks user which system HAL he wants */
static const winxp_HAL *winxp_askUserForHAL(inst_Context *inst) {
    // array with valid HALs, maximum as many as we know, we will only populate as many as we can find the files for
    const winxp_HAL *validHALs[util_arraySize(winxp_supportedHALS)];
    size_t HALcount = 0;
    ad_Menu *menu = ad_menuCreate("Windows XP HAL DLL selection", 
        "Please select the HAL you find most likely to work on your machine.\n"
        "(If you are unsure, use Standard PC or ACPI PC. Use a 'multiprocessor' variant\n"
        "for HT/MT or multi-core CPUs)", false);

    int32_t selectedHAL = 0;

    QI_ASSERT(menu != NULL);

    for (size_t i = 0; i < util_arraySize(winxp_supportedHALS); i++) {
        if (winxp_isValidHAL(inst, &winxp_supportedHALS[i])) {
            ad_menuAddItemFormatted(menu, winxp_supportedHALS[i].halName);
            validHALs[HALcount++] = &winxp_supportedHALS[i];
        }
    }

    if (HALcount == 0) {
        return NULL;
    }

    selectedHAL = ad_menuExecute(menu);

    ad_menuDestroy(menu);

    if (selectedHAL < 0) {
        return NULL;
    }

    return validHALs[selectedHAL];
}

bool winxp_checkPartition(inst_Context *inst) {
    QI_ASSERT(inst != NULL);

    if (inst->destinationPartition->fileSystem == fs_fat32) {
        return true;
    } else {
        return false;
    }
}

bool winxp_prepareInstall(inst_Context *inst) {
    QI_ASSERT(inst != NULL);

    s_systemHAL = winxp_askUserForHAL(inst);

    return true;
}

inst_InstallStatus winxp_doInstall(inst_Context *inst) {
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

    // If driver data copy was successful, update system HAL

    if (s_systemHAL == NULL) {
        return INST_OK;
    }

    installSuccess = winxp_updateHAL(inst, s_systemHAL);

    if (!installSuccess) {
        return INST_FILE_COPY_ERROR;
    }

    return INST_OK;
}

#include "mbr_boot_winxp.h"

const inst_OSInstaller inst_winxpInstaller = {
    winxp_checkPartition,
    winxp_prepareInstall,
    winxp_doInstall,
    NULL,
    &__WINXP_FAT32_BOOT_SECTOR_MODIFIER_LIST__
};
