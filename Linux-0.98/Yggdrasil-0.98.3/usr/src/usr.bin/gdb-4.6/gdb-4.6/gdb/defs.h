/* Basic, host-specific, and target-specific definitions for GDB.
   Copyright (C) 1986, 1989, 1991 Free Software Foundation, Inc.

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

#if !defined (DEFS_H)
#define DEFS_H 1

#include <stdio.h>

/* First include ansidecl.h so we can use the various macro definitions
   here and in all subsequent file inclusions.  */

#include "ansidecl.h"

/* An address in the program being debugged.  Host byte order.  */
typedef unsigned int CORE_ADDR;

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

/* The character C++ uses to build identifiers that must be unique from
   the program's identifiers (such as $this and $$vptr).  */
#define CPLUS_MARKER '$'	/* May be overridden to '.' for SysV */

#include <errno.h>		/* System call error return status */

extern int quit_flag;
extern int immediate_quit;

extern void
quit PARAMS ((void));

#define QUIT { if (quit_flag) quit (); }

/* Notes on classes: class_alias is for alias commands which are not
   abbreviations of the original command.  */

enum command_class
{
  /* Special args to help_list */
  all_classes = -2, all_commands = -1,
  /* Classes of commands */
  no_class = -1, class_run = 0, class_vars, class_stack,
  class_files, class_support, class_info, class_breakpoint,
  class_alias, class_obscure, class_user, class_maintenance
};

/* the cleanup list records things that have to be undone
   if an error happens (descriptors to be closed, memory to be freed, etc.)
   Each link in the chain records a function to call and an
   argument to give it.

   Use make_cleanup to add an element to the cleanup chain.
   Use do_cleanups to do all cleanup actions back to a given
   point in the chain.  Use discard_cleanups to remove cleanups
   from the chain back to a given point, not doing them.  */

struct cleanup
{
  struct cleanup *next;
  void (*function) PARAMS ((PTR));
  PTR arg;
};

/* From blockframe.c */

extern int
inside_entry_func PARAMS ((CORE_ADDR));

extern int
inside_entry_file PARAMS ((CORE_ADDR addr));

extern int
inside_main_func PARAMS ((CORE_ADDR pc));

/* From cplus-dem.c */

extern char *
cplus_demangle PARAMS ((const char *, int));

extern char *
cplus_mangle_opname PARAMS ((char *, int));

/* From libmmalloc.a (memory mapped malloc library) */

extern PTR
mmalloc_attach PARAMS ((int, PTR));

extern PTR
mmalloc_detach PARAMS ((PTR));

extern PTR
mmalloc PARAMS ((PTR, long));

extern PTR
mrealloc PARAMS ((PTR, PTR, long));

extern void
mfree PARAMS ((PTR, PTR));

extern int
mmalloc_setkey PARAMS ((PTR, int, PTR));

extern PTR
mmalloc_getkey PARAMS ((PTR, int));

/* From utils.c */

extern char *
demangle_and_match PARAMS ((const char *, const char *, int));

extern int
strcmp_iw PARAMS ((const char *, const char *));

extern char *
safe_strerror PARAMS ((int));

extern char *
safe_strsignal PARAMS ((int));

extern void
init_malloc PARAMS ((PTR));

extern void
request_quit PARAMS ((int));

extern void
do_cleanups PARAMS ((struct cleanup *));

extern void
discard_cleanups PARAMS ((struct cleanup *));

/* The bare make_cleanup function is one of those rare beasts that
   takes almost any type of function as the first arg and anything that
   will fit in a "void *" as the second arg.

   Should be, once all calls and called-functions are cleaned up:
extern struct cleanup *
make_cleanup PARAMS ((void (*function) (PTR), PTR));

   Until then, lint and/or various type-checking compiler options will
   complain about make_cleanup calls.  It'd be wrong to just cast things,
   since the type actually passed when the function is called would be
   wrong.  */

extern struct cleanup *
make_cleanup ();

extern struct cleanup *
save_cleanups PARAMS ((void));

extern void
restore_cleanups PARAMS ((struct cleanup *));

extern void
free_current_contents PARAMS ((char **));

extern void
null_cleanup PARAMS ((char **));

extern int
myread PARAMS ((int, char *, int));

extern int
query ();

extern void
wrap_here PARAMS ((char *));

extern void
reinitialize_more_filter PARAMS ((void));

extern int
print_insn PARAMS ((CORE_ADDR, FILE *));

