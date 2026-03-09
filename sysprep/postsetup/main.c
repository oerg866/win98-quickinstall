/*  QISETUP:
    This is the POST-INSTALL processor code for QuickInstall.
    It is run at the end of the HW detection phase.
    It does the final setup + reboot and replaces the manual reboot.

    Currently it does the following:

    * Randomizes computer name to avoid network conflicts with multiple QI-based machines
    * Removes some "Run" registry keys to remove initial desktop lag
    * Performs the final reboot.

    The code is fleshed out a bit to enable further additions.

    This source file is provided for reference alongside a binary.
    It's not part of the build.sh stage of the whole thing to avoid installing another toolchain...

    Compile with OpenWatcom in Win32 mode.
    wcl386 main.c -bt=nt -l=nt_win -4 -q -fe=qisetup

    (C)2026 E. Voirin (oerg866)
*/

#include <windows.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <strings.h>

HANDLE __logFile = NULL;

#ifndef __func__
// stfu intellisense :)
#define __func__
#endif

#define LOGGING
#define LOGGING_FILE "C:\\QI_LOG.TXT"
//#define LOGGING_FILE "QI_LOG.TXT"

unsigned char __BLANK_PWL[] = {
  0xe3, 0x82, 0x85, 0x96, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0x52, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x93, 0x17, 0x10, 0xcb, 0xf4, 0x52,
  0x95, 0x6d, 0x45, 0x42, 0xa9, 0x1a, 0xeb, 0x90, 0x65, 0x0c, 0xca, 0xc5,
  0xca, 0xb0, 0x61, 0xa9, 0xf2, 0x23, 0x14, 0xba, 0x7f, 0x97, 0xa1, 0x55,
  0xb1, 0xab, 0x7f, 0x57, 0x63, 0x8d, 0xf2, 0x7b, 0x0c, 0xe0, 0x6d, 0xed,
  0x4c, 0x2a, 0x64, 0x53, 0x2f, 0x6c, 0x18, 0x4f, 0x01, 0xa4, 0x0d, 0xfc,
  0x62, 0x4f, 0x6d, 0xe5, 0xed, 0xe4, 0xec, 0xff, 0x7f, 0xd4, 0xe0, 0xe3,
  0x5d, 0xee, 0x05, 0xc8, 0x69, 0xd4, 0xfe, 0xd5, 0x6e, 0x64, 0x54, 0x35,
  0x6c, 0x3c, 0x5f, 0x21, 0xff, 0xd6, 0x1a, 0x42, 0xba, 0x6c, 0xb8, 0x30,
  0xc3, 0x9c, 0x99, 0x56
};

const char *winError(DWORD err)
{
    static char buf[256];
    size_t i;

    FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, err, 0, buf, sizeof(buf), NULL
    );

    for (i = 0; i < sizeof(buf) && buf[i] != 0x00; i++) {
        if (buf[i] == '\r' || buf[i] == '\n') buf[i] = 0x00;
    }
    
    return buf;
}

static int __ind = 0; // Logging indent

void log(const char *fmt, ...) {
#ifdef LOGGING
#define INDENT_SIZE (4)
#define INDENT (__ind * INDENT_SIZE)
    char buf[512];
    char *msgStart = &buf[1+10+1+1+INDENT]; // "[1234567890] <msg>\r\n"
    size_t maxMsgLen = 512 - (1+10+1+1) - 2 - 1 - INDENT; // -2 (\r\n) and -1 (null term) and -indentation
    int msgLen;
    DWORD written;

    va_list argptr;
    va_start(argptr, fmt);
    sprintf(buf, "[%10lu] %*s", (unsigned long) GetTickCount(), INDENT, "");
    msgLen = vsnprintf(msgStart, maxMsgLen, fmt, argptr);
    msgStart[msgLen + 0] = '\r';
    msgStart[msgLen + 1] = '\n';
    msgStart[msgLen + 2] = 0;
    va_end(argptr);
    WriteFile(__logFile, buf, strlen(buf), &written, NULL);
#endif
}

