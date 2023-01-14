#ifndef PLATFORM_H
#define PLATFORM_H

#include <stddef.h>
#include <sys/types.h>
#include <errno.h>

/* Generic types */

#if defined(__GNUC__) || defined(__clang__) || (defined(_MSC_VER) && (_MSC_VER >= 1600)) || (defined(__WATCOMC__) && (WATCOMC >= 1200))
#include <stdint.h>
#include <stdbool.h>
#endif

#if (_MSC_VER > 800)
typedef unsigned __int8_t uint8_t;
typedef __int8_t int8_t;
typedef unsigned __int16_t uint16_t;
typedef __int16_t int16_t;
typedef unsigned __int32_t uint32_t;
typedef __int32_t int32_t;
typedef unsigned __int64_t uint64_t;
typedef __int64_t int64_t;
#endif

#if !defined(bool) && !defined(BOOL) && defined(_Bool)
typedef _Bool bool;
#endif

#if !defined(bool)
/* nothing helped, we need to define it by hand.. */
typedef uint8_t bool;
#endif

#if !defined(true)
#define false (0)
#define true (1)
#endif

#if !defined(MIN)
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))
#endif

#if defined(__DOS__)
typedef uint8_t __huge *hugeptr;
typedef uint8_t __far *farptr;
#else
typedef uint8_t *hugeptr;
typedef uint8_t *farptr;

#endif

#define __PAGE(x) (x >> 16)
#define __SEGMENT(x) (_PAGE(x) << 4)
#define __OFFSET(x) (x & 0XFFFF)

#define UNUSED(x) ((void)x)

/* Path separator */

#if defined(__DOS__) || defined(_WIN32)
#define PATH_SEPARATOR '\\'
#else
#define PATH_SEPARATOR '/'
#endif

/* DOS attributes */
#if !defined(_A_NORMAL)
#define  _A_NORMAL       0x00    /* Normal file - read/write permitted */
#define  _A_RDONLY       0x01    /* Read-only file */
#define  _A_HIDDEN       0x02    /* Hidden file */
#define  _A_SYSTEM       0x04    /* System file */
#define  _A_VOLID        0x08    /* Volume-ID entry */
#define  _A_SUBDIR       0x10    /* Subdirectory */
#define  _A_ARCH         0x20    /* Archive file */
#endif

#if defined(_WIN32)
#include "windows.h"
#endif

#endif