extern void
fputs_filtered PARAMS ((const char *, FILE *));

extern void
puts_filtered PARAMS ((char *));

extern void
fprintf_filtered ();

extern void
printf_filtered ();

extern void
printfi_filtered ();

extern void
print_spaces PARAMS ((int, FILE *));

extern void
print_spaces_filtered PARAMS ((int, FILE *));

extern char *
n_spaces PARAMS ((int));

extern void
printchar PARAMS ((int, FILE *, int));

extern char *
strdup_demangled PARAMS ((const char *));

extern void
fprint_symbol PARAMS ((FILE *, char *));

extern void
fputs_demangled PARAMS ((char *, FILE *, int));

extern void
perror_with_name PARAMS ((char *));

extern void
print_sys_errmsg PARAMS ((char *, int));

/* From regex.c */

extern char *
re_comp PARAMS ((char *));

/* From symfile.c */

extern void
symbol_file_command PARAMS ((char *, int));

/* From main.c */

extern char *
skip_quoted PARAMS ((char *));

extern char *
gdb_readline PARAMS ((char *));

extern char *
command_line_input PARAMS ((char *, int));

extern void
print_prompt PARAMS ((void));

extern int
batch_mode PARAMS ((void));

extern int
input_from_terminal_p PARAMS ((void));

extern int
catch_errors PARAMS ((int (*) (char *), char *, char *));

/* From printcmd.c */

extern void
set_next_address PARAMS ((CORE_ADDR));

extern void
print_address_symbolic PARAMS ((CORE_ADDR, FILE *, int, char *));

extern void
print_address PARAMS ((CORE_ADDR, FILE *));

/* From source.c */

extern int
openp PARAMS ((char *, int, char *, int, int, char **));

extern void
mod_path PARAMS ((char *, char **));

extern void
directory_command PARAMS ((char *, int));

extern void
init_source_path PARAMS ((void));

/* From findvar.c */

extern int
read_relative_register_raw_bytes PARAMS ((int, char *));

/* From readline (but not in any readline .h files).  */

extern char *
tilde_expand PARAMS ((char *));

/* Structure for saved commands lines
   (for breakpoints, defined commands, etc).  */

struct command_line
{
  struct command_line *next;
  char *line;
};

extern struct command_line *
read_command_lines PARAMS ((void));

extern void
free_command_lines PARAMS ((struct command_line **));

/* String containing the current directory (what getwd would return).  */

extern char *current_directory;

/* Default radixes for input and output.  Only some values supported.  */
extern unsigned input_radix;
extern unsigned output_radix;

/* Baud rate specified for communication with serial target systems.  */
extern char *baud_rate;

/* Languages represented in the symbol table and elsewhere. */

enum language 
{
   language_unknown, 		/* Language not known */
   language_auto,		/* Placeholder for automatic setting */
   language_c, 			/* C */
   language_cplus, 		/* C++ */
   language_m2			/* Modula-2 */
};

/* Return a format string for printf that will print a number in the local
   (language-specific) hexadecimal format.  Result is static and is
   overwritten by the next call.  local_hex_format_custom takes printf
   options like "08" or "l" (to produce e.g. %08x or %lx).  */

#define local_hex_format() (current_language->la_hex_format)

extern char *
local_hex_format_custom PARAMS ((char *));	/* language.c */

/* Return a string that contains a number formatted in the local
   (language-specific) hexadecimal format.  Result is static and is
   overwritten by the next call.  local_hex_string_custom takes printf
   options like "08" or "l".  */

extern char *
local_hex_string PARAMS ((int));		/* language.c */

extern char *
local_hex_string_custom PARAMS ((int, char *));	/* language.c */


/* Host machine definition.  This will be a symlink to one of the
   xm-*.h files, built by the `configure' script.  */

#include "xm.h"

/* If the xm.h file did not define the mode string used to open the
   files, assume that binary files are opened the same way as text
   files */
#ifndef FOPEN_RB
#include "fopen-same.h"
#endif

/*
 * Allow things in gdb to be declared "const".  If compiling ANSI, it
 * just works.  If compiling with gcc but non-ansi, redefine to __const__.
 * If non-ansi, non-gcc, then eliminate "const" entirely, making those
 * objects be read-write rather than read-only.
 */

#ifndef const
#ifndef __STDC__
# ifdef __GNUC__
#  define const __const__
# else
#  define const /*nothing*/
# endif /* GNUC */
#endif /* STDC */
#endif /* const */

