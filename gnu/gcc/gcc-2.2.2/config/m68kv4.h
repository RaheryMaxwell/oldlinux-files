/* Target definitions for GNU compiler for mc680x0 running System V.4
   Copyright (C) 1991 Free Software Foundation, Inc.

   Written by Ron Guilmette (rfg@ncd.com) and Fred Fish (fnf@cygnus.com).

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

/* Use SGS_* macros to control compilation in m68k.md */

#define SGS_SWITCH_TABLES	/* Different switch table handling */

#include "m68ksgs.h"		/* The m68k/SVR4 assembler is SGS based */
#include "svr4.h"		/* Pick up the generic SVR4 macros */

/* See m68k.h.  7 means 68020 with 68881.  */

#ifndef TARGET_DEFAULT
#define	TARGET_DEFAULT (5 /*68020*/ + 2 /*68881*/)
#endif

/*  Override the definition of NO_DOLLAR_IN_LABEL in svr4.h, for special
    g++ assembler names.  When this is defined, g++ uses embedded '.'
    characters and some m68k assemblers have problems with this.  The
    chances are much greater that any particular assembler will permit
    embedded '$' characters. */

#undef NO_DOLLAR_IN_LABEL

/* Define PCC_STATIC_STRUCT_RETURN if the convention on the target machine
   is to use the nonreentrant technique for returning structure and union
   values, as commonly implemented by the AT&T Portable C Compiler (PCC).
   When defined, the gcc option -fpcc-struct-return can be used to cause
   this form to be generated.  When undefined, the option does nothing.
   For m68k SVR4, the convention is to use a reentrant technique compatible
   with the gcc default, so override the definition of this macro in m68k.h */

#undef PCC_STATIC_STRUCT_RETURN

/* Provide a set of pre-definitions and pre-assertions appropriate for
   the m68k running svr4.  __svr4__ is our extension.  */

#define CPP_PREDEFINES \
  "-Dm68k -Dunix -D__svr4__ -Asystem(unix) -Acpu(m68k) -Amachine(m68k)"

/* Test to see if the target includes a 68881 by default, and use CPP_SPEC
   to control whether or not __HAVE_68881__ is defined by default or not.
   If a 68881 is the default, gcc will use inline 68881 instructions, by
   predefining __HAVE_68881__, unless -msoft-float is specified.
   If a 68881 is not the default, gcc will only define __HAVE_68881__ if
   -m68881 is specified. */

#if TARGET_DEFAULT & 2
#define CPP_SPEC "%{!msoft-float:-D__HAVE_68881__}"
#else
#define CPP_SPEC "%{m68881:-D__HAVE_68881__}"
#endif

/* Output assembler code to FILE to increment profiler label # LABELNO
   for profiling a function entry.  We override the definition in m68k.h
   and match the way the native m68k/SVR4 compiler does profiling, with the
   address of the profile counter in a1, not a0, and using bsr rather
   than jsr. */

#undef FUNCTION_PROFILER
#define FUNCTION_PROFILER(FILE, LABELNO)				\
  asm_fprintf ((FILE), "\tlea.l\t(%LLP%d,%Rpc),%Ra1\n\tbsr\t_mcount\n", \
	       (LABELNO))

/* Local common symbols are declared to the assembler with ".lcomm" rather
   than ".bss", so override the definition in svr4.h */

#undef BSS_ASM_OP
#define BSS_ASM_OP	".lcomm"

/* Register in which address to store a structure value is passed to a
   function.  The default in m68k.h is a1.  For m68k/SVR4 it is a0. */

#undef STRUCT_VALUE_REGNUM
#define STRUCT_VALUE_REGNUM 8

#define ASM_COMMENT_START "#"

#undef TYPE_OPERAND_FMT
#define TYPE_OPERAND_FMT      "@%s"

/* Define how the m68k registers should be numbered for Dwarf output.
   The numbering provided here should be compatible with the native
   SVR4 SDB debugger in the m68k/SVR4 reference port, where d0-d7
   are 0-7, a0-a8 are 8-15, and fp0-fp7 are 16-23. */