#define logEnter()                  do { log("+" __func__); __ind++; } while (0)
#define logEnter1(fmt, p1)          do { log("+" __func__ " (" fmt ")", p1); __ind++; } while (0)
#define logEnter2(fmt, p1, p2)      do { log("+" __func__ " (" fmt ")", p1, p2); __ind++; } while (0)
#define logEnter3(fmt, p1, p2, p3)  do { log("+" __func__ " (" fmt ")", p1, p2, p3); __ind++; } while (0)
#define logExit(msg, val)           do { __ind--; log("-" __func__ " (" msg ")", val); } while (0)
#define logReturn(ret, msg, val)    do { logExit(msg, val); return ret; } while (0);
#define boolStr(b)                  ((b) ? "true" : "false")

#define closeKeyLogAndReturnIf(condition, ret, key, msg, val) do { \
        if (condition) { RegCloseKey(key); logReturn(ret, msg, val); } \
    } while (0)

#define logAndReturnIf(condition, ret, msg, val) do { \
        if (condition) { logReturn(ret, msg, val); } \
    } while (0)

typedef bool (*KeyEnumerateFunc)(HKEY base, const char *key);

static bool regDoForEachSubkey(HKEY base, const char *key, bool returnAfterTrue, KeyEnumerateFunc func) {
    DWORD ret;
    DWORD index = 0;
    HKEY hKey;

    logEnter2("%s - returnAfterTrue: %s", key, returnAfterTrue ? "yes" : "no");

    ret = RegOpenKeyExA(base, key, 0, KEY_ENUMERATE_SUB_KEYS, &hKey);
    closeKeyLogAndReturnIf(ret != ERROR_SUCCESS, false, hKey, "Can't open key (%s)", winError(ret));

    while (1)
    {
        char subkey[256];
        DWORD subkeySize = sizeof(subkey);
        bool success = true;

        ret = RegEnumKeyExA(hKey, index, subkey, &subkeySize, NULL, NULL, NULL, NULL);

        closeKeyLogAndReturnIf(ret == ERROR_NO_MORE_ITEMS,  true,   hKey, "Successfully processed %u subkeys", index);
        closeKeyLogAndReturnIf(ret != ERROR_SUCCESS,        false,  hKey, "Failed to Enumerate key (%s)", winError(ret));
        log("Processing subkey %u: '%s'", index, subkey);

        index++;
        
        success = func(hKey, subkey);

        // True = "subkey was handled". Return after true means return after one successful handling.
        if (success && returnAfterTrue) break;

    }

    RegCloseKey(hKey);
    logReturn(true, "Processed %lu subkeys", index);
}

// Returns <key>\<subKey> - don't call this again until after you're done using the return of this one!
static const char *regSubKey(const char *key, const char *subKey) {
    static char skBuf[512] = "";
    snprintf(skBuf, sizeof(skBuf), "%s\\%s", key, subKey);
    return skBuf;
}

static bool regKeyExists(HKEY base, const char *key) {
    HKEY hKey;
    DWORD ret = RegOpenKeyExA(base, key, 0, KEY_READ, &hKey);
    logEnter1("%s", key);
    closeKeyLogAndReturnIf(ret != ERROR_SUCCESS, false, hKey, "-> no", 0);
    logReturn(true, "-> yes", 0);
}

static bool createRegKey(HKEY base, const char *key) {
    HKEY hKey;
    DWORD ret;

    logEnter1("%s", key);
    ret = RegCreateKeyExA(base, key, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS,NULL, &hKey, NULL);
    closeKeyLogAndReturnIf(ret != ERROR_SUCCESS, false, hKey, "Can't create key (%s)", winError(ret));
    RegCloseKey(hKey);
    logReturn(true, "Created key '%s'", key);
}

static bool delRegKey(HKEY base, const char *key) {
    HKEY hKey;
    DWORD ret;

    logEnter1("%s", key);
    
    ret = RegDeleteKeyA(base, key);
    closeKeyLogAndReturnIf(ret != ERROR_SUCCESS && ret != ERROR_FILE_NOT_FOUND, false, hKey, "Can't delete key (%s)", winError(ret));

    RegCloseKey(hKey);
    logReturn(true, "Deleted key '%s'", key);
}

