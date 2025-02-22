/* Extract registers from an HP-PA core file, for GDB.
   Copyright (C) 1988, 1991, 1992  Free Software Foundation, Inc.

This file is part of GDB.

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

/* core.c is supposed to be the more machine-independent aspects of this;
   this file is more machine-specific.  */

#include "defs.h"
#include "gdbcore.h"

/* These are needed on various systems to expand REGISTER_U_ADDR.  */
/* FIXME:  Remove any not needed on HP-PA.  */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/dir.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/user.h>
#ifndef USG
#include <sys/ptrace.h>
#endif


/* Extract the register values out of the core file and store
   them where `read_register' will find them.

   CORE_REG_SECT points to the register values themselves, read into memory.
   CORE_REG_SIZE is the size of that area.
   WHICH says which set of registers we are handling (0 = int, 2 = float
         on machines where they are discontiguous).
   REG_ADDR is the offset from u.u_ar0 to the register values relative to
            core_reg_sect.  This is used with old-fashioned core files to
	    locate the registers in a large upage-plus-stack ".reg" section.
	    Original upage address X is at location core_reg_sect+x+reg_addr.
 */

void
fetch_core_registers (core_reg_sect, core_reg_size, which, reg_addr)
     char *core_reg_sect;
     unsigned core_reg_size;
     int which;
     unsigned reg_addr;
{
  register int regno;
  register unsigned int addr;
  int bad_reg = -1;
  register reg_ptr = -reg_addr;		/* Original u.u_ar0 is -reg_addr. */

  /* If u.u_ar0 was an absolute address in the core file, relativize it now,
     so we can use it as an offset into core_reg_sect.  When we're done,
     "register 0" will be at core_reg_sect+reg_ptr, and we can use
     register_addr to offset to the other registers.  If this is a modern
     core file without a upage, reg_ptr will be zero and this is all a big
     NOP.  */
  if (reg_ptr > core_reg_size)
    reg_ptr -= KERNEL_U_ADDR;
  if (reg_ptr > core_reg_size)
    fprintf (stderr, "Can't find registers in core file\n");

  for (regno = 0; regno < NUM_REGS; regno++)
    {
      addr = register_addr (regno, reg_ptr);
      if (addr >= core_reg_size) {
	if (bad_reg < 0)
	  bad_reg = regno;
      } else {
	if (regno == PCOQ_HEAD_REGNUM || regno == PCOQ_TAIL_REGNUM)
	  core_reg_sect[addr +3] &= ~0x3;
	supply_register (regno, core_reg_sect + addr);
      }
    }
  if (bad_reg > 0)
    {
      error ("Register %s not found in core file.", reg_names[bad_reg]);
    }
}


#ifdef REGISTER_U_ADDR

/* Return the address in the core dump or inferior of register REGNO.
   BLOCKEND is the address of the end of the user structure.  */

unsigned int
register_addr (regno, blockend)
     int regno;
     int blockend;
{
  int addr;

  if (regno < 0 || regno >= NUM_REGS)
    error ("Invalid register number %d.", regno);

  REGISTER_U_ADDR (addr, blockend, regno);

  return addr;
}

#endif /* REGISTER_U_ADDR */







