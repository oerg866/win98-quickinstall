#ifndef _QI_ASSERT_H_
#define _QI_ASSERT_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "anbui/anbui.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"

static inline void __qi__assert(const char * assertion, const char * file, unsigned int line, const char * func) {
    ad_deinit();
    system("clear");
    printf("GURU MEDITATION!\n\nFile '%s' | Line %u | Function '%s'\n\nCondition '%s' failed!\n\nExiting to shell.\n", file, line, func, assertion);
    sync();
    abort();
}

static inline void __qi__fatal_error(const char *msg, const char * assertion, const char * file, unsigned int line, const char * func) {
    ad_okBox("Fatal Error", false,
        "ERROR:\n"
        "%s\n"
        "This is an unrecoverable error and the installer must close.\n\n"
        "Debug info:\n"
        "Assertion: %s\n"
        "Function:  %s\n"
        "File:      %s:%u",
        msg, assertion, func, file, line);
    ad_deinit();
    system("clear");
    sync();
    abort();
}

#pragma GCC diagnostic pop

#define QI_ASSERT(x) ((void)((x) || (__qi__assert(#x, __FILE__, __LINE__, __func__),0)))

#define QI_FATAL(x, msg) ((void)((x) || (__qi__fatal_error(msg, #x, __FILE__, __LINE__, __func__),0)))

#endif