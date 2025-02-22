/* Copyright (C) 1991 Free Software Foundation, Inc.
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
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

/* Initialize STREAM as necessary.
   This may change I/O functions, give a buffer, etc.
   If no buffer is allocated, but the bufsize is set,
   the bufsize will be used to allocate the buffer.  */
void
DEFUN(__stdio_init_stream, (stream), FILE *stream)
{
  register CONST int fd = *(int *) stream->__cookie;
  struct stat statb;

  if (stream->__buffer != NULL || stream->__userbuf)
    /* If's unbuffered by request, we can't do anything useful.  */
    return;

  /* Find out what sort of file this is.  */
  if (__fstat(fd, &statb) < 0)
      return;

  if (S_ISFIFO(statb.st_mode))
    {
      /* It's a named pipe (FIFO).  Make it unbuffered.  */
      stream->__userbuf = 1;
      return;
    }

  if (S_ISCHR(statb.st_mode))
    {
      /* It's a character device.
	 Make it line-buffered if it's a terminal.  */
      if (__isatty(fd))
	stream->__linebuf = 1;
    }
  
  /* Use the block-size field to determine
     the system's optimal buffering size.  */
  stream->__bufsize = statb.st_blksize;
}