static bool getRegString(HKEY base, const char *key, const char *valueName, char *targetBuf, size_t bufSize) {
    HKEY hKey;
    DWORD ret;
    DWORD keyType;
    DWORD keySize = bufSize - 1;
    
    logEnter2("%s\\%s", key, valueName);

    ret = RegOpenKeyExA(base, key, 0, KEY_READ, &hKey);
    closeKeyLogAndReturnIf(ret != ERROR_SUCCESS, false, hKey, "Can't open key (%s)", winError(ret));

    ret = RegQueryValueExA(hKey, valueName, NULL, &keyType, (BYTE*)targetBuf, &keySize);
    closeKeyLogAndReturnIf(ret != ERROR_SUCCESS, false,  hKey, "Can't query value (%lu)", ret);
    closeKeyLogAndReturnIf(keyType != REG_SZ, false,  hKey, "Unexpected key type: %lu", keyType);

    targetBuf[bufSize - 1] = 0x00;

    RegCloseKey(hKey);
    logReturn(true, "%s", targetBuf);
}

static bool setRegString(HKEY base, const char *key, const char *valueName, const char *value) {
    HKEY hKey;
    DWORD ret;

    logEnter3("%s\\%s = %s", key, valueName, value);
    
    ret = RegOpenKeyExA(base, key, 0, KEY_WRITE, &hKey);
    closeKeyLogAndReturnIf(ret != ERROR_SUCCESS, false, hKey, "Can't open key (%s)", winError(ret));
    
    ret = RegSetValueExA(hKey, valueName, 0, REG_SZ, (BYTE*)value, strlen(value) + 1);
    closeKeyLogAndReturnIf(ret != ERROR_SUCCESS, false,  hKey, "Can't set value (%lu)", ret);

    logReturn(true, "OK", 0);
}

static bool setRegBinary(HKEY base, const char *key, const char *valueName, const BYTE *value, size_t valueSize) {
    HKEY hKey;
    DWORD ret;

    logEnter3("%s\\%s (%lu bytes)", key, valueName, valueSize);
    
    ret = RegOpenKeyExA(base, key, 0, KEY_WRITE, &hKey);
    closeKeyLogAndReturnIf(ret != ERROR_SUCCESS, false, hKey, "Can't open key (%s)", winError(ret));
    
    ret = RegSetValueExA(hKey, valueName, 0, REG_BINARY, value, valueSize);
    closeKeyLogAndReturnIf(ret != ERROR_SUCCESS, false,  hKey, "Can't set value (%lu)", ret);

    logReturn(true, "OK", 0);
}

static bool delRegValue(HKEY base, const char *key, const char *valueName) {
    HKEY hKey;
    DWORD ret;

    logEnter2("%s\\%s", key, valueName);

    ret = RegOpenKeyExA(base, key, 0, KEY_WRITE, &hKey);
    closeKeyLogAndReturnIf(ret != ERROR_SUCCESS, false, hKey, "Can't open key (%s)", winError(ret));

    ret = RegDeleteValueA(hKey, valueName);
    closeKeyLogAndReturnIf(ret != ERROR_SUCCESS && ret != ERROR_FILE_NOT_FOUND, false, hKey, "Can't delete value (%s)", winError(ret));

    logReturn(true, "OK", 0);
}

static void setComputerName() {
    const char nameKey1[] = "System\\CurrentControlSet\\Control\\ComputerName\\ComputerName";
    const char nameKey2[] = "System\\CurrentControlSet\\Services\\VxD\\VNETSUP";
    const char nameValueName[] = "ComputerName";
    char oldName[512];
    char newName[8+1] = "W9QIXXXX";
    unsigned short randomId;

    logEnter();

    srand(GetTickCount());

    if (!getRegString(HKEY_LOCAL_MACHINE, nameKey1, nameValueName, oldName, sizeof(oldName))) {
        logExit("Failed to read computer name", 0);
        return;
    }

    log("Current computer name is: '%s'", oldName);
    randomId = (unsigned short) rand();
    sprintf(newName, "W9QI%04X", randomId);
    log("New computer name: '%s'", newName);

    if (!setRegString(HKEY_LOCAL_MACHINE, nameKey1, nameValueName, newName)) {
        logExit("Failed to set key: %s", nameKey1);
        return;
    }
    
    if (!setRegString(HKEY_LOCAL_MACHINE, nameKey2, nameValueName, newName)) {
        logExit("Failed to set key: %s", nameKey2);
        return;
    }
    
    logExit("OK", 0);
}

