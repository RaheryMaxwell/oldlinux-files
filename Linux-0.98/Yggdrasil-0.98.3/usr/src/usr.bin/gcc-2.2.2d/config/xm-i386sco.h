/* Configuration for GCC for Intel i386 running SCO.  */

#include "xm-i386v.h"

/* On SCO 3.2.1, ldexp rejects values outside [0.5, 1). */

#define BROKEN_LDEXP

/* Big buffers improve performance.  */

#define IO_BUFFER_SIZE (0x8000 - 1024)