#ifndef volatile
#ifndef __STDC__
# ifdef __GNUC__
#  define volatile __volatile__
# else
#  define volatile /*nothing*/
# endif /* GNUC */
#endif /* STDC */
#endif /* volatile */

/* Some compilers (many AT&T SVR4 compilers for instance), do not accept
   declarations of functions that never return (exit for instance) as
   "volatile void".  For such compilers "NORETURN" can be defined away
   to keep them happy */

#ifndef NORETURN
# ifdef __lucid
#   define NORETURN /*nothing*/
# else
#   define NORETURN volatile
# endif
#endif

/* Defaults for system-wide constants (if not defined by xm.h, we fake it).  */

#if !defined (UINT_MAX)
#define UINT_MAX 0xffffffff
#endif

#if !defined (LONG_MAX)
#define LONG_MAX 0x7fffffff
#endif

#if !defined (INT_MAX)
#define INT_MAX 0x7fffffff
#endif

#if !defined (INT_MIN)
/* Two's complement, 32 bit.  */
#define INT_MIN -0x80000000
#endif

/* Number of bits in a char or unsigned char for the target machine.
   Just like CHAR_BIT in <limits.h> but describes the target machine.  */
#if !defined (TARGET_CHAR_BIT)
#define TARGET_CHAR_BIT 8
#endif

/* Number of bits in a short or unsigned short for the target machine. */
#if !defined (TARGET_SHORT_BIT)
#define TARGET_SHORT_BIT (sizeof (short) * TARGET_CHAR_BIT)
#endif

/* Number of bits in an int or unsigned int for the target machine. */
#if !defined (TARGET_INT_BIT)
#define TARGET_INT_BIT (sizeof (int) * TARGET_CHAR_BIT)
#endif

/* Number of bits in a long or unsigned long for the target machine. */
#if !defined (TARGET_LONG_BIT)
#define TARGET_LONG_BIT (sizeof (long) * TARGET_CHAR_BIT)
#endif

/* Number of bits in a long long or unsigned long long for the target machine. */
#if !defined (TARGET_LONG_LONG_BIT)
#define TARGET_LONG_LONG_BIT (2 * TARGET_LONG_BIT)
#endif

/* Number of bits in a float for the target machine. */
#if !defined (TARGET_FLOAT_BIT)
#define TARGET_FLOAT_BIT (sizeof (float) * TARGET_CHAR_BIT)
#endif

/* Number of bits in a double for the target machine. */
#if !defined (TARGET_DOUBLE_BIT)
#define TARGET_DOUBLE_BIT (sizeof (double) * TARGET_CHAR_BIT)
#endif

/* Number of bits in a long double for the target machine. */
#if !defined (TARGET_LONG_DOUBLE_BIT)
#define TARGET_LONG_DOUBLE_BIT (2 * TARGET_DOUBLE_BIT)
#endif

/* Number of bits in a "complex" for the target machine. */
#if !defined (TARGET_COMPLEX_BIT)
#define TARGET_COMPLEX_BIT (2 * TARGET_FLOAT_BIT)
#endif

/* Number of bits in a "double complex" for the target machine. */
#if !defined (TARGET_DOUBLE_COMPLEX_BIT)
#define TARGET_DOUBLE_COMPLEX_BIT (2 * TARGET_DOUBLE_BIT)
#endif

/* Number of bits in a pointer for the target machine */
#if !defined (TARGET_PTR_BIT)
#define TARGET_PTR_BIT TARGET_INT_BIT
#endif

/* Convert a LONGEST to an int.  This is used in contexts (e.g. number
   of arguments to a function, number in a value history, register
   number, etc.) where the value must not be larger than can fit
   in an int.  */
#if !defined (longest_to_int)
#if defined (LONG_LONG)
#define longest_to_int(x) (((x) > INT_MAX || (x) < INT_MIN) \
			   ? (error ("Value out of range."),0) : (int) (x))
#else /* No LONG_LONG.  */
/* Assume sizeof (int) == sizeof (long).  */
#define longest_to_int(x) ((int) (x))
#endif /* No LONG_LONG.  */
#endif /* No longest_to_int.  */

/* This should not be a typedef, because "unsigned LONGEST" needs
   to work. LONG_LONG is defined if the host has "long long".  */

#ifndef LONGEST
# ifdef LONG_LONG
#  define LONGEST long long
# else
#  define LONGEST long
# endif
#endif

