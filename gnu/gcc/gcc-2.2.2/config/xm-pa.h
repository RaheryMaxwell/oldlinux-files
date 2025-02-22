/* Configuration for GNU C-compiler for PA-RISC.
   Copyright (C) 1988 Free Software Foundation, Inc.
   Contributed by Michael Tiemann (tiemann@cygnus.com).

This file is part of GNU CC.

GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#ifdef hpux

#include <magic.h>
#if defined(SHL_MAGIC)
#define hpux8 1
#endif

#define USG
#define bcopy(a,b,c) memcpy (b,a,c)
#define bzero(a,b) memset (a,0,b)
#define bcmp(a,b,c) memcmp (a,b,c)

#else /* hpux */
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
extern int errno;
#endif /* hpux */

/* #defines that need visibility everywhere.  */
#define FALSE 0
#define TRUE 1

/* This describes the machine the compiler is hosted on.  */
#define HOST_BITS_PER_CHAR 8
#define HOST_BITS_PER_SHORT 16
#define HOST_BITS_PER_INT 32
#define HOST_BITS_PER_LONG 32
#define HOST_BITS_PER_LONGLONG 64

/* Doubles are stored in memory with the high order word first.  This
   matters when cross-compiling.  */
#define HOST_WORDS_BIG_ENDIAN 1

/* Place any machine-dependent include files here, in case we
   are bootstrapping.  */

/* target machine dependencies.
   tm.h is a symbolic link to the actual target specific file.   */
#include "tm.h"

/* Arguments to use with `exit'.  */
#define SUCCESS_EXIT_CODE 0
#define FATAL_EXIT_CODE 33

#if 0
/* If compiled with GNU C, use the built-in alloca */
#ifdef __GNUC__
#define alloca __builtin_alloca
#endif
#endif

/* Don't try to use sys_siglist.  */
#define NO_SYS_SIGLIST

/* HP's compiler has problems with enum bitfields.  */
#define ONLY_INT_FIELDS