static bool fileExists(const char *root, const char *filename) {
    char concat[MAX_PATH*2+1];
    DWORD attrib;
    bool ret;
    logEnter2("%s\\%s", root, filename);
    snprintf(concat, sizeof(concat), "%s\\%s", root, filename);
    log("%s", concat);
    attrib = GetFileAttributesA(concat);
    ret = (attrib != INVALID_FILE_ATTRIBUTES) && (0 == (attrib & FILE_ATTRIBUTE_DIRECTORY));
    logReturn(ret, "%s", ret ? "exists" : "missing");
}

static bool is98Lite() {
    char sysDir[MAX_PATH];
    bool ret;
    logEnter();
    GetSystemDirectoryA(sysDir, MAX_PATH);
    log("System directory: '%s'", sysDir);
    ret = fileExists(sysDir, "shell32.w98") || fileExists(sysDir, "shell32.wme");
    logReturn(ret, "System type: %s", ret ? "98Lite" : "stock");
}

static void cleanupRegistry() {
    const char runKey[] = "Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    const char runServicesKey[] = "Software\\Microsoft\\Windows\\CurrentVersion\\RunServices";
    const char pnpEnum050C[] = "Enum\\Root\\*PNP0C05\\0000";
    const BYTE pnpEnum050CValue[] = { 0x00 };
    bool ret = true;

    logEnter();

    if (is98Lite()) {
        ret &= delRegValue(HKEY_LOCAL_MACHINE,  runKey,         "LoadPowerProfile");
        ret &= delRegValue(HKEY_LOCAL_MACHINE,  runKey,         "TaskMonitor");
        ret &= delRegValue(HKEY_LOCAL_MACHINE,  runServicesKey, "LoadPowerProfile");
        ret &= setRegBinary(HKEY_LOCAL_MACHINE, pnpEnum050C,    "APMMenuSuspend", pnpEnum050CValue, sizeof(pnpEnum050CValue));
    }

    logExit("%s", boolStr(ret));
}

static bool ifVredirUnsetDefault(HKEY base, const char *subkey) {
    char tmp[128];
    bool ret = true;

    logEnter1("%s", subkey);

    // If DeviceVXDs value isn't found, we skip it
    logAndReturnIf(false == getRegString(base, subkey, "DeviceVxDs", tmp, sizeof(tmp)), false, "Not VREDIR, skipping...", 0);
    // If this isn't vredir, we skip it too
    logAndReturnIf(0 != strcasecmp(tmp, "VREDIR.VXD"), false, "Not VREDIR, skipping...", 0);
    // Try to delete the Default key's VALUE
    logAndReturnIf(false == delRegValue(base, regSubKey(subkey, "Ndi\\Default"), ""), false, "Failed to delete 'Default' value", 0);
    // Try to delete the Default key altogether
    logAndReturnIf(false == delRegKey(base, regSubKey(subkey, "Ndi\\Default")), false, "Failed to delete 'Default' key", 0);

    logReturn(true, "Handled VREDIR!", 0);
}

static bool setNetworkConfigFlags(HKEY base, const char *subkey, DWORD flags) {
    BYTE tmp[4] = { 0, 0, 0, 0 };
    memcpy(tmp, &flags, sizeof(tmp));

    logEnter2("%s = %08lx", subkey, flags);

    log("Setting ConfigFlags of '%s' to %08lx", subkey, flags);

    logAndReturnIf(false == setRegBinary(base, subkey, "ConfigFlags", tmp, sizeof(tmp)), false, "Failed to set ConfigFlags", 0);

    logReturn(true, "Set config flags", 0);
}

