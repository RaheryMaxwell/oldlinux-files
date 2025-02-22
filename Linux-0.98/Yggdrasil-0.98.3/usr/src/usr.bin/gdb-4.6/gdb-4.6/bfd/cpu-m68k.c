/* BFD library support routines for architectures.
   Copyright (C) 1990-1991 Free Software Foundation, Inc.
   Hacked by Steve Chamberlain of Cygnus Support.

This file is part of BFD, the Binary File Descriptor library.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"

int bfd_default_scan_num_mach();


#define N(name, print,d)  \
{  32, 32, 8, bfd_arch_m68k, name, "m68k",print,2,d,bfd_default_compatible,bfd_default_scan, 0, 0x20000, 0x2000, }

static bfd_arch_info_type arch_info_struct[] =
{ 
  N(68000,"m68k:68000",false),
  N(68008,"m68k:68008",false),
  N(68010,"m68k:68010",false),
  N(68020,"m68k:68020",true), /* the word m68k will match this too */
  N(68030,"m68k:68030",false),
  N(68040,"m68k:68040",false),
  N(68070,"m68k:68070",false),
  0
}
;


void DEFUN_VOID(bfd_m68k_arch)
{
  unsigned int i;
  for (i = 0; arch_info_struct[i].bits_per_word; i++)
      {
	bfd_arch_linkin(&arch_info_struct[i]);
      }
}



