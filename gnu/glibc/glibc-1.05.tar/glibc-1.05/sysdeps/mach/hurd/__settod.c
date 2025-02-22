/* Copyright (C) 1991, 1992 Free Software Foundation, Inc.
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
#include <sys/time.h>

/* Set the current time of day and timezone information.
   This call is restricted to the super-user.  */
int
DEFUN(__settimeofday, (tv, tz),
      CONST struct timeval *tv AND CONST struct timezone *tz)
{
  error_t err;
  mach_port_t hostpriv;

  if (tz != NULL)
    {
      errno = ENOSYS;
      return -1;
    }

  hostpriv = __pid2task (-1);
  if (hostpriv == MACH_PORT_NULL)
    return -1;

  err = __host_set_time (hostpriv, tv);
  __mach_port_deallocate (__mach_task_self (), hostpriv);

  if (err)
    return __hurd_fail (err);

  return 0;
}