static bool networkConfigFlagsDisable(HKEY base, const char *subkey) {
    return setNetworkConfigFlags(base, subkey, 0x00000000);
}

static bool networkConfigFlagsEnable(HKEY base, const char *subkey) {
    return setNetworkConfigFlags(base, subkey, 0x00000010);
}

static bool stringStartsWith(const char *s1, const char *s2) {
    return strncasecmp(s2, s1, strlen(s2)) == 0;
}

static bool assignPwlFileInSystemIni(const char *uppercaseName, const char *pwlFileName) {
    char sini[512];
    char sbak[512];
    char line[1024];
    FILE *in = NULL;
    FILE *out = NULL;
    DWORD ret;
    bool success = true;
    bool inPasswordListsSection;

    logEnter2("User '%s' PWL File '%s'", uppercaseName, pwlFileName);
  
    sprintf(sini, "%s\\%s", getenv("WINDIR"), "SYSTEM.INI");
    sprintf(sbak, "%s\\%s", getenv("WINDIR"), "SYSTEM.BAK");

    logAndReturnIf(MoveFileA(sini, sbak) != true, false, "Failed to backup SYSTEM.INI", 0);

    in = fopen(sbak, "r");
    out = fopen(sini, "w");

    if (in == NULL || out == NULL) {
        fclose(in);
        fclose(out);
        logReturn(false, "File open error", 0);
    }
    
    // Write everything in system.ini BEFORE password lists section
    while (fgets(line, sizeof(line), in) != NULL) {
        success &= 0 < fwrite(line, 1, strlen(line), out);
        if (stringStartsWith(line, "[Password Lists]")) {
            inPasswordListsSection = true;
            break;
        }
    }

    if (inPasswordListsSection) {
        // Write all lines in the password lists section
        while (fgets(line, sizeof(line), in) != NULL) {
            if (stringStartsWith(line, uppercaseName)) {
                // This name is already handled, override the PWL file
                success &= 0 < fprintf(out, "%s=%s\n", uppercaseName, pwlFileName);
            } else {
                // Some other bloke, we don't care
                success &= 0 < fwrite(line, 1, strlen(line), out);
            }
        }
    } else {
        // We're not in the pasword lists section so we have to create/fill it by hand
        success &= 0 < fprintf(out, "\n[Password Lists]\n");
        success &= 0 < fprintf(out, "%s=%s\n", uppercaseName, pwlFileName);
    }

    // Write the rest of the SYSTEM.INI lines
    while (fgets(line, sizeof(line), in) != NULL) {
        success &= 0 < fwrite(line, 1, strlen(line), out);
    }

    fprintf(out, "\n");
    fclose(in);
    fclose(out);

    logReturn(false, "%s", success ? "OK" : "Error");
}

static bool setDefaultUser(const char *user) {
    char pwlName[32];
    char userUpper[32];
    char buf[512];
    size_t written;
    FILE *f = NULL;
    logEnter1("User '%s'", user);

    logAndReturnIf(strlen(user) > 28, false, "Name is too long", 0);

    // Create blank PWL file
    if (strlen(user) > 8) {
        memcpy(pwlName, user, 8);
        sprintf(&pwlName[8], ".PWL");
    } else {
        strcpy(pwlName, user);
        strcat(pwlName, ".PWL");
    }

    sprintf(buf, "%s\\%s", getenv("WINDIR"), pwlName);
    log("Creating blank pwl file '%s'", buf);

    f = fopen(buf, "wb");
    written = fwrite(__BLANK_PWL, 1, sizeof(__BLANK_PWL), f);
    fclose(f);

    logAndReturnIf(written == 0, false, "Error writing PWL file", 0);

    // create uppercase version.
    strcpy(userUpper, user);
    CharUpperBuffA(userUpper, strlen(userUpper));

    // Try finding
    assignPwlFileInSystemIni(userUpper, buf);
    
    logReturn(true, "wrote %lu bytes", (unsigned long) written);
}

