/* Definitions of global stdio data structures.

Copyright (C) 1991 Free Software Foundation, Inc.
This file is part of the GNU C Library.

The GNU C Library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

The GNU C Library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with the GNU C Library; see the file COPYING.LIB.  If
not, write to the Free Software Foundation, Inc., 675 Mass Ave,
Cambridge, MA 02139, USA.  */

#include <ansidecl.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>

/* This file defines all the global internal variables for stdio.  */

/* Standard streams.  */
#define	READ		1, 0
#define	WRITE		0, 1
#define	BUFFERED	0
#define	UNBUFFERED	1
#define	stdstream(name, next, fd, readwrite, unbuffered)		      \
    {									      \
      _IOMAGIC,								      \
      NULL, NULL, NULL, NULL, 0,					      \
      (next),								      \
      &(name)->__fileno, fd,						      \
      '\0', NULL, 0,							      \
      0, 0,								      \
      unbuffered, 0,							      \
      0, 0,								      \
      { readwrite, /* ... */ },						      \
      { NULL, NULL, NULL, NULL },					      \
      { NULL, NULL },							      \
      0, 0								      \
    }
static FILE stdstreams[3] =
  {
    stdstream (&stdstreams[0], &stdstreams[1], STDIN_FILENO, READ, BUFFERED),
    stdstream (&stdstreams[1], &stdstreams[2], STDOUT_FILENO, WRITE, BUFFERED),
    stdstream (&stdstreams[2], NULL, STDERR_FILENO, WRITE, UNBUFFERED),
  };
FILE *stdin = &stdstreams[0];
FILE *stdout = &stdstreams[1];
FILE *stderr = &stdstreams[2];

/* Pointer to the first stream in the list.  */
FILE *__stdio_head = &stdstreams[0];

/* This function MUST be in this file!
   This is because we want _cleanup to go into the __libc_atexit set
   when any stdio code is used (and to use any stdio code, one must reference
   something defined in this file), and since only local symbols can be made
   set elements, having the set element stab entry here and _cleanup elsewhere
   loses; and having them both elsewhere loses because there is no reference
   to cause _cleanup to be linked in.  */

void
DEFUN_VOID(_cleanup)
{
  (void) fclose((FILE *) NULL);
}


#ifdef	HAVE_GNU_LD
#include <gnu-stabs.h>

text_set_element(__libc_atexit, _cleanup);
#endif