/* Assorted functions we can declare, now that const and volatile are 
   defined.  */

extern char *
savestring PARAMS ((const char *, int));

extern char *
msavestring PARAMS ((void *, const char *, int));

extern char *
strsave PARAMS ((const char *));

extern char *
mstrsave PARAMS ((void *, const char *));

extern char *
concat PARAMS ((char *, ...));

extern PTR
xmalloc PARAMS ((long));

extern PTR
xrealloc PARAMS ((PTR, long));

extern PTR
xmmalloc PARAMS ((PTR, long));

extern PTR
xmrealloc PARAMS ((PTR, PTR, long));

extern PTR
mmalloc PARAMS ((PTR, long));

extern PTR
mrealloc PARAMS ((PTR, PTR, long));

extern void
mfree PARAMS ((PTR, PTR));

extern int
mmcheck PARAMS ((PTR, void (*) (void)));

extern int
mmtrace PARAMS ((void));

extern int
parse_escape PARAMS ((char **));

extern const char * const reg_names[];

extern NORETURN void			/* Does not return to the caller.  */
error ();

extern NORETURN void			/* Does not return to the caller.  */
fatal ();

extern NORETURN void			/* Not specified as volatile in ... */
exit PARAMS ((int));			/* 4.10.4.3 */

extern NORETURN void			/* Does not return to the caller.  */
nomem PARAMS ((long));

extern NORETURN void			/* Does not return to the caller.  */
return_to_top_level PARAMS ((void));

extern void
warning_setup PARAMS ((void));

extern void
warning ();

/* Global functions from other, non-gdb GNU thingies (libiberty for
   instance) */

extern char *
basename PARAMS ((char *));

extern char *
getenv PARAMS ((CONST char *));

extern char **
buildargv PARAMS ((char *));

extern void
freeargv PARAMS ((char **));

extern char *
strerrno PARAMS ((int));

extern char *
strsigno PARAMS ((int));

extern int
errno_max PARAMS ((void));

extern int
signo_max PARAMS ((void));

extern int
strtoerrno PARAMS ((char *));

extern int
strtosigno PARAMS ((char *));

extern char *
strsignal PARAMS ((int));

/* From other system libraries */

#ifndef PSIGNAL_IN_SIGNAL_H
extern void
psignal PARAMS ((unsigned, char *));
#endif

/* For now, we can't include <stdlib.h> because it conflicts with
   "../include/getopt.h".  (FIXME)

   However, if a function is defined in the ANSI C standard and a prototype
   for that function is defined and visible in any header file in an ANSI
   conforming environment, then that prototype must match the definition in
   the ANSI standard.  So we can just duplicate them here without conflict,
   since they must be the same in all conforming ANSI environments.  If
   these cause problems, then the environment is not ANSI conformant. */
   
#ifdef __STDC__
#include <stddef.h>
#endif

extern int
fclose PARAMS ((FILE *stream));				/* 4.9.5.1 */

extern void
perror PARAMS ((const char *));				/* 4.9.10.4 */

extern double
atof PARAMS ((const char *nptr));			/* 4.10.1.1 */

extern int
atoi PARAMS ((const char *));				/* 4.10.1.2 */

#ifndef MALLOC_INCOMPATIBLE

extern PTR
malloc PARAMS ((size_t size));                          /* 4.10.3.3 */

extern PTR
realloc PARAMS ((void *ptr, size_t size));              /* 4.10.3.4 */

extern void
free PARAMS ((void *));					/* 4.10.3.2 */

#endif	/* MALLOC_INCOMPATIBLE */

extern void
qsort PARAMS ((void *base, size_t nmemb,		/* 4.10.5.2 */
	       size_t size,
	       int (*comp)(const void *, const void *)));

#ifndef	MEM_FNS_DECLARED	/* Some non-ANSI use void *, not char *.  */
extern PTR
memcpy PARAMS ((void *, const void *, size_t));		/* 4.11.2.1 */
#endif

extern int
memcmp PARAMS ((const void *, const void *, size_t));	/* 4.11.4.1 */

extern char *
strchr PARAMS ((const char *, int));			/* 4.11.5.2 */

extern char *
strrchr PARAMS ((const char *, int));			/* 4.11.5.5 */

extern char *
strstr PARAMS ((const char *, const char *));		/* 4.11.5.7 */

