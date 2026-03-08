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

HANDLE __logFile = NULL;

#ifndef __func__
// stfu intellisense :)
#define __func__
#endif

#define LOGGING
//#define LOGGING_FILE "C:\\QI_LOG.TXT"
#define LOGGING_FILE "QI_LOG.TXT"


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
#define logExit(msg, val)           do { __ind--; log("-" __func__ " (" msg ")", val); } while (0)
#define logReturn(ret, msg, val)    do { logExit(msg, val); return ret; } while (0);
#define boolStr(b)                  ((b) ? "true" : "false")

#define closeKeyLogAndReturnIf(condition, ret, key, msg, val) do { \
        if (condition) { RegCloseKey(key); logReturn(ret, msg, val); } \
    } while (0)

static bool getRegString(HKEY base, const char *key, const char *valueName, char *targetBuf, size_t bufSize) {
    HKEY hKey;
    DWORD ret;
    DWORD keyType;
    DWORD keySize = bufSize - 1;
    
    logEnter();

    ret = RegOpenKeyExA(base, key, 0, KEY_READ, &hKey);
    closeKeyLogAndReturnIf(ret != ERROR_SUCCESS, false, hKey, "Can't open key (%s)", winError(ret));

    ret = RegQueryValueExA(hKey, valueName, NULL, &keyType, (BYTE*)targetBuf, &keySize);
    closeKeyLogAndReturnIf(ret != ERROR_SUCCESS, false,  hKey, "Can't query value (%lu)", ret);
    closeKeyLogAndReturnIf(keyType != REG_SZ, false,  hKey, "Unexpected key type: %lu", keyType);

    targetBuf[bufSize - 1] = 0x00;

    RegCloseKey(hKey);
    logReturn(true, "OK", 0);
}

static bool setRegString(HKEY base, const char *key, const char *valueName, const char *value) {
    HKEY hKey;
    DWORD ret;

    logEnter();
    
    ret = RegOpenKeyExA(base, key, 0, KEY_WRITE, &hKey);
    closeKeyLogAndReturnIf(ret != ERROR_SUCCESS, false, hKey, "Can't open key (%s)", winError(ret));
    
    ret = RegSetValueExA(hKey, valueName, 0, REG_SZ, (BYTE*)value, strlen(value) + 1);
    closeKeyLogAndReturnIf(ret != ERROR_SUCCESS, false,  hKey, "Can't set value (%lu)", ret);

    logReturn(true, "OK", 0);
}

static bool setRegBinary(HKEY base, const char *key, const char *valueName, const BYTE *value, size_t valueSize) {
    HKEY hKey;
    DWORD ret;

    logEnter();
    
    ret = RegOpenKeyExA(base, key, 0, KEY_WRITE, &hKey);
    closeKeyLogAndReturnIf(ret != ERROR_SUCCESS, false, hKey, "Can't open key (%s)", winError(ret));
    
    ret = RegSetValueExA(hKey, valueName, 0, REG_BINARY, value, valueSize);
    closeKeyLogAndReturnIf(ret != ERROR_SUCCESS, false,  hKey, "Can't set value (%lu)", ret);

    logReturn(true, "OK", 0);
}

static bool delRegValue(HKEY base, const char *key, const char *valueName) {
    HKEY hKey;
    DWORD ret;

    logEnter();

    ret = RegOpenKeyExA(base, key, 0, KEY_WRITE, &hKey);
    closeKeyLogAndReturnIf(ret != ERROR_SUCCESS, false, hKey, "Can't open key (%s)", winError(ret));

    ret = RegDeleteValueA(hKey, valueName);
    closeKeyLogAndReturnIf(ret != ERROR_SUCCESS, false, hKey, "Can't delete value (%s)", winError(ret));

    logReturn(true, "OK", 0);
}

void setComputerName() {
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
    logEnter();
    snprintf(concat, sizeof(concat), "%s\\%s", root, filename);
    log("%s", concat);
    attrib = GetFileAttributesA(concat);
    ret = (attrib != INVALID_FILE_ATTRIBUTES) && (0 == (attrib & FILE_ATTRIBUTE_DIRECTORY));
    logReturn(ret, "%s", ret ? "exists" : "missing");
}

bool is98Lite() {
    char sysDir[MAX_PATH];
    bool ret;
    logEnter();
    GetSystemDirectoryA(sysDir, MAX_PATH);
    log("System directory: '%s'", sysDir);
    ret = fileExists(sysDir, "shell32.w98") || fileExists(sysDir, "shell32.wme");
    logReturn(ret, "System type: %s", ret ? "98Lite" : "stock");
}

void cleanupRegistry() {
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
    cleanupRegistry();
    doReboot();

#ifdef LOGGING
    CloseHandle(__logFile);
#endif

    return( 0 );
}