#define DBX_REGISTER_NUMBER(REGNO) (REGNO)

/* The ASM_OUTPUT_SKIP macro is first defined in m68k.h, using ".skip".
   It is then overridden by m68ksgs.h to use ".space", and again by svr4.h
   to use ".zero".  The m68k/SVR4 assembler uses ".space", so repeat the
   definition from m68ksgs.h here.  Note that ASM_NO_SKIP_IN_TEXT is
   defined in m68ksgs.h, so we don't have to repeat it here. */

#undef ASM_OUTPUT_SKIP
#define ASM_OUTPUT_SKIP(FILE,SIZE)  \
  fprintf (FILE, "\t%s %u\n", SPACE_ASM_OP, (SIZE))

/* 1 if N is a possible register number for a function value.
   For m68k/SVR4 allow d0, a0, or fp0 as return registers, for integral,
   pointer, or floating types, respectively. Reject fp0 if not using a
   68881 coprocessor. */

#undef FUNCTION_VALUE_REGNO_P
#define FUNCTION_VALUE_REGNO_P(N) \
  ((N) == 0 || (N) == 8 || (TARGET_68881 && (N) == 16))

/* Define how to generate (in the callee) the output value of a function
   and how to find (in the caller) the value returned by a function.  VALTYPE
   is the data type of the value (as a tree).  If the precise function being
   called is known, FUNC is its FUNCTION_DECL; otherwise, FUNC is 0.
   For m68k/SVR4 generate the result in d0, a0, or fp0 as appropriate. */
   
#undef FUNCTION_VALUE
#define FUNCTION_VALUE(VALTYPE, FUNC)					\
  (TREE_CODE (VALTYPE) == REAL_TYPE && TARGET_68881			\
   ? gen_rtx (REG, TYPE_MODE (VALTYPE), 16)				\
   : (TREE_CODE (VALTYPE) == POINTER_TYPE				\
      ? gen_rtx (REG, TYPE_MODE (VALTYPE), 8)				\
      : gen_rtx (REG, TYPE_MODE (VALTYPE), 0)))

/* For compatibility with the large body of existing code which does not
   always properly declare external functions returning pointer types, the
   m68k/SVR4 convention is to copy the value returned for pointer functions
   from a0 to d0 in the function epilogue, so that callers that have
   neglected to properly declare the callee can still find the correct return
   value. */

extern int current_function_returns_pointer;
#define FUNCTION_EXTRA_EPILOGUE(FILE, SIZE)				\
do {									\
  if ((current_function_returns_pointer) && 				\
      ! find_equiv_reg (0, get_last_insn (), 0, 0, 0, 8, Pmode))	\
    asm_fprintf (FILE, "\tmov.l %Ra0,%Rd0\n");				\
} while (0);

/* Define how to find the value returned by a library function assuming the
   value has mode MODE.
   For m68k/SVR4 look for integer values in d0, pointer values in d0
   (returned in both d0 and a0), and floating values in fp0. */

#undef LIBCALL_VALUE
#define LIBCALL_VALUE(MODE)						\
  (((MODE) == SFmode || (MODE) == DFmode) && TARGET_68881		\
   ? gen_rtx (REG, (MODE), 16)						\
   : gen_rtx (REG, (MODE), 0))

/* Boundary (in *bits*) on which stack pointer should be aligned.
   The m68k/SVR4 convention is to keep the stack pointer longword aligned. */
 
#undef STACK_BOUNDARY
#define STACK_BOUNDARY 32

/* Alignment of field after `int : 0' in a structure.
   For m68k/SVR4, this is the next longword boundary. */

#undef EMPTY_FIELD_BOUNDARY
#define EMPTY_FIELD_BOUNDARY 32

/* No data type wants to be aligned rounder than this.
   For m68k/SVR4, some types (doubles for example) are aligned on 8 byte
   boundaries */
	
#undef BIGGEST_ALIGNMENT
#define BIGGEST_ALIGNMENT 64