extern char *
strtok PARAMS ((char *, const char *));			/* 4.11.5.8 */

#ifndef	MEM_FNS_DECLARED	/* Some non-ANSI use void *, not char *.  */
extern PTR
memset PARAMS ((void *, int, size_t));			/* 4.11.6.1 */
#endif

extern char *
strerror PARAMS ((int));				/* 4.11.6.2 */

/* Various possibilities for alloca.  */
#ifndef alloca
# ifdef __GNUC__
#  define alloca __builtin_alloca
# else
#  ifdef sparc
#   include <alloca.h>		/* NOTE:  Doesn't declare alloca() */
#  endif
#  ifdef __STDC__
   extern void *alloca (size_t);
#  else /* __STDC__ */
   extern char *alloca ();
#  endif
# endif
#endif

/* TARGET_BYTE_ORDER and HOST_BYTE_ORDER must be defined to one of these.  */

#if !defined (BIG_ENDIAN)
#define BIG_ENDIAN 4321
#endif

#if !defined (LITTLE_ENDIAN)
#define LITTLE_ENDIAN 1234
#endif

/* Target-system-dependent parameters for GDB.

   The standard thing is to include defs.h.  However, files that are
   specific to a particular target can define TM_FILE_OVERRIDE before
   including defs.h, then can include any particular tm-file they desire.  */

/* Target machine definition.  This will be a symlink to one of the
   tm-*.h files, built by the `configure' script.  */

#ifndef TM_FILE_OVERRIDE
#include "tm.h"
#endif

/* The bit byte-order has to do just with numbering of bits in
   debugging symbols and such.  Conceptually, it's quite separate
   from byte/word byte order.  */

#if !defined (BITS_BIG_ENDIAN)
#if TARGET_BYTE_ORDER == BIG_ENDIAN
#define BITS_BIG_ENDIAN 1
#endif /* Big endian.  */

#if TARGET_BYTE_ORDER == LITTLE_ENDIAN
#define BITS_BIG_ENDIAN 0
#endif /* Little endian.  */
#endif /* BITS_BIG_ENDIAN not defined.  */

/* Swap LEN bytes at BUFFER between target and host byte-order.  */
#if TARGET_BYTE_ORDER == HOST_BYTE_ORDER
#define SWAP_TARGET_AND_HOST(buffer,len)
#else /* Target and host byte order differ.  */
#define SWAP_TARGET_AND_HOST(buffer,len) \
  {	       	       	       	       	       	       	       	       	 \
    char tmp;								 \
    char *p = (char *)(buffer);						 \
    char *q = ((char *)(buffer)) + len - 1;		   		 \
    for (; p < q; p++, q--)				 		 \
      {									 \
        tmp = *q;							 \
        *q = *p;							 \
        *p = tmp;							 \
      }									 \
  }
#endif /* Target and host byte order differ.  */

/* On some machines there are bits in addresses which are not really
   part of the address, but are used by the kernel, the hardware, etc.
   for special purposes.  ADDR_BITS_REMOVE takes out any such bits
   so we get a "real" address such as one would find in a symbol
   table.  ADDR_BITS_SET sets those bits the way the system wants
   them.  */
#if !defined (ADDR_BITS_REMOVE)
#define ADDR_BITS_REMOVE(addr) (addr)
#define ADDR_BITS_SET(addr) (addr)
#endif /* No ADDR_BITS_REMOVE.  */

/* From valops.c */

extern CORE_ADDR
push_bytes PARAMS ((CORE_ADDR, char *, int));

/* In some modules, we don't have a definition of REGISTER_TYPE yet, so we
   must avoid prototyping this function for now.  FIXME.  Should be:
extern CORE_ADDR
push_word PARAMS ((CORE_ADDR, REGISTER_TYPE));
 */
extern CORE_ADDR
push_word ();

/* Some parts of gdb might be considered optional, in the sense that they
   are not essential for being able to build a working, usable debugger
   for a specific environment.  For example, the maintenance commands
   are there for the benefit of gdb maintainers.  As another example,
   some environments really don't need gdb's that are able to read N
   different object file formats.  In order to make it possible (but
   not necessarily recommended) to build "stripped down" versions of
   gdb, the following defines control selective compilation of those
   parts of gdb which can be safely left out when necessary.  Note that
   the default is to include everything. */

#ifndef MAINTENANCE_CMDS
#define MAINTENANCE_CMDS 1
#endif

#endif /* !defined (DEFS_H) */