// Set Logon provider to Windows Logon so that the login happens automagically
void setWindowsLogon() {
    bool ret = true;
    static const char realModeNet[] = "Software\\Microsoft\\Windows\\CurrentVersion\\Network\\Real Mode Net";
    static const char netClient[] = "System\\CurrentControlSet\\Services\\Class\\NetClient";
    static const char enumNetworkFamily[] = "Enum\\Network\\FAMILY";
    static const char enumNetworkVredir[] = "Enum\\Network\\VREDIR";
    static const char networkLogon[] = "Network\\Logon";
    static const char securityProvider[] = "Security\\Provider";
    static const char control[] = "System\\CurrentcontrolSet\\Control";
    static const char winLogon[] = "Software\\Microsoft\\Windows\\CurrentVersion\\Winlogon";
  
    char currentUser[32] = "";

    BYTE zero[4] = { 0, 0, 0, 0 };

    logEnter();

    // Set preferred redirector for real mode net from VREDIR to... well... nothing
    ret &= setRegString(HKEY_LOCAL_MACHINE, realModeNet, "preferredredir", "");

    // In netClient section we need to find the one that is VREDIR.VXD and then set it to DEFAULT.
    ret &= regDoForEachSubkey(HKEY_LOCAL_MACHINE, netClient, true, ifVredirUnsetDefault);

    if (regKeyExists(HKEY_LOCAL_MACHINE, enumNetworkFamily)) {
        // Set ConfigFlags to Enable for Microsoft Family Logon
        ret &= regDoForEachSubkey(HKEY_LOCAL_MACHINE, enumNetworkFamily, false, networkConfigFlagsEnable);

        // Set ConfigFlags to Disable for Client for Microsoft Networks
        ret &= regDoForEachSubkey(HKEY_LOCAL_MACHINE, enumNetworkVredir, false, networkConfigFlagsDisable);
    } else {
        // Set ConfigFlags to Enable for Client for Microsoft Networks
        ret &= regDoForEachSubkey(HKEY_LOCAL_MACHINE, enumNetworkVredir, false, networkConfigFlagsEnable);
    }

    // Set primary login provider
    ret &= setRegString(HKEY_LOCAL_MACHINE, networkLogon, "PrimaryProvider", "Windows Logon");

    // Delete security provider platform type (dunno why)
    ret &= createRegKey(HKEY_LOCAL_MACHINE, securityProvider);
    ret &= setRegBinary(HKEY_LOCAL_MACHINE, securityProvider, "Platform_Type", zero, sizeof(zero));

    // Set auto admin logon. First get current user
    if (!getRegString(HKEY_LOCAL_MACHINE, control, "Current User", currentUser, sizeof(currentUser))) {
        ret &= getRegString(HKEY_LOCAL_MACHINE, networkLogon, "username", currentUser, sizeof(currentUser));
        ret &= setRegString(HKEY_LOCAL_MACHINE, control, "Current User", currentUser);
    }

    ret &= createRegKey(HKEY_LOCAL_MACHINE, winLogon);
    ret &= setRegString(HKEY_LOCAL_MACHINE, winLogon, "AutoAdminLogon", "1");
    ret &= setRegString(HKEY_LOCAL_MACHINE, winLogon, "DefaultUserName", currentUser);
    ret &= setRegString(HKEY_LOCAL_MACHINE, winLogon, "DefaultPassword", "");

    ret &= setDefaultUser(currentUser);

    logExit("%s", boolStr(ret));
}

void doReboot() {
    bool ret;
    logEnter();
    ret = ExitWindowsEx(EWX_REBOOT, 0);
    logExit("ExitWindowsEx: %u", (unsigned) ret);
}

int PASCAL WinMain( HINSTANCE currinst, HINSTANCE previnst, LPSTR cmdline, int cmdshow ) {
#ifdef LOGGING
    __logFile = CreateFileA(LOGGING_FILE, GENERIC_READ|GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
#endif

    log("Windows 9x QuickInstall Post-Install processor");
    log("----------------------------------------------");

    setComputerName();
    setWindowsLogon();
    cleanupRegistry();
    doReboot();

#ifdef LOGGING
    CloseHandle(__logFile);
#endif

    return( 0 );
}
