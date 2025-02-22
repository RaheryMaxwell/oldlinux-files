/* Copyright (C) 1992 Free Software Foundation, Inc.
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
#include <errno.h>
#include <stddef.h>
#include <termios.h>
#include <unistd.h>

/* Flush pending data on FD.  */
int
DEFUN(tcflush, (fd, queue_selector), int fd AND int queue_selector)
{
  switch (queue_selector)
    {
    case TCIFLUSH:
      return ioctl (fd, TCFLSH, 0);
    case TCOFLUSH:
      return ioctl (fd, TCFLSH, 1);
    case TCIOFLUSH:
      return ioctl (fd, TCFLSH, 2);
    default:
      errno = EINVAL;
      return -1;
    }
}
