/* DWARF debugging format support for GDB.
   Copyright (C) 1991, 1992 Free Software Foundation, Inc.
   Written by Fred Fish at Cygnus Support.  Portions based on dbxread.c,
   mipsread.c, coffread.c, and dwarfread.c from a Data General SVR4 gdb port.

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

/*

FIXME: Figure out how to get the frame pointer register number in the
execution environment of the target.  Remove R_FP kludge

FIXME: Add generation of dependencies list to partial symtab code.

FIXME: Resolve minor differences between what information we put in the
partial symbol table and what dbxread puts in.  For example, we don't yet
put enum constants there.  And dbxread seems to invent a lot of typedefs
we never see.  Use the new printpsym command to see the partial symbol table
contents.

FIXME: Figure out a better way to tell gdb about the name of the function
contain the user's entry point (I.E. main())

FIXME: See other FIXME's and "ifdef 0" scattered throughout the code for
other things to work on, if you get bored. :-)

*/

#include "defs.h"
#include <varargs.h>
#include <fcntl.h>
#include <string.h>

#include "bfd.h"
#include "symtab.h"
#include "gdbtypes.h"
#include "symfile.h"
#include "objfiles.h"
#include "libbfd.h"	/* FIXME Secret Internal BFD stuff (bfd_read) */
#include "elf/dwarf.h"
#include "buildsym.h"
#include "demangle.h"

#ifdef MAINTENANCE	/* Define to 1 to compile in some maintenance stuff */
#define SQUAWK(stuff) dwarfwarn stuff
#else
#define SQUAWK(stuff)
#endif

#ifndef R_FP		/* FIXME */
#define R_FP 14		/* Kludge to get frame pointer register number */
#endif

typedef unsigned int DIE_REF;	/* Reference to a DIE */

#ifndef GCC_PRODUCER
#define GCC_PRODUCER "GNU C "
#endif

#ifndef GPLUS_PRODUCER
#define GPLUS_PRODUCER "GNU C++ "
#endif

#ifndef LCC_PRODUCER
#define LCC_PRODUCER "LUCID C++ "
#endif

#ifndef CFRONT_PRODUCER
#define CFRONT_PRODUCER "CFRONT "	/* A wild a** guess... */
#endif

#define STREQ(a,b)		(strcmp(a,b)==0)
#define STREQN(a,b,n)		(strncmp(a,b,n)==0)

/* Flags to target_to_host() that tell whether or not the data object is
   expected to be signed.  Used, for example, when fetching a signed
   integer in the target environment which is used as a signed integer
   in the host environment, and the two environments have different sized
   ints.  In this case, *somebody* has to sign extend the smaller sized
   int. */

#define GET_UNSIGNED	0	/* No sign extension required */
#define GET_SIGNED	1	/* Sign extension required */

/* Defines for things which are specified in the document "DWARF Debugging
   Information Format" published by UNIX International, Programming Languages
   SIG.  These defines are based on revision 1.0.0, Jan 20, 1992. */

#define SIZEOF_DIE_LENGTH	4
#define SIZEOF_DIE_TAG		2
#define SIZEOF_ATTRIBUTE	2
#define SIZEOF_FORMAT_SPECIFIER	1
#define SIZEOF_FMT_FT		2
#define SIZEOF_LINETBL_LENGTH	4
#define SIZEOF_LINETBL_LINENO	4
#define SIZEOF_LINETBL_STMT	2
#define SIZEOF_LINETBL_DELTA	4
#define SIZEOF_LOC_ATOM_CODE	1

#define FORM_FROM_ATTR(attr)	((attr) & 0xF)	/* Implicitly specified */

/* Macros that return the sizes of various types of data in the target
   environment.

   FIXME:  Currently these are just compile time constants (as they are in
   other parts of gdb as well).  They need to be able to get the right size
   either from the bfd or possibly from the DWARF info.  It would be nice if
   the DWARF producer inserted DIES that describe the fundamental types in
   the target environment into the DWARF info, similar to the way dbx stabs
   producers produce information about their fundamental types. */

#define TARGET_FT_POINTER_SIZE(objfile)	(TARGET_PTR_BIT / TARGET_CHAR_BIT)
#define TARGET_FT_LONG_SIZE(objfile)	(TARGET_LONG_BIT / TARGET_CHAR_BIT)

/* The Amiga SVR4 header file <dwarf.h> defines AT_element_list as a
   FORM_BLOCK2, and this is the value emitted by the AT&T compiler.
   However, the Issue 2 DWARF specification from AT&T defines it as
   a FORM_BLOCK4, as does the latest specification from UI/PLSIG.
   For backwards compatibility with the AT&T compiler produced executables
   we define AT_short_element_list for this variant. */

#define	AT_short_element_list	 (0x00f0|FORM_BLOCK2)

/* External variables referenced. */

extern int info_verbose;		/* From main.c; nonzero => verbose */
extern char *warning_pre_print;		/* From utils.c */

/* The DWARF debugging information consists of two major pieces,
   one is a block of DWARF Information Entries (DIE's) and the other
   is a line number table.  The "struct dieinfo" structure contains
   the information for a single DIE, the one currently being processed.

   In order to make it easier to randomly access the attribute fields
   of the current DIE, which are specifically unordered within the DIE,
   each DIE is scanned and an instance of the "struct dieinfo"
   structure is initialized.

   Initialization is done in two levels.  The first, done by basicdieinfo(),
   just initializes those fields that are vital to deciding whether or not
   to use this DIE, how to skip past it, etc.  The second, done by the
   function completedieinfo(), fills in the rest of the information.

   Attributes which have block forms are not interpreted at the time
   the DIE is scanned, instead we just save pointers to the start
   of their value fields.

   Some fields have a flag <name>_p that is set when the value of the
   field is valid (I.E. we found a matching attribute in the DIE).  Since
   we may want to test for the presence of some attributes in the DIE,
   such as AT_low_pc, without restricting the values of the field,
   we need someway to note that we found such an attribute.
   
 */
   
typedef char BLOCK;

struct dieinfo {
  char *		die;		/* Pointer to the raw DIE data */
  unsigned long 	die_length;	/* Length of the raw DIE data */
  DIE_REF		die_ref;	/* Offset of this DIE */
  unsigned short	die_tag;	/* Tag for this DIE */
  unsigned long		at_padding;
  unsigned long		at_sibling;
  BLOCK *		at_location;
  char *		at_name;
  unsigned short	at_fund_type;
  BLOCK *		at_mod_fund_type;
  unsigned long		at_user_def_type;
  BLOCK *		at_mod_u_d_type;
  unsigned short	at_ordering;
  BLOCK *		at_subscr_data;
  unsigned long		at_byte_size;
  unsigned short	at_bit_offset;
  unsigned long		at_bit_size;
  BLOCK *		at_element_list;
  unsigned long		at_stmt_list;
  unsigned long		at_low_pc;
  unsigned long		at_high_pc;
  unsigned long		at_language;
  unsigned long		at_member;
  unsigned long		at_discr;
  BLOCK *		at_discr_value;
  unsigned short	at_visibility;
  unsigned long		at_import;
  BLOCK *		at_string_length;
  char *		at_comp_dir;
  char *		at_producer;
  unsigned long		at_frame_base;
  unsigned long		at_start_scope;
  unsigned long		at_stride_size;
  unsigned long		at_src_info;
  char *		at_prototyped;
  unsigned int		has_at_low_pc:1;
  unsigned int		has_at_stmt_list:1;
  unsigned int		short_element_list:1;
};

static int diecount;	/* Approximate count of dies for compilation unit */
static struct dieinfo *curdie;	/* For warnings and such */

static char *dbbase;	/* Base pointer to dwarf info */
static int dbroff;	/* Relative offset from start of .debug section */
static char *lnbase;	/* Base pointer to line section */
static int isreg;	/* Kludge to identify register variables */
static int offreg;	/* Kludge to identify basereg references */

/* This value is added to each symbol value.  FIXME:  Generalize to 
   the section_offsets structure used by dbxread.  */
static CORE_ADDR baseaddr;	/* Add to each symbol value */

/* The section offsets used in the current psymtab or symtab.  FIXME,
   only used to pass one value (baseaddr) at the moment.  */
static struct section_offsets *base_section_offsets;

/* Each partial symbol table entry contains a pointer to private data for the
   read_symtab() function to use when expanding a partial symbol table entry
   to a full symbol table entry.  For DWARF debugging info, this data is
   contained in the following structure and macros are provided for easy
   access to the members given a pointer to a partial symbol table entry.

   dbfoff	Always the absolute file offset to the start of the ".debug"
		section for the file containing the DIE's being accessed.

   dbroff	Relative offset from the start of the ".debug" access to the
		first DIE to be accessed.  When building the partial symbol
		table, this value will be zero since we are accessing the
		entire ".debug" section.  When expanding a partial symbol
		table entry, this value will be the offset to the first
		DIE for the compilation unit containing the symbol that
		triggers the expansion.

   dblength	The size of the chunk of DIE's being examined, in bytes.

   lnfoff	The absolute file offset to the line table fragment.  Ignored
		when building partial symbol tables, but used when expanding
		them, and contains the absolute file offset to the fragment
		of the ".line" section containing the line numbers for the
		current compilation unit.
 */

struct dwfinfo {
  int dbfoff;		/* Absolute file offset to start of .debug section */
  int dbroff;		/* Relative offset from start of .debug section */
  int dblength;		/* Size of the chunk of DIE's being examined */
  int lnfoff;		/* Absolute file offset to line table fragment */
};

#define DBFOFF(p) (((struct dwfinfo *)((p)->read_symtab_private))->dbfoff)
#define DBROFF(p) (((struct dwfinfo *)((p)->read_symtab_private))->dbroff)
#define DBLENGTH(p) (((struct dwfinfo *)((p)->read_symtab_private))->dblength)
#define LNFOFF(p) (((struct dwfinfo *)((p)->read_symtab_private))->lnfoff)

/* The generic symbol table building routines have separate lists for
   file scope symbols and all all other scopes (local scopes).  So
   we need to select the right one to pass to add_symbol_to_list().
   We do it by keeping a pointer to the correct list in list_in_scope.

   FIXME:  The original dwarf code just treated the file scope as the first
   local scope, and all other local scopes as nested local scopes, and worked
   fine.  Check to see if we really need to distinguish these in buildsym.c */

struct pending **list_in_scope = &file_symbols;

/* DIES which have user defined types or modified user defined types refer to
   other DIES for the type information.  Thus we need to associate the offset
   of a DIE for a user defined type with a pointer to the type information.

   Originally this was done using a simple but expensive algorithm, with an
   array of unsorted structures, each containing an offset/type-pointer pair.
   This array was scanned linearly each time a lookup was done.  The result
   was that gdb was spending over half it's startup time munging through this
   array of pointers looking for a structure that had the right offset member.

   The second attempt used the same array of structures, but the array was
   sorted using qsort each time a new offset/type was recorded, and a binary
   search was used to find the type pointer for a given DIE offset.  This was
   even slower, due to the overhead of sorting the array each time a new
   offset/type pair was entered.

   The third attempt uses a fixed size array of type pointers, indexed by a
   value derived from the DIE offset.  Since the minimum DIE size is 4 bytes,
   we can divide any DIE offset by 4 to obtain a unique index into this fixed
   size array.  Since each element is a 4 byte pointer, it takes exactly as
   much memory to hold this array as to hold the DWARF info for a given
   compilation unit.  But it gets freed as soon as we are done with it. */

static struct type **utypes;	/* Pointer to array of user type pointers */
static int numutypes;		/* Max number of user type pointers */

/* Forward declarations of static functions so we don't have to worry
   about ordering within this file.  */

static int
attribute_size PARAMS ((unsigned int));

static unsigned long
target_to_host PARAMS ((char *, int, int, struct objfile *));

static void
add_enum_psymbol PARAMS ((struct dieinfo *, struct objfile *));

static void
handle_producer PARAMS ((char *));

static void
read_file_scope PARAMS ((struct dieinfo *, char *, char *, struct objfile *));

static void
read_func_scope PARAMS ((struct dieinfo *, char *, char *, struct objfile *));

static void
read_lexical_block_scope PARAMS ((struct dieinfo *, char *, char *,
				  struct objfile *));

static void
dwarfwarn ();

static void
scan_partial_symbols PARAMS ((char *, char *, struct objfile *));

static void
scan_compilation_units PARAMS ((char *, char *, char *, unsigned int,
				unsigned int, struct objfile *));

static void
add_partial_symbol PARAMS ((struct dieinfo *, struct objfile *));

static void
init_psymbol_list PARAMS ((struct objfile *, int));

static void
basicdieinfo PARAMS ((struct dieinfo *, char *, struct objfile *));

static void
completedieinfo PARAMS ((struct dieinfo *, struct objfile *));

static void
dwarf_psymtab_to_symtab PARAMS ((struct partial_symtab *));

static void
psymtab_to_symtab_1 PARAMS ((struct partial_symtab *));

static struct symtab *
read_ofile_symtab PARAMS ((struct partial_symtab *));

static void
process_dies PARAMS ((char *, char *, struct objfile *));

static void
read_structure_scope PARAMS ((struct dieinfo *, char *, char *,
			      struct objfile *));

static struct type *
decode_array_element_type PARAMS ((char *));

static struct type *
decode_subscr_data PARAMS ((char *, char *));

static void
dwarf_read_array_type PARAMS ((struct dieinfo *));

static void
read_tag_pointer_type PARAMS ((struct dieinfo *dip));

static void
read_subroutine_type PARAMS ((struct dieinfo *, char *, char *));

static void
read_enumeration PARAMS ((struct dieinfo *, char *, char *, struct objfile *));

static struct type *
struct_type PARAMS ((struct dieinfo *, char *, char *, struct objfile *));

static struct type *
enum_type PARAMS ((struct dieinfo *, struct objfile *));

static void
decode_line_numbers PARAMS ((char *));

static struct type *
decode_die_type PARAMS ((struct dieinfo *));

static struct type *
decode_mod_fund_type PARAMS ((char *));

static struct type *
decode_mod_u_d_type PARAMS ((char *));

static struct type *
decode_modified_type PARAMS ((char *, unsigned int, int));

static struct type *
decode_fund_type PARAMS ((unsigned int));

static char *
create_name PARAMS ((char *, struct obstack *));

static struct type *
lookup_utype PARAMS ((DIE_REF));

static struct type *
alloc_utype PARAMS ((DIE_REF, struct type *));

static struct symbol *
new_symbol PARAMS ((struct dieinfo *, struct objfile *));

static int
locval PARAMS ((char *));

static void
record_minimal_symbol PARAMS ((char *, CORE_ADDR, enum minimal_symbol_type,
			       struct objfile *));

/*

GLOBAL FUNCTION

	dwarf_build_psymtabs -- build partial symtabs from DWARF debug info

SYNOPSIS

	void dwarf_build_psymtabs (int desc, char *filename, 
	     struct section_offsets *section_offsets,
	     int mainline, unsigned int dbfoff, unsigned int dbsize,
	     unsigned int lnoffset, unsigned int lnsize,
	     struct objfile *objfile)

DESCRIPTION

	This function is called upon to build partial symtabs from files
	containing DIE's (Dwarf Information Entries) and DWARF line numbers.

	It is passed a file descriptor for an open file containing the DIES
	and line number information, the corresponding filename for that
	file, a base address for relocating the symbols, a flag indicating
	whether or not this debugging information is from a "main symbol
	table" rather than a shared library or dynamically linked file,
	and file offset/size pairs for the DIE information and line number
	information.

RETURNS

	No return value.

 */

void
dwarf_build_psymtabs (desc, filename, section_offsets, mainline, dbfoff, dbsize,
		      lnoffset, lnsize, objfile)
     int desc;
     char *filename;
     struct section_offsets *section_offsets;
     int mainline;
     unsigned int dbfoff;
     unsigned int dbsize;
     unsigned int lnoffset;
     unsigned int lnsize;
     struct objfile *objfile;
{
  struct cleanup *back_to;
  
  current_objfile = objfile;
  dbbase = xmalloc (dbsize);
  dbroff = 0;
  if ((lseek (desc, dbfoff, 0) != dbfoff) ||
      (read (desc, dbbase, dbsize) != dbsize))
    {
      free (dbbase);
      error ("can't read DWARF data from '%s'", filename);
    }
  back_to = make_cleanup (free, dbbase);
  
  /* If we are reinitializing, or if we have never loaded syms yet, init.
     Since we have no idea how many DIES we are looking at, we just guess
     some arbitrary value. */
  
  if (mainline || objfile -> global_psymbols.size == 0 ||
      objfile -> static_psymbols.size == 0)
    {
      init_psymbol_list (objfile, 1024);
    }
  
  /* Save the relocation factor where everybody can see it.  */

  base_section_offsets = section_offsets;
  baseaddr = ANOFFSET (section_offsets, 0);

  /* Follow the compilation unit sibling chain, building a partial symbol
     table entry for each one.  Save enough information about each compilation
     unit to locate the full DWARF information later. */
  
  scan_compilation_units (filename, dbbase, dbbase + dbsize,
			  dbfoff, lnoffset, objfile);
  
  do_cleanups (back_to);
  current_objfile = NULL;
}


/*

LOCAL FUNCTION

	record_minimal_symbol -- add entry to gdb's minimal symbol table

SYNOPSIS

	static void record_minimal_symbol (char *name, CORE_ADDR address,
					  enum minimal_symbol_type ms_type,
					  struct objfile *objfile)

DESCRIPTION

	Given a pointer to the name of a symbol that should be added to the
	minimal symbol table, and the address associated with that
	symbol, records this information for later use in building the
	minimal symbol table.

 */

static void
record_minimal_symbol (name, address, ms_type, objfile)
     char *name;
     CORE_ADDR address;
     enum minimal_symbol_type ms_type;
     struct objfile *objfile;
{
  name = obsavestring (name, strlen (name), &objfile -> symbol_obstack);
  prim_record_minimal_symbol (name, address, ms_type);
}

/*

LOCAL FUNCTION

	dwarfwarn -- issue a DWARF related warning

DESCRIPTION

	Issue warnings about DWARF related things that aren't serious enough
	to warrant aborting with an error, but should not be ignored either.
	This includes things like detectable corruption in DIE's, missing
	DIE's, unimplemented features, etc.

	In general, running across tags or attributes that we don't recognize
	is not considered to be a problem and we should not issue warnings
	about such.

NOTES

	We mostly follow the example of the error() routine, but without
	returning to command level.  It is arguable about whether warnings
	should be issued at all, and if so, where they should go (stdout or
	stderr).

	We assume that curdie is valid and contains at least the basic
	information for the DIE where the problem was noticed.
*/

static void
dwarfwarn (va_alist)
     va_dcl
{
  va_list ap;
  char *fmt;
  
  va_start (ap);
  fmt = va_arg (ap, char *);
  warning_setup ();
  fprintf (stderr, "warning: DWARF ref 0x%x: ", curdie -> die_ref);
  if (curdie -> at_name)
    {
      fprintf (stderr, "'%s': ", curdie -> at_name);
    }
  vfprintf (stderr, fmt, ap);
  fprintf (stderr, "\n");
  fflush (stderr);
  va_end (ap);
}

/*

LOCAL FUNCTION

	read_lexical_block_scope -- process all dies in a lexical block

SYNOPSIS

	static void read_lexical_block_scope (struct dieinfo *dip,
		char *thisdie, char *enddie)

DESCRIPTION

	Process all the DIES contained within a lexical block scope.
	Start a new scope, process the dies, and then close the scope.

 */

static void
read_lexical_block_scope (dip, thisdie, enddie, objfile)
     struct dieinfo *dip;
     char *thisdie;
     char *enddie;
     struct objfile *objfile;
{
  register struct context_stack *new;

  push_context (0, dip -> at_low_pc);
  process_dies (thisdie + dip -> die_length, enddie, objfile);
  new = pop_context ();
  if (local_symbols != NULL)
    {
      finish_block (0, &local_symbols, new -> old_blocks, new -> start_addr,
		    dip -> at_high_pc, objfile);
    }
  local_symbols = new -> locals;
}

/*

LOCAL FUNCTION

	lookup_utype -- look up a user defined type from die reference

SYNOPSIS

	static type *lookup_utype (DIE_REF die_ref)

DESCRIPTION

	Given a DIE reference, lookup the user defined type associated with
	that DIE, if it has been registered already.  If not registered, then
	return NULL.  Alloc_utype() can be called to register an empty
	type for this reference, which will be filled in later when the
	actual referenced DIE is processed.
 */

static struct type *
lookup_utype (die_ref)
     DIE_REF die_ref;
{
  struct type *type = NULL;
  int utypeidx;
  
  utypeidx = (die_ref - dbroff) / 4;
  if ((utypeidx < 0) || (utypeidx >= numutypes))
    {
      dwarfwarn ("reference to DIE (0x%x) outside compilation unit", die_ref);
    }
  else
    {
      type = *(utypes + utypeidx);
    }
  return (type);
}


/*

LOCAL FUNCTION

	alloc_utype  -- add a user defined type for die reference

SYNOPSIS

	static type *alloc_utype (DIE_REF die_ref, struct type *utypep)

DESCRIPTION

	Given a die reference DIE_REF, and a possible pointer to a user
	defined type UTYPEP, register that this reference has a user
	defined type and either use the specified type in UTYPEP or
	make a new empty type that will be filled in later.

	We should only be called after calling lookup_utype() to verify that
	there is not currently a type registered for DIE_REF.
 */

static struct type *
alloc_utype (die_ref, utypep)
     DIE_REF die_ref;
     struct type *utypep;
{
  struct type **typep;
  int utypeidx;
  
  utypeidx = (die_ref - dbroff) / 4;
  typep = utypes + utypeidx;
  if ((utypeidx < 0) || (utypeidx >= numutypes))
    {
      utypep = lookup_fundamental_type (current_objfile, FT_INTEGER);
      dwarfwarn ("reference to DIE (0x%x) outside compilation unit", die_ref);
    }
  else if (*typep != NULL)
    {
      utypep = *typep;
      SQUAWK (("internal error: dup user type allocation"));
    }
  else
    {
      if (utypep == NULL)
	{
	  utypep = alloc_type (current_objfile);
	}
      *typep = utypep;
    }
  return (utypep);
}

/*

LOCAL FUNCTION

	decode_die_type -- return a type for a specified die

SYNOPSIS

	static struct type *decode_die_type (struct dieinfo *dip)

DESCRIPTION

	Given a pointer to a die information structure DIP, decode the
	type of the die and return a pointer to the decoded type.  All
	dies without specific types default to type int.
 */

static struct type *
decode_die_type (dip)
     struct dieinfo *dip;
{
  struct type *type = NULL;
  
  if (dip -> at_fund_type != 0)
    {
      type = decode_fund_type (dip -> at_fund_type);
    }
  else if (dip -> at_mod_fund_type != NULL)
    {
      type = decode_mod_fund_type (dip -> at_mod_fund_type);
    }
  else if (dip -> at_user_def_type)
    {
      if ((type = lookup_utype (dip -> at_user_def_type)) == NULL)
	{
	  type = alloc_utype (dip -> at_user_def_type, NULL);
	}
    }
  else if (dip -> at_mod_u_d_type)
    {
      type = decode_mod_u_d_type (dip -> at_mod_u_d_type);
    }
  else
    {
      type = lookup_fundamental_type (current_objfile, FT_INTEGER);
    }
  return (type);
}

/*

LOCAL FUNCTION

	struct_type -- compute and return the type for a struct or union

SYNOPSIS

	static struct type *struct_type (struct dieinfo *dip, char *thisdie,
	    char *enddie, struct objfile *objfile)

DESCRIPTION

	Given pointer to a die information structure for a die which
	defines a union or structure (and MUST define one or the other),
	and pointers to the raw die data that define the range of dies which
	define the members, compute and return the user defined type for the
	structure or union.
 */

static struct type *
struct_type (dip, thisdie, enddie, objfile)
     struct dieinfo *dip;
     char *thisdie;
     char *enddie;
     struct objfile *objfile;
{
  struct type *type;
  struct nextfield {
    struct nextfield *next;
    struct field field;
  };
  struct nextfield *list = NULL;
  struct nextfield *new;
  int nfields = 0;
  int n;
  char *tpart1;
  struct dieinfo mbr;
  char *nextdie;
  
  if ((type = lookup_utype (dip -> die_ref)) == NULL)
    {
      /* No forward references created an empty type, so install one now */
      type = alloc_utype (dip -> die_ref, NULL);
    }
  INIT_CPLUS_SPECIFIC(type);
  switch (dip -> die_tag)
    {
      case TAG_structure_type:
        TYPE_CODE (type) = TYPE_CODE_STRUCT;
	tpart1 = "struct";
	break;
      case TAG_union_type:
	TYPE_CODE (type) = TYPE_CODE_UNION;
	tpart1 = "union";
	break;
      default:
	/* Should never happen */
	TYPE_CODE (type) = TYPE_CODE_UNDEF;
	tpart1 = "???";
	SQUAWK (("missing structure or union tag"));
	break;
    }
  /* Some compilers try to be helpful by inventing "fake" names for
     anonymous enums, structures, and unions, like "~0fake" or ".0fake".
     Thanks, but no thanks... */
  if (dip -> at_name != NULL
      && *dip -> at_name != '~'
      && *dip -> at_name != '.')
    {
      TYPE_NAME (type) = obconcat (&objfile -> type_obstack,
				   tpart1, " ", dip -> at_name);
    }
  if (dip -> at_byte_size != 0)
    {
      TYPE_LENGTH (type) = dip -> at_byte_size;
    }
  thisdie += dip -> die_length;
  while (thisdie < enddie)
    {
      basicdieinfo (&mbr, thisdie, objfile);
      completedieinfo (&mbr, objfile);
      if (mbr.die_length <= SIZEOF_DIE_LENGTH)
	{
	  break;
	}
      else if (mbr.at_sibling != 0)
	{
	  nextdie = dbbase + mbr.at_sibling - dbroff;
	}
      else
	{
	  nextdie = thisdie + mbr.die_length;
	}
      switch (mbr.die_tag)
	{
	case TAG_member:
	  /* Get space to record the next field's data.  */
	  new = (struct nextfield *) alloca (sizeof (struct nextfield));
	  new -> next = list;
	  list = new;
	  /* Save the data.  */
	  list -> field.name =
	      obsavestring (mbr.at_name, strlen (mbr.at_name),
			    &objfile -> type_obstack);
	  list -> field.type = decode_die_type (&mbr);
	  list -> field.bitpos = 8 * locval (mbr.at_location);
	  /* Handle bit fields. */
	  list -> field.bitsize = mbr.at_bit_size;
#if BITS_BIG_ENDIAN
	  /* For big endian bits, the at_bit_offset gives the additional
	     bit offset from the MSB of the containing anonymous object to
	     the MSB of the field.  We don't have to do anything special
	     since we don't need to know the size of the anonymous object. */
	  list -> field.bitpos += mbr.at_bit_offset;
#else
	  /* For little endian bits, we need to have a non-zero at_bit_size,
	     so that we know we are in fact dealing with a bitfield.  Compute
	     the bit offset to the MSB of the anonymous object, subtract off
	     the number of bits from the MSB of the field to the MSB of the
	     object, and then subtract off the number of bits of the field
	     itself.  The result is the bit offset of the LSB of the field. */
	  if (mbr.at_bit_size > 0)
	    {
	      list -> field.bitpos +=
		mbr.at_byte_size * 8 - mbr.at_bit_offset - mbr.at_bit_size;
	    }
#endif
	  nfields++;
	  break;
	default:
	  process_dies (thisdie, nextdie, objfile);
	  break;
	}
      thisdie = nextdie;
    }
  /* Now create the vector of fields, and record how big it is.  We may
     not even have any fields, if this DIE was generated due to a reference
     to an anonymous structure or union.  In this case, TYPE_FLAG_STUB is
     set, which clues gdb in to the fact that it needs to search elsewhere
     for the full structure definition. */
  if (nfields == 0)
    {
      TYPE_FLAGS (type) |= TYPE_FLAG_STUB;
    }
  else
    {
      TYPE_NFIELDS (type) = nfields;
      TYPE_FIELDS (type) = (struct field *)
	obstack_alloc (&objfile -> type_obstack,
		       sizeof (struct field) * nfields);
      /* Copy the saved-up fields into the field vector.  */
      for (n = nfields; list; list = list -> next)
	{
	  TYPE_FIELD (type, --n) = list -> field;
	}	
    }
  return (type);
}

/*

LOCAL FUNCTION

	read_structure_scope -- process all dies within struct or union

SYNOPSIS

	static void read_structure_scope (struct dieinfo *dip,
		char *thisdie, char *enddie, struct objfile *objfile)

DESCRIPTION

	Called when we find the DIE that starts a structure or union
	scope (definition) to process all dies that define the members
	of the structure or union.  DIP is a pointer to the die info
	struct for the DIE that names the structure or union.

NOTES

	Note that we need to call struct_type regardless of whether or not
	the DIE has an at_name attribute, since it might be an anonymous
	structure or union.  This gets the type entered into our set of
	user defined types.

	However, if the structure is incomplete (an opaque struct/union)
	then suppress creating a symbol table entry for it since gdb only
	wants to find the one with the complete definition.  Note that if
	it is complete, we just call new_symbol, which does it's own
	checking about whether the struct/union is anonymous or not (and
	suppresses creating a symbol table entry itself).
	
 */

static void
read_structure_scope (dip, thisdie, enddie, objfile)
     struct dieinfo *dip;
     char *thisdie;
     char *enddie;
     struct objfile *objfile;
{
  struct type *type;
  struct symbol *sym;
  
  type = struct_type (dip, thisdie, enddie, objfile);
  if (!(TYPE_FLAGS (type) & TYPE_FLAG_STUB))
    {
      if ((sym = new_symbol (dip, objfile)) != NULL)
	{
	  SYMBOL_TYPE (sym) = type;
	}
    }
}

/*

LOCAL FUNCTION

	decode_array_element_type -- decode type of the array elements

SYNOPSIS

	static struct type *decode_array_element_type (char *scan, char *end)

DESCRIPTION

	As the last step in decoding the array subscript information for an
	array DIE, we need to decode the type of the array elements.  We are
	passed a pointer to this last part of the subscript information and
	must return the appropriate type.  If the type attribute is not
	recognized, just warn about the problem and return type int.
 */

static struct type *
decode_array_element_type (scan)
     char *scan;
{
  struct type *typep;
  DIE_REF die_ref;
  unsigned short attribute;
  unsigned short fundtype;
  int nbytes;
  
  attribute = target_to_host (scan, SIZEOF_ATTRIBUTE, GET_UNSIGNED,
			      current_objfile);
  scan += SIZEOF_ATTRIBUTE;
  if ((nbytes = attribute_size (attribute)) == -1)
    {
      SQUAWK (("bad array element type attribute 0x%x", attribute));
      typep = lookup_fundamental_type (current_objfile, FT_INTEGER);
    }
  else
    {
      switch (attribute)
	{
	  case AT_fund_type:
	    fundtype = target_to_host (scan, nbytes, GET_UNSIGNED,
				       current_objfile);
	    typep = decode_fund_type (fundtype);
	    break;
	  case AT_mod_fund_type:
	    typep = decode_mod_fund_type (scan);
	    break;
	  case AT_user_def_type:
	    die_ref = target_to_host (scan, nbytes, GET_UNSIGNED,
				      current_objfile);
	    if ((typep = lookup_utype (die_ref)) == NULL)
	      {
		typep = alloc_utype (die_ref, NULL);
	      }
	    break;
	  case AT_mod_u_d_type:
	    typep = decode_mod_u_d_type (scan);
	    break;
	  default:
	    SQUAWK (("bad array element type attribute 0x%x", attribute));
	    typep = lookup_fundamental_type (current_objfile, FT_INTEGER);
	    break;
	  }
    }
  return (typep);
}

/*

LOCAL FUNCTION

	decode_subscr_data -- decode array subscript and element type data

SYNOPSIS

	static struct type *decode_subscr_data (char *scan, char *end)

DESCRIPTION

	The array subscripts and the data type of the elements of an
	array are described by a list of data items, stored as a block
	of contiguous bytes.  There is a data item describing each array
	dimension, and a final data item describing the element type.
	The data items are ordered the same as their appearance in the
	source (I.E. leftmost dimension first, next to leftmost second,
	etc).

	We are passed a pointer to the start of the block of bytes
	containing the data items, and a pointer to the first byte past
	the data.  This function decodes the data and returns a type.

BUGS
	FIXME:  This code only implements the forms currently used
	by the AT&T and GNU C compilers.

	The end pointer is supplied for error checking, maybe we should
	use it for that...
 */

static struct type *
decode_subscr_data (scan, end)
     char *scan;
     char *end;
{
  struct type *typep = NULL;
  struct type *nexttype;
  unsigned int format;
  unsigned short fundtype;
  unsigned long lowbound;
  unsigned long highbound;
  int nbytes;
  
  format = target_to_host (scan, SIZEOF_FORMAT_SPECIFIER, GET_UNSIGNED,
			   current_objfile);
  scan += SIZEOF_FORMAT_SPECIFIER;
  switch (format)
    {
    case FMT_ET:
      typep = decode_array_element_type (scan);
      break;
    case FMT_FT_C_C:
      fundtype = target_to_host (scan, SIZEOF_FMT_FT, GET_UNSIGNED,
				 current_objfile);
      scan += SIZEOF_FMT_FT;
      if (fundtype != FT_integer && fundtype != FT_signed_integer
	  && fundtype != FT_unsigned_integer)
	{
	  SQUAWK (("array subscripts must be integral types, not type 0x%x",
		   fundtype));
	}
      else
	{
	  nbytes = TARGET_FT_LONG_SIZE (current_objfile);
	  lowbound = target_to_host (scan, nbytes, GET_UNSIGNED,
				     current_objfile);
	  scan += nbytes;
	  highbound = target_to_host (scan, nbytes, GET_UNSIGNED,
				      current_objfile);
	  scan += nbytes;
	  nexttype = decode_subscr_data (scan, end);
	  if (nexttype != NULL)
	    {
	      typep = alloc_type (current_objfile);
	      TYPE_CODE (typep) = TYPE_CODE_ARRAY;
	      TYPE_LENGTH (typep) = TYPE_LENGTH (nexttype);
	      TYPE_LENGTH (typep) *= (highbound - lowbound) + 1;
	      TYPE_TARGET_TYPE (typep) = nexttype;
	    }		    
	}
      break;
    case FMT_FT_C_X:
    case FMT_FT_X_C:
    case FMT_FT_X_X:
    case FMT_UT_C_C:
    case FMT_UT_C_X:
    case FMT_UT_X_C:
    case FMT_UT_X_X:
      SQUAWK (("array subscript format 0x%x not handled yet", format));
      break;
    default:
      SQUAWK (("unknown array subscript format %x", format));
      break;
    }
  return (typep);
}

/*

LOCAL FUNCTION

	dwarf_read_array_type -- read TAG_array_type DIE

SYNOPSIS

	static void dwarf_read_array_type (struct dieinfo *dip)

DESCRIPTION

	Extract all information from a TAG_array_type DIE and add to
	the user defined type vector.
 */

static void
dwarf_read_array_type (dip)
     struct dieinfo *dip;
{
  struct type *type;
  struct type *utype;
  char *sub;
  char *subend;
  unsigned short blocksz;
  int nbytes;
  
  if (dip -> at_ordering != ORD_row_major)
    {
      /* FIXME:  Can gdb even handle column major arrays? */
      SQUAWK (("array not row major; not handled correctly"));
    }
  if ((sub = dip -> at_subscr_data) != NULL)
    {
      nbytes = attribute_size (AT_subscr_data);
      blocksz = target_to_host (sub, nbytes, GET_UNSIGNED, current_objfile);
      subend = sub + nbytes + blocksz;
      sub += nbytes;
      type = decode_subscr_data (sub, subend);
      if (type == NULL)
	{
	  if ((utype = lookup_utype (dip -> die_ref)) == NULL)
	    {
	      utype = alloc_utype (dip -> die_ref, NULL);
	    }
	  TYPE_CODE (utype) = TYPE_CODE_ARRAY;
	  TYPE_TARGET_TYPE (utype) = 
      	    lookup_fundamental_type (current_objfile, FT_INTEGER);
	  TYPE_LENGTH (utype) = 1 * TYPE_LENGTH (TYPE_TARGET_TYPE (utype));
	}
      else
	{
	  if ((utype = lookup_utype (dip -> die_ref)) == NULL)
	    {
	      alloc_utype (dip -> die_ref, type);
	    }
	  else
	    {
	      TYPE_CODE (utype) = TYPE_CODE_ARRAY;
	      TYPE_LENGTH (utype) = TYPE_LENGTH (type);
	      TYPE_TARGET_TYPE (utype) = TYPE_TARGET_TYPE (type);
	    }
	}
    }
}

/*

LOCAL FUNCTION

	read_tag_pointer_type -- read TAG_pointer_type DIE

SYNOPSIS

	static void read_tag_pointer_type (struct dieinfo *dip)

DESCRIPTION

	Extract all information from a TAG_pointer_type DIE and add to
	the user defined type vector.
 */

static void
read_tag_pointer_type (dip)
     struct dieinfo *dip;
{
  struct type *type;
  struct type *utype;
  
  type = decode_die_type (dip);
  if ((utype = lookup_utype (dip -> die_ref)) == NULL)
    {
      utype = lookup_pointer_type (type);
      alloc_utype (dip -> die_ref, utype);
    }
  else
    {
      TYPE_TARGET_TYPE (utype) = type;
      TYPE_POINTER_TYPE (type) = utype;

      /* We assume the machine has only one representation for pointers!  */
      /* FIXME:  This confuses host<->target data representations, and is a
	 poor assumption besides. */
      
      TYPE_LENGTH (utype) = sizeof (char *);
      TYPE_CODE (utype) = TYPE_CODE_PTR;
    }
}

/*

LOCAL FUNCTION

	read_subroutine_type -- process TAG_subroutine_type dies

SYNOPSIS

	static void read_subroutine_type (struct dieinfo *dip, char thisdie,
		char *enddie)

DESCRIPTION

	Handle DIES due to C code like:

	struct foo {
	    int (*funcp)(int a, long l);  (Generates TAG_subroutine_type DIE)
	    int b;
	};

NOTES

	The parameter DIES are currently ignored.  See if gdb has a way to
	include this info in it's type system, and decode them if so.  Is
	this what the type structure's "arg_types" field is for?  (FIXME)
 */

static void
read_subroutine_type (dip, thisdie, enddie)
     struct dieinfo *dip;
     char *thisdie;
     char *enddie;
{
  struct type *type;		/* Type that this function returns */
  struct type *ftype;		/* Function that returns above type */
  
  /* Decode the type that this subroutine returns */

  type = decode_die_type (dip);

  /* Check to see if we already have a partially constructed user
     defined type for this DIE, from a forward reference. */

  if ((ftype = lookup_utype (dip -> die_ref)) == NULL)
    {
      /* This is the first reference to one of these types.  Make
	 a new one and place it in the user defined types. */
      ftype = lookup_function_type (type);
      alloc_utype (dip -> die_ref, ftype);
    }
  else
    {
      /* We have an existing partially constructed type, so bash it
	 into the correct type. */
      TYPE_TARGET_TYPE (ftype) = type;
      TYPE_FUNCTION_TYPE (type) = ftype;
      TYPE_LENGTH (ftype) = 1;
      TYPE_CODE (ftype) = TYPE_CODE_FUNC;
    }
}

/*

LOCAL FUNCTION

	read_enumeration -- process dies which define an enumeration

SYNOPSIS

	static void read_enumeration (struct dieinfo *dip, char *thisdie,
		char *enddie, struct objfile *objfile)

DESCRIPTION

	Given a pointer to a die which begins an enumeration, process all
	the dies that define the members of the enumeration.

NOTES

	Note that we need to call enum_type regardless of whether or not we
	have a symbol, since we might have an enum without a tag name (thus
	no symbol for the tagname).
 */

static void
read_enumeration (dip, thisdie, enddie, objfile)
     struct dieinfo *dip;
     char *thisdie;
     char *enddie;
     struct objfile *objfile;
{
  struct type *type;
  struct symbol *sym;
  
  type = enum_type (dip, objfile);
  if ((sym = new_symbol (dip, objfile)) != NULL)
    {
      SYMBOL_TYPE (sym) = type;
    }
}

/*

LOCAL FUNCTION

	enum_type -- decode and return a type for an enumeration

SYNOPSIS

	static type *enum_type (struct dieinfo *dip, struct objfile *objfile)

DESCRIPTION

	Given a pointer to a die information structure for the die which
	starts an enumeration, process all the dies that define the members
	of the enumeration and return a type pointer for the enumeration.

	At the same time, for each member of the enumeration, create a
	symbol for it with namespace VAR_NAMESPACE and class LOC_CONST,
	and give it the type of the enumeration itself.

NOTES

	Note that the DWARF specification explicitly mandates that enum
	constants occur in reverse order from the source program order,
	for "consistency" and because this ordering is easier for many
	compilers to generate. (Draft 6, sec 3.8.5, Enumeration type
	Entries).  Because gdb wants to see the enum members in program
	source order, we have to ensure that the order gets reversed while
	we are processing them.
 */

static struct type *
enum_type (dip, objfile)
     struct dieinfo *dip;
     struct objfile *objfile;
{
  struct type *type;
  struct nextfield {
    struct nextfield *next;
    struct field field;
  };
  struct nextfield *list = NULL;
  struct nextfield *new;
  int nfields = 0;
  int n;
  char *scan;
  char *listend;
  unsigned short blocksz;
  struct symbol *sym;
  int nbytes;
  
  if ((type = lookup_utype (dip -> die_ref)) == NULL)
    {
      /* No forward references created an empty type, so install one now */
      type = alloc_utype (dip -> die_ref, NULL);
    }
  TYPE_CODE (type) = TYPE_CODE_ENUM;
  /* Some compilers try to be helpful by inventing "fake" names for
     anonymous enums, structures, and unions, like "~0fake" or ".0fake".
     Thanks, but no thanks... */
  if (dip -> at_name != NULL
      && *dip -> at_name != '~'
      && *dip -> at_name != '.')
    {
      TYPE_NAME (type) = obconcat (&objfile -> type_obstack, "enum",
				   " ", dip -> at_name);
    }
  if (dip -> at_byte_size != 0)
    {
      TYPE_LENGTH (type) = dip -> at_byte_size;
    }
  if ((scan = dip -> at_element_list) != NULL)
    {
      if (dip -> short_element_list)
	{
	  nbytes = attribute_size (AT_short_element_list);
	}
      else
	{
	  nbytes = attribute_size (AT_element_list);
	}
      blocksz = target_to_host (scan, nbytes, GET_UNSIGNED, objfile);
      listend = scan + nbytes + blocksz;
      scan += nbytes;
      while (scan < listend)
	{
	  new = (struct nextfield *) alloca (sizeof (struct nextfield));
	  new -> next = list;
	  list = new;
	  list -> field.type = NULL;
	  list -> field.bitsize = 0;
	  list -> field.bitpos =
	    target_to_host (scan, TARGET_FT_LONG_SIZE (objfile), GET_SIGNED,
			    objfile);
	  scan += TARGET_FT_LONG_SIZE (objfile);
	  list -> field.name = obsavestring (scan, strlen (scan),
					     &objfile -> type_obstack);
	  scan += strlen (scan) + 1;
	  nfields++;
	  /* Handcraft a new symbol for this enum member. */
	  sym = (struct symbol *) obstack_alloc (&objfile->symbol_obstack,
						 sizeof (struct symbol));
	  memset (sym, 0, sizeof (struct symbol));
	  SYMBOL_NAME (sym) = create_name (list -> field.name,
					   &objfile->symbol_obstack);
	  SYMBOL_NAMESPACE (sym) = VAR_NAMESPACE;
	  SYMBOL_CLASS (sym) = LOC_CONST;
	  SYMBOL_TYPE (sym) = type;
	  SYMBOL_VALUE (sym) = list -> field.bitpos;
	  add_symbol_to_list (sym, list_in_scope);
	}
      /* Now create the vector of fields, and record how big it is. This is
	 where we reverse the order, by pulling the members off the list in
	 reverse order from how they were inserted.  If we have no fields
	 (this is apparently possible in C++) then skip building a field
	 vector. */
      if (nfields > 0)
	{
	  TYPE_NFIELDS (type) = nfields;
	  TYPE_FIELDS (type) = (struct field *)
	    obstack_alloc (&objfile->symbol_obstack, sizeof (struct field) * nfields);
	  /* Copy the saved-up fields into the field vector.  */
	  for (n = 0; (n < nfields) && (list != NULL); list = list -> next)
	    {
	      TYPE_FIELD (type, n++) = list -> field;
	    }	
	}
    }
  return (type);
}

/*

LOCAL FUNCTION

	read_func_scope -- process all dies within a function scope

DESCRIPTION

	Process all dies within a given function scope.  We are passed
	a die information structure pointer DIP for the die which
	starts the function scope, and pointers into the raw die data
	that define the dies within the function scope.

	For now, we ignore lexical block scopes within the function.
	The problem is that AT&T cc does not define a DWARF lexical
	block scope for the function itself, while gcc defines a
	lexical block scope for the function.  We need to think about
	how to handle this difference, or if it is even a problem.
	(FIXME)
 */

static void
read_func_scope (dip, thisdie, enddie, objfile)
     struct dieinfo *dip;
     char *thisdie;
     char *enddie;
     struct objfile *objfile;
{
  register struct context_stack *new;
  
  if (objfile -> ei.entry_point >= dip -> at_low_pc &&
      objfile -> ei.entry_point <  dip -> at_high_pc)
    {
      objfile -> ei.entry_func_lowpc = dip -> at_low_pc;
      objfile -> ei.entry_func_highpc = dip -> at_high_pc;
    }
  if (STREQ (dip -> at_name, "main"))	/* FIXME: hardwired name */
    {
      objfile -> ei.main_func_lowpc = dip -> at_low_pc;
      objfile -> ei.main_func_highpc = dip -> at_high_pc;
    }
  new = push_context (0, dip -> at_low_pc);
  new -> name = new_symbol (dip, objfile);
  list_in_scope = &local_symbols;
  process_dies (thisdie + dip -> die_length, enddie, objfile);
  new = pop_context ();
  /* Make a block for the local symbols within.  */
  finish_block (new -> name, &local_symbols, new -> old_blocks,
		new -> start_addr, dip -> at_high_pc, objfile);
  list_in_scope = &file_symbols;
}


/*

LOCAL FUNCTION

	handle_producer -- process the AT_producer attribute

DESCRIPTION

	Perform any operations that depend on finding a particular
	AT_producer attribute.

 */

static void
handle_producer (producer)
     char *producer;
{

  /* If this compilation unit was compiled with g++ or gcc, then set the
     processing_gcc_compilation flag. */

  processing_gcc_compilation =
    STREQN (producer, GPLUS_PRODUCER, strlen (GPLUS_PRODUCER))
      || STREQN (producer, GCC_PRODUCER, strlen (GCC_PRODUCER));

  /* Select a demangling style if we can identify the producer and if
     the current style is auto.  We leave the current style alone if it
     is not auto.  We also leave the demangling style alone if we find a
     gcc (cc1) producer, as opposed to a g++ (cc1plus) producer. */

#if 0 /* Works, but is disabled for now.  -fnf */
  if (current_demangling_style == auto_demangling)
    {
      if (STREQN (producer, GPLUS_PRODUCER, strlen (GPLUS_PRODUCER)))
	{
	  set_demangling_style (GNU_DEMANGLING_STYLE_STRING);
	}
      else if (STREQN (producer, LCC_PRODUCER, strlen (LCC_PRODUCER)))
	{
	  set_demangling_style (LUCID_DEMANGLING_STYLE_STRING);
	}
      else if (STREQN (producer, CFRONT_PRODUCER, strlen (CFRONT_PRODUCER)))
	{
	  set_demangling_style (CFRONT_DEMANGLING_STYLE_STRING);
	}
    }
#endif
}


/*

LOCAL FUNCTION

	read_file_scope -- process all dies within a file scope

DESCRIPTION

	Process all dies within a given file scope.  We are passed a
	pointer to the die information structure for the die which
	starts the file scope, and pointers into the raw die data which
	mark the range of dies within the file scope.

	When the partial symbol table is built, the file offset for the line
	number table for each compilation unit is saved in the partial symbol
	table entry for that compilation unit.  As the symbols for each
	compilation unit are read, the line number table is read into memory
	and the variable lnbase is set to point to it.  Thus all we have to
	do is use lnbase to access the line number table for the current
	compilation unit.
 */

static void
read_file_scope (dip, thisdie, enddie, objfile)
     struct dieinfo *dip;
     char *thisdie;
     char *enddie;
     struct objfile *objfile;
{
  struct cleanup *back_to;
  struct symtab *symtab;
  
  if (objfile -> ei.entry_point >= dip -> at_low_pc &&
      objfile -> ei.entry_point <  dip -> at_high_pc)
    {
      objfile -> ei.entry_file_lowpc = dip -> at_low_pc;
      objfile -> ei.entry_file_highpc = dip -> at_high_pc;
    }
  if (dip -> at_producer != NULL)
    {
      handle_producer (dip -> at_producer);
    }
  numutypes = (enddie - thisdie) / 4;
  utypes = (struct type **) xmalloc (numutypes * sizeof (struct type *));
  back_to = make_cleanup (free, utypes);
  memset (utypes, 0, numutypes * sizeof (struct type *));
  start_symtab (dip -> at_name, NULL, dip -> at_low_pc);
  decode_line_numbers (lnbase);
  process_dies (thisdie + dip -> die_length, enddie, objfile);
  symtab = end_symtab (dip -> at_high_pc, 0, 0, objfile);
  /* FIXME:  The following may need to be expanded for other languages */
  switch (dip -> at_language)
    {
      case LANG_C89:
      case LANG_C:
	symtab -> language = language_c;
	break;
      case LANG_C_PLUS_PLUS:
	symtab -> language = language_cplus;
	break;
      default:
	;
    }
  do_cleanups (back_to);
  utypes = NULL;
  numutypes = 0;
}

/*

LOCAL FUNCTION

	process_dies -- process a range of DWARF Information Entries

SYNOPSIS

	static void process_dies (char *thisdie, char *enddie,
				  struct objfile *objfile)

DESCRIPTION

	Process all DIE's in a specified range.  May be (and almost
	certainly will be) called recursively.
 */

static void
process_dies (thisdie, enddie, objfile)
     char *thisdie;
     char *enddie;
     struct objfile *objfile;
{
  char *nextdie;
  struct dieinfo di;
  
  while (thisdie < enddie)
    {
      basicdieinfo (&di, thisdie, objfile);
      if (di.die_length < SIZEOF_DIE_LENGTH)
	{
	  break;
	}
      else if (di.die_tag == TAG_padding)
	{
	  nextdie = thisdie + di.die_length;
	}
      else
	{
	  completedieinfo (&di, objfile);
	  if (di.at_sibling != 0)
	    {
	      nextdie = dbbase + di.at_sibling - dbroff;
	    }
	  else
	    {
	      nextdie = thisdie + di.die_length;
	    }
	  switch (di.die_tag)
	    {
	    case TAG_compile_unit:
	      read_file_scope (&di, thisdie, nextdie, objfile);
	      break;
	    case TAG_global_subroutine:
	    case TAG_subroutine:
	      if (di.has_at_low_pc)
		{
		  read_func_scope (&di, thisdie, nextdie, objfile);
		}
	      break;
	    case TAG_lexical_block:
	      read_lexical_block_scope (&di, thisdie, nextdie, objfile);
	      break;
	    case TAG_structure_type:
	    case TAG_union_type:
	      read_structure_scope (&di, thisdie, nextdie, objfile);
	      break;
	    case TAG_enumeration_type:
	      read_enumeration (&di, thisdie, nextdie, objfile);
	      break;
	    case TAG_subroutine_type:
	      read_subroutine_type (&di, thisdie, nextdie);
	      break;
	    case TAG_array_type:
	      dwarf_read_array_type (&di);
	      break;
	    case TAG_pointer_type:
	      read_tag_pointer_type (&di);
	      break;
	    default:
	      new_symbol (&di, objfile);
	      break;
	    }
	}
      thisdie = nextdie;
    }
}

/*

LOCAL FUNCTION

	decode_line_numbers -- decode a line number table fragment

SYNOPSIS

	static void decode_line_numbers (char *tblscan, char *tblend,
		long length, long base, long line, long pc)

DESCRIPTION

	Translate the DWARF line number information to gdb form.

	The ".line" section contains one or more line number tables, one for
	each ".line" section from the objects that were linked.

	The AT_stmt_list attribute for each TAG_source_file entry in the
	".debug" section contains the offset into the ".line" section for the
	start of the table for that file.

	The table itself has the following structure:

	<table length><base address><source statement entry>
	4 bytes       4 bytes       10 bytes

	The table length is the total size of the table, including the 4 bytes
	for the length information.

	The base address is the address of the first instruction generated
	for the source file.

	Each source statement entry has the following structure:

	<line number><statement position><address delta>
	4 bytes      2 bytes             4 bytes

	The line number is relative to the start of the file, starting with
	line 1.

	The statement position either -1 (0xFFFF) or the number of characters
	from the beginning of the line to the beginning of the statement.

	The address delta is the difference between the base address and
	the address of the first instruction for the statement.

	Note that we must copy the bytes from the packed table to our local
	variables before attempting to use them, to avoid alignment problems
	on some machines, particularly RISC processors.

BUGS

	Does gdb expect the line numbers to be sorted?  They are now by
	chance/luck, but are not required to be.  (FIXME)

	The line with number 0 is unused, gdb apparently can discover the
	span of the last line some other way. How?  (FIXME)
 */

static void
decode_line_numbers (linetable)
     char *linetable;
{
  char *tblscan;
  char *tblend;
  unsigned long length;
  unsigned long base;
  unsigned long line;
  unsigned long pc;
  
  if (linetable != NULL)
    {
      tblscan = tblend = linetable;
      length = target_to_host (tblscan, SIZEOF_LINETBL_LENGTH, GET_UNSIGNED,
			       current_objfile);
      tblscan += SIZEOF_LINETBL_LENGTH;
      tblend += length;
      base = target_to_host (tblscan, TARGET_FT_POINTER_SIZE (objfile),
			     GET_UNSIGNED, current_objfile);
      tblscan += TARGET_FT_POINTER_SIZE (objfile);
      base += baseaddr;
      while (tblscan < tblend)
	{
	  line = target_to_host (tblscan, SIZEOF_LINETBL_LINENO, GET_UNSIGNED,
				 current_objfile);
	  tblscan += SIZEOF_LINETBL_LINENO + SIZEOF_LINETBL_STMT;
	  pc = target_to_host (tblscan, SIZEOF_LINETBL_DELTA, GET_UNSIGNED,
			       current_objfile);
	  tblscan += SIZEOF_LINETBL_DELTA;
	  pc += base;
	  if (line != 0)
	    {
	      record_line (current_subfile, line, pc);
	    }
	}
    }
}

/*

LOCAL FUNCTION

	locval -- compute the value of a location attribute

SYNOPSIS

	static int locval (char *loc)

DESCRIPTION

	Given pointer to a string of bytes that define a location, compute
	the location and return the value.

	When computing values involving the current value of the frame pointer,
	the value zero is used, which results in a value relative to the frame
	pointer, rather than the absolute value.  This is what GDB wants
	anyway.
    
	When the result is a register number, the global isreg flag is set,
	otherwise it is cleared.  This is a kludge until we figure out a better
	way to handle the problem.  Gdb's design does not mesh well with the
	DWARF notion of a location computing interpreter, which is a shame
	because the flexibility goes unused.

NOTES

	Note that stack[0] is unused except as a default error return.
	Note that stack overflow is not yet handled.
 */

static int
locval (loc)
     char *loc;
{
  unsigned short nbytes;
  unsigned short locsize;
  auto long stack[64];
  int stacki;
  char *end;
  long regno;
  int loc_atom_code;
  int loc_value_size;
  
  nbytes = attribute_size (AT_location);
  locsize = target_to_host (loc, nbytes, GET_UNSIGNED, current_objfile);
  loc += nbytes;
  end = loc + locsize;
  stacki = 0;
  stack[stacki] = 0;
  isreg = 0;
  offreg = 0;
  loc_value_size = TARGET_FT_LONG_SIZE (current_objfile);
  while (loc < end)
    {
      loc_atom_code = target_to_host (loc, SIZEOF_LOC_ATOM_CODE, GET_UNSIGNED,
				      current_objfile);
      loc += SIZEOF_LOC_ATOM_CODE;
      switch (loc_atom_code)
	{
	  case 0:
	    /* error */
	    loc = end;
	    break;
	  case OP_REG:
	    /* push register (number) */
	    stack[++stacki] = target_to_host (loc, loc_value_size,
					      GET_UNSIGNED, current_objfile);
	    loc += loc_value_size;
	    isreg = 1;
	    break;
	  case OP_BASEREG:
	    /* push value of register (number) */
	    /* Actually, we compute the value as if register has 0 */
	    offreg = 1;
	    regno = target_to_host (loc, loc_value_size, GET_UNSIGNED,
				    current_objfile);
	    loc += loc_value_size;
	    if (regno == R_FP)
	      {
		stack[++stacki] = 0;
	      }
	    else
	      {
		stack[++stacki] = 0;
		SQUAWK (("BASEREG %d not handled!", regno));
	      }
	    break;
	  case OP_ADDR:
	    /* push address (relocated address) */
	    stack[++stacki] = target_to_host (loc, loc_value_size,
					      GET_UNSIGNED, current_objfile);
	    loc += loc_value_size;
	    break;
	  case OP_CONST:
	    /* push constant (number)   FIXME: signed or unsigned! */
	    stack[++stacki] = target_to_host (loc, loc_value_size,
					      GET_SIGNED, current_objfile);
	    loc += loc_value_size;
	    break;
	  case OP_DEREF2:
	    /* pop, deref and push 2 bytes (as a long) */
	    SQUAWK (("OP_DEREF2 address 0x%x not handled", stack[stacki]));
	    break;
	  case OP_DEREF4:	/* pop, deref and push 4 bytes (as a long) */
	    SQUAWK (("OP_DEREF4 address 0x%x not handled", stack[stacki]));
	    break;
	  case OP_ADD:	/* pop top 2 items, add, push result */
	    stack[stacki - 1] += stack[stacki];
	    stacki--;
	    break;
	}
    }
  return (stack[stacki]);
}

/*

LOCAL FUNCTION

	read_ofile_symtab -- build a full symtab entry from chunk of DIE's

SYNOPSIS

	static struct symtab *read_ofile_symtab (struct partial_symtab *pst)

DESCRIPTION

	When expanding a partial symbol table entry to a full symbol table
	entry, this is the function that gets called to read in the symbols
	for the compilation unit.

	Returns a pointer to the newly constructed symtab (which is now
	the new first one on the objfile's symtab list).
 */

static struct symtab *
read_ofile_symtab (pst)
     struct partial_symtab *pst;
{
  struct cleanup *back_to;
  unsigned long lnsize;
  int foffset;
  bfd *abfd;
  char lnsizedata[SIZEOF_LINETBL_LENGTH];

  abfd = pst -> objfile -> obfd;
  current_objfile = pst -> objfile;

  /* Allocate a buffer for the entire chunk of DIE's for this compilation
     unit, seek to the location in the file, and read in all the DIE's. */

  diecount = 0;
  dbbase = xmalloc (DBLENGTH(pst));
  dbroff = DBROFF(pst);
  foffset = DBFOFF(pst) + dbroff;
  base_section_offsets = pst->section_offsets;
  baseaddr = ANOFFSET (pst->section_offsets, 0);
  if (bfd_seek (abfd, foffset, 0) ||
      (bfd_read (dbbase, DBLENGTH(pst), 1, abfd) != DBLENGTH(pst)))
    {
      free (dbbase);
      error ("can't read DWARF data");
    }
  back_to = make_cleanup (free, dbbase);

  /* If there is a line number table associated with this compilation unit
     then read the size of this fragment in bytes, from the fragment itself.
     Allocate a buffer for the fragment and read it in for future 
     processing. */

  lnbase = NULL;
  if (LNFOFF (pst))
    {
      if (bfd_seek (abfd, LNFOFF (pst), 0) ||
	  (bfd_read ((PTR) lnsizedata, sizeof (lnsizedata), 1, abfd) !=
	   sizeof (lnsizedata)))
	{
	  error ("can't read DWARF line number table size");
	}
      lnsize = target_to_host (lnsizedata, SIZEOF_LINETBL_LENGTH,
			       GET_UNSIGNED, pst -> objfile);
      lnbase = xmalloc (lnsize);
      if (bfd_seek (abfd, LNFOFF (pst), 0) ||
	  (bfd_read (lnbase, lnsize, 1, abfd) != lnsize))
	{
	  free (lnbase);
	  error ("can't read DWARF line numbers");
	}
      make_cleanup (free, lnbase);
    }

  process_dies (dbbase, dbbase + DBLENGTH(pst), pst -> objfile);
  do_cleanups (back_to);
  current_objfile = NULL;
  return (pst -> objfile -> symtabs);
}

/*

LOCAL FUNCTION

	psymtab_to_symtab_1 -- do grunt work for building a full symtab entry

SYNOPSIS

	static void psymtab_to_symtab_1 (struct partial_symtab *pst)

DESCRIPTION

	Called once for each partial symbol table entry that needs to be
	expanded into a full symbol table entry.

*/

static void
psymtab_to_symtab_1 (pst)
     struct partial_symtab *pst;
{
  int i;
  
  if (pst != NULL)
    {
      if (pst->readin)
	{
	  warning ("psymtab for %s already read in.  Shouldn't happen.",
		   pst -> filename);
	}
      else
	{
	  /* Read in all partial symtabs on which this one is dependent */
	  for (i = 0; i < pst -> number_of_dependencies; i++)
	    {
	      if (!pst -> dependencies[i] -> readin)
		{
		  /* Inform about additional files that need to be read in. */
		  if (info_verbose)
		    {
		      fputs_filtered (" ", stdout);
		      wrap_here ("");
		      fputs_filtered ("and ", stdout);
		      wrap_here ("");
		      printf_filtered ("%s...",
				       pst -> dependencies[i] -> filename);
		      wrap_here ("");
		      fflush (stdout);		/* Flush output */
		    }
		  psymtab_to_symtab_1 (pst -> dependencies[i]);
		}
	    }	  
	  if (DBLENGTH (pst))		/* Otherwise it's a dummy */
	    {
	      pst -> symtab = read_ofile_symtab (pst);
	      if (info_verbose)
		{
		  printf_filtered ("%d DIE's, sorting...", diecount);
		  wrap_here ("");
		  fflush (stdout);
		}
	      sort_symtab_syms (pst -> symtab);
	    }
	  pst -> readin = 1;
	}
    }
}

/*

LOCAL FUNCTION

	dwarf_psymtab_to_symtab -- build a full symtab entry from partial one

SYNOPSIS

	static void dwarf_psymtab_to_symtab (struct partial_symtab *pst)

DESCRIPTION

	This is the DWARF support entry point for building a full symbol
	table entry from a partial symbol table entry.  We are passed a
	pointer to the partial symbol table entry that needs to be expanded.

*/

static void
dwarf_psymtab_to_symtab (pst)
     struct partial_symtab *pst;
{

  if (pst != NULL)
    {
      if (pst -> readin)
	{
	  warning ("psymtab for %s already read in.  Shouldn't happen.",
		   pst -> filename);
	}
      else
	{
	  if (DBLENGTH (pst) || pst -> number_of_dependencies)
	    {
	      /* Print the message now, before starting serious work, to avoid
		 disconcerting pauses.  */
	      if (info_verbose)
		{
		  printf_filtered ("Reading in symbols for %s...",
				   pst -> filename);
		  fflush (stdout);
		}
	      
	      psymtab_to_symtab_1 (pst);
	      
#if 0	      /* FIXME:  Check to see what dbxread is doing here and see if
		 we need to do an equivalent or is this something peculiar to
		 stabs/a.out format.
		 Match with global symbols.  This only needs to be done once,
		 after all of the symtabs and dependencies have been read in.
		 */
	      scan_file_globals (pst -> objfile);
#endif
	      
	      /* Finish up the verbose info message.  */
	      if (info_verbose)
		{
		  printf_filtered ("done.\n");
		  fflush (stdout);
		}
	    }
	}
    }
}

/*

LOCAL FUNCTION

	init_psymbol_list -- initialize storage for partial symbols

SYNOPSIS

	static void init_psymbol_list (struct objfile *objfile, int total_symbols)

DESCRIPTION

	Initializes storage for all of the partial symbols that will be
	created by dwarf_build_psymtabs and subsidiaries.
 */

static void
init_psymbol_list (objfile, total_symbols)
     struct objfile *objfile;
     int total_symbols;
{
  /* Free any previously allocated psymbol lists.  */
  
  if (objfile -> global_psymbols.list)
    {
      mfree (objfile -> md, (PTR)objfile -> global_psymbols.list);
    }
  if (objfile -> static_psymbols.list)
    {
      mfree (objfile -> md, (PTR)objfile -> static_psymbols.list);
    }
  
  /* Current best guess is that there are approximately a twentieth
     of the total symbols (in a debugging file) are global or static
     oriented symbols */
  
  objfile -> global_psymbols.size = total_symbols / 10;
  objfile -> static_psymbols.size = total_symbols / 10;
  objfile -> global_psymbols.next =
    objfile -> global_psymbols.list = (struct partial_symbol *)
      xmmalloc (objfile -> md, objfile -> global_psymbols.size
			     * sizeof (struct partial_symbol));
  objfile -> static_psymbols.next =
    objfile -> static_psymbols.list = (struct partial_symbol *)
      xmmalloc (objfile -> md, objfile -> static_psymbols.size
			     * sizeof (struct partial_symbol));
}

/*

LOCAL FUNCTION

	add_enum_psymbol -- add enumeration members to partial symbol table

DESCRIPTION

	Given pointer to a DIE that is known to be for an enumeration,
	extract the symbolic names of the enumeration members and add
	partial symbols for them.
*/

static void
add_enum_psymbol (dip, objfile)
     struct dieinfo *dip;
     struct objfile *objfile;
{
  char *scan;
  char *listend;
  unsigned short blocksz;
  int nbytes;
  
  if ((scan = dip -> at_element_list) != NULL)
    {
      if (dip -> short_element_list)
	{
	  nbytes = attribute_size (AT_short_element_list);
	}
      else
	{
	  nbytes = attribute_size (AT_element_list);
	}
      blocksz = target_to_host (scan, nbytes, GET_UNSIGNED, objfile);
      scan += nbytes;
      listend = scan + blocksz;
      while (scan < listend)
	{
	  scan += TARGET_FT_LONG_SIZE (objfile);
	  ADD_PSYMBOL_TO_LIST (scan, strlen (scan), VAR_NAMESPACE, LOC_CONST,
			       objfile -> static_psymbols, 0);
	  scan += strlen (scan) + 1;
	}
    }
}

/*

LOCAL FUNCTION

	add_partial_symbol -- add symbol to partial symbol table

DESCRIPTION

	Given a DIE, if it is one of the types that we want to
	add to a partial symbol table, finish filling in the die info
	and then add a partial symbol table entry for it.

*/

static void
add_partial_symbol (dip, objfile)
     struct dieinfo *dip;
     struct objfile *objfile;
{
  switch (dip -> die_tag)
    {
    case TAG_global_subroutine:
      record_minimal_symbol (dip -> at_name, dip -> at_low_pc, mst_text,
			    objfile);
      ADD_PSYMBOL_TO_LIST (dip -> at_name, strlen (dip -> at_name),
			   VAR_NAMESPACE, LOC_BLOCK,
			   objfile -> global_psymbols,
			   dip -> at_low_pc);
      break;
    case TAG_global_variable:
      record_minimal_symbol (dip -> at_name, locval (dip -> at_location),
			    mst_data, objfile);
      ADD_PSYMBOL_TO_LIST (dip -> at_name, strlen (dip -> at_name),
			   VAR_NAMESPACE, LOC_STATIC,
			   objfile -> global_psymbols,
			   0);
      break;
    case TAG_subroutine:
      ADD_PSYMBOL_TO_LIST (dip -> at_name, strlen (dip -> at_name),
			   VAR_NAMESPACE, LOC_BLOCK,
			   objfile -> static_psymbols,
			   dip -> at_low_pc);
      break;
    case TAG_local_variable:
      ADD_PSYMBOL_TO_LIST (dip -> at_name, strlen (dip -> at_name),
			   VAR_NAMESPACE, LOC_STATIC,
			   objfile -> static_psymbols,
			   0);
      break;
    case TAG_typedef:
      ADD_PSYMBOL_TO_LIST (dip -> at_name, strlen (dip -> at_name),
			   VAR_NAMESPACE, LOC_TYPEDEF,
			   objfile -> static_psymbols,
			   0);
      break;
    case TAG_structure_type:
    case TAG_union_type:
      ADD_PSYMBOL_TO_LIST (dip -> at_name, strlen (dip -> at_name),
			   STRUCT_NAMESPACE, LOC_TYPEDEF,
			   objfile -> static_psymbols,
			   0);
      break;
    case TAG_enumeration_type:
      if (dip -> at_name)
	{
	  ADD_PSYMBOL_TO_LIST (dip -> at_name, strlen (dip -> at_name),
			       STRUCT_NAMESPACE, LOC_TYPEDEF,
			       objfile -> static_psymbols,
			       0);
	}
      add_enum_psymbol (dip, objfile);
      break;
    }
}

/*

LOCAL FUNCTION

	scan_partial_symbols -- scan DIE's within a single compilation unit

DESCRIPTION

	Process the DIE's within a single compilation unit, looking for
	interesting DIE's that contribute to the partial symbol table entry
	for this compilation unit.  Since we cannot follow any sibling
	chains without reading the complete DIE info for every DIE,
	it is probably faster to just sequentially check each one to
	see if it is one of the types we are interested in, and if so,
	then extract all the attributes info and generate a partial
	symbol table entry.

NOTES

	Don't attempt to add anonymous structures or unions since they have
	no name.  Anonymous enumerations however are processed, because we
	want to extract their member names (the check for a tag name is
	done later).

	Also, for variables and subroutines, check that this is the place
	where the actual definition occurs, rather than just a reference
	to an external.
 */

static void
scan_partial_symbols (thisdie, enddie, objfile)
     char *thisdie;
     char *enddie;
     struct objfile *objfile;
{
  char *nextdie;
  struct dieinfo di;
  
  while (thisdie < enddie)
    {
      basicdieinfo (&di, thisdie, objfile);
      if (di.die_length < SIZEOF_DIE_LENGTH)
	{
	  break;
	}
      else
	{
	  nextdie = thisdie + di.die_length;
	  /* To avoid getting complete die information for every die, we
	     only do it (below) for the cases we are interested in. */
	  switch (di.die_tag)
	    {
	    case TAG_global_subroutine:
	    case TAG_subroutine:
	    case TAG_global_variable:
	    case TAG_local_variable:
	      completedieinfo (&di, objfile);
	      if (di.at_name && (di.has_at_low_pc || di.at_location))
		{
		  add_partial_symbol (&di, objfile);
		}
	      break;
	    case TAG_typedef:
	    case TAG_structure_type:
	    case TAG_union_type:
	      completedieinfo (&di, objfile);
	      if (di.at_name)
		{
		  add_partial_symbol (&di, objfile);
		}
	      break;
	    case TAG_enumeration_type:
	      completedieinfo (&di, objfile);
	      add_partial_symbol (&di, objfile);
	      break;
	    }
	}
      thisdie = nextdie;
    }
}

/*

LOCAL FUNCTION

	scan_compilation_units -- build a psymtab entry for each compilation

DESCRIPTION

	This is the top level dwarf parsing routine for building partial
	symbol tables.

	It scans from the beginning of the DWARF table looking for the first
	TAG_compile_unit DIE, and then follows the sibling chain to locate
	each additional TAG_compile_unit DIE.
   
	For each TAG_compile_unit DIE it creates a partial symtab structure,
	calls a subordinate routine to collect all the compilation unit's
	global DIE's, file scope DIEs, typedef DIEs, etc, and then links the
	new partial symtab structure into the partial symbol table.  It also
	records the appropriate information in the partial symbol table entry
	to allow the chunk of DIE's and line number table for this compilation
	unit to be located and re-read later, to generate a complete symbol
	table entry for the compilation unit.

	Thus it effectively partitions up a chunk of DIE's for multiple
	compilation units into smaller DIE chunks and line number tables,
	and associates them with a partial symbol table entry.

NOTES

	If any compilation unit has no line number table associated with
	it for some reason (a missing at_stmt_list attribute, rather than
	just one with a value of zero, which is valid) then we ensure that
	the recorded file offset is zero so that the routine which later
	reads line number table fragments knows that there is no fragment
	to read.

RETURNS

	Returns no value.

 */

static void
scan_compilation_units (filename, thisdie, enddie, dbfoff, lnoffset, objfile)
     char *filename;
     char *thisdie;
     char *enddie;
     unsigned int dbfoff;
     unsigned int lnoffset;
     struct objfile *objfile;
{
  char *nextdie;
  struct dieinfo di;
  struct partial_symtab *pst;
  int culength;
  int curoff;
  int curlnoffset;

  while (thisdie < enddie)
    {
      basicdieinfo (&di, thisdie, objfile);
      if (di.die_length < SIZEOF_DIE_LENGTH)
	{
	  break;
	}
      else if (di.die_tag != TAG_compile_unit)
	{
	  nextdie = thisdie + di.die_length;
	}
      else
	{
	  completedieinfo (&di, objfile);
	  if (di.at_sibling != 0)
	    {
	      nextdie = dbbase + di.at_sibling - dbroff;
	    }
	  else
	    {
	      nextdie = thisdie + di.die_length;
	    }
	  curoff = thisdie - dbbase;
	  culength = nextdie - thisdie;
	  curlnoffset = di.has_at_stmt_list ? lnoffset + di.at_stmt_list : 0;

	  /* First allocate a new partial symbol table structure */

	  pst = start_psymtab_common (objfile, base_section_offsets, di.at_name,
				      di.at_low_pc,
				      objfile -> global_psymbols.next,
				      objfile -> static_psymbols.next);

	  pst -> texthigh = di.at_high_pc;
	  pst -> read_symtab_private = (char *)
	      obstack_alloc (&objfile -> psymbol_obstack,
			     sizeof (struct dwfinfo));
	  DBFOFF (pst) = dbfoff;
	  DBROFF (pst) = curoff;
	  DBLENGTH (pst) = culength;
	  LNFOFF (pst)  = curlnoffset;
	  pst -> read_symtab = dwarf_psymtab_to_symtab;

	  /* Now look for partial symbols */

	  scan_partial_symbols (thisdie + di.die_length, nextdie, objfile);

	  pst -> n_global_syms = objfile -> global_psymbols.next -
	    (objfile -> global_psymbols.list + pst -> globals_offset);
	  pst -> n_static_syms = objfile -> static_psymbols.next - 
	    (objfile -> static_psymbols.list + pst -> statics_offset);
	  sort_pst_symbols (pst);
	  /* If there is already a psymtab or symtab for a file of this name,
	     remove it. (If there is a symtab, more drastic things also
	     happen.)  This happens in VxWorks.  */
	  free_named_symtabs (pst -> filename);
	}
      thisdie = nextdie;      
    }
}

/*

LOCAL FUNCTION

	new_symbol -- make a symbol table entry for a new symbol

SYNOPSIS

	static struct symbol *new_symbol (struct dieinfo *dip,
					  struct objfile *objfile)

DESCRIPTION

	Given a pointer to a DWARF information entry, figure out if we need
	to make a symbol table entry for it, and if so, create a new entry
	and return a pointer to it.
 */

static struct symbol *
new_symbol (dip, objfile)
     struct dieinfo *dip;
     struct objfile *objfile;
{
  struct symbol *sym = NULL;
  
  if (dip -> at_name != NULL)
    {
      sym = (struct symbol *) obstack_alloc (&objfile -> symbol_obstack,
					     sizeof (struct symbol));
      memset (sym, 0, sizeof (struct symbol));
      SYMBOL_NAME (sym) = create_name (dip -> at_name, &objfile->symbol_obstack);
      /* default assumptions */
      SYMBOL_NAMESPACE (sym) = VAR_NAMESPACE;
      SYMBOL_CLASS (sym) = LOC_STATIC;
      SYMBOL_TYPE (sym) = decode_die_type (dip);
      switch (dip -> die_tag)
	{
	case TAG_label:
	  SYMBOL_VALUE (sym) = dip -> at_low_pc;
	  SYMBOL_CLASS (sym) = LOC_LABEL;
	  break;
	case TAG_global_subroutine:
	case TAG_subroutine:
	  SYMBOL_VALUE (sym) = dip -> at_low_pc;
	  SYMBOL_TYPE (sym) = lookup_function_type (SYMBOL_TYPE (sym));
	  SYMBOL_CLASS (sym) = LOC_BLOCK;
	  if (dip -> die_tag == TAG_global_subroutine)
	    {
	      add_symbol_to_list (sym, &global_symbols);
	    }
	  else
	    {
	      add_symbol_to_list (sym, list_in_scope);
	    }
	  break;
	case TAG_global_variable:
	  if (dip -> at_location != NULL)
	    {
	      SYMBOL_VALUE (sym) = locval (dip -> at_location);
	      add_symbol_to_list (sym, &global_symbols);
	      SYMBOL_CLASS (sym) = LOC_STATIC;
	      SYMBOL_VALUE (sym) += baseaddr;
	    }
	  break;
	case TAG_local_variable:
	  if (dip -> at_location != NULL)
	    {
	      SYMBOL_VALUE (sym) = locval (dip -> at_location);
	      add_symbol_to_list (sym, list_in_scope);
	      if (isreg)
		{
		  SYMBOL_CLASS (sym) = LOC_REGISTER;
		}
	      else if (offreg)
		{
		  SYMBOL_CLASS (sym) = LOC_LOCAL;
		}
	      else
		{
		  SYMBOL_CLASS (sym) = LOC_STATIC;
		  SYMBOL_VALUE (sym) += baseaddr;
		}
	    }
	  break;
	case TAG_formal_parameter:
	  if (dip -> at_location != NULL)
	    {
	      SYMBOL_VALUE (sym) = locval (dip -> at_location);
	    }
	  add_symbol_to_list (sym, list_in_scope);
	  if (isreg)
	    {
	      SYMBOL_CLASS (sym) = LOC_REGPARM;
	    }
	  else
	    {
	      SYMBOL_CLASS (sym) = LOC_ARG;
	    }
	  break;
	case TAG_unspecified_parameters:
	  /* From varargs functions; gdb doesn't seem to have any interest in
	     this information, so just ignore it for now. (FIXME?) */
	  break;
	case TAG_structure_type:
	case TAG_union_type:
	case TAG_enumeration_type:
	  SYMBOL_CLASS (sym) = LOC_TYPEDEF;
	  SYMBOL_NAMESPACE (sym) = STRUCT_NAMESPACE;
	  add_symbol_to_list (sym, list_in_scope);
	  break;
	case TAG_typedef:
	  SYMBOL_CLASS (sym) = LOC_TYPEDEF;
	  SYMBOL_NAMESPACE (sym) = VAR_NAMESPACE;
	  add_symbol_to_list (sym, list_in_scope);
	  break;
	default:
	  /* Not a tag we recognize.  Hopefully we aren't processing trash
	     data, but since we must specifically ignore things we don't
	     recognize, there is nothing else we should do at this point. */
	  break;
	}
    }
  return (sym);
}

/*

LOCAL FUNCTION

	decode_mod_fund_type -- decode a modified fundamental type

SYNOPSIS

	static struct type *decode_mod_fund_type (char *typedata)

DESCRIPTION

	Decode a block of data containing a modified fundamental
	type specification.  TYPEDATA is a pointer to the block,
	which starts with a length containing the size of the rest
	of the block.  At the end of the block is a fundmental type
	code value that gives the fundamental type.  Everything
	in between are type modifiers.

	We simply compute the number of modifiers and call the general
	function decode_modified_type to do the actual work.
*/

static struct type *
decode_mod_fund_type (typedata)
     char *typedata;
{
  struct type *typep = NULL;
  unsigned short modcount;
  int nbytes;
  
  /* Get the total size of the block, exclusive of the size itself */

  nbytes = attribute_size (AT_mod_fund_type);
  modcount = target_to_host (typedata, nbytes, GET_UNSIGNED, current_objfile);
  typedata += nbytes;

  /* Deduct the size of the fundamental type bytes at the end of the block. */

  modcount -= attribute_size (AT_fund_type);

  /* Now do the actual decoding */

  typep = decode_modified_type (typedata, modcount, AT_mod_fund_type);
  return (typep);
}

/*

LOCAL FUNCTION

	decode_mod_u_d_type -- decode a modified user defined type

SYNOPSIS

	static struct type *decode_mod_u_d_type (char *typedata)

DESCRIPTION

	Decode a block of data containing a modified user defined
	type specification.  TYPEDATA is a pointer to the block,
	which consists of a two byte length, containing the size
	of the rest of the block.  At the end of the block is a
	four byte value that gives a reference to a user defined type.
	Everything in between are type modifiers.

	We simply compute the number of modifiers and call the general
	function decode_modified_type to do the actual work.
*/

static struct type *
decode_mod_u_d_type (typedata)
     char *typedata;
{
  struct type *typep = NULL;
  unsigned short modcount;
  int nbytes;
  
  /* Get the total size of the block, exclusive of the size itself */

  nbytes = attribute_size (AT_mod_u_d_type);
  modcount = target_to_host (typedata, nbytes, GET_UNSIGNED, current_objfile);
  typedata += nbytes;

  /* Deduct the size of the reference type bytes at the end of the block. */

  modcount -= attribute_size (AT_user_def_type);

  /* Now do the actual decoding */

  typep = decode_modified_type (typedata, modcount, AT_mod_u_d_type);
  return (typep);
}

/*

LOCAL FUNCTION

	decode_modified_type -- decode modified user or fundamental type

SYNOPSIS

	static struct type *decode_modified_type (char *modifiers,
	    unsigned short modcount, int mtype)

DESCRIPTION

	Decode a modified type, either a modified fundamental type or
	a modified user defined type.  MODIFIERS is a pointer to the
	block of bytes that define MODCOUNT modifiers.  Immediately
	following the last modifier is a short containing the fundamental
	type or a long containing the reference to the user defined
	type.  Which one is determined by MTYPE, which is either
	AT_mod_fund_type or AT_mod_u_d_type to indicate what modified
	type we are generating.

	We call ourself recursively to generate each modified type,`
	until MODCOUNT reaches zero, at which point we have consumed
	all the modifiers and generate either the fundamental type or
	user defined type.  When the recursion unwinds, each modifier
	is applied in turn to generate the full modified type.

NOTES

	If we find a modifier that we don't recognize, and it is not one
	of those reserved for application specific use, then we issue a
	warning and simply ignore the modifier.

BUGS

	We currently ignore MOD_const and MOD_volatile.  (FIXME)

 */

static struct type *
decode_modified_type (modifiers, modcount, mtype)
     char *modifiers;
     unsigned int modcount;
     int mtype;
{
  struct type *typep = NULL;
  unsigned short fundtype;
  DIE_REF die_ref;
  char modifier;
  int nbytes;
  
  if (modcount == 0)
    {
      switch (mtype)
	{
	case AT_mod_fund_type:
	  nbytes = attribute_size (AT_fund_type);
	  fundtype = target_to_host (modifiers, nbytes, GET_UNSIGNED,
				     current_objfile);
	  typep = decode_fund_type (fundtype);
	  break;
	case AT_mod_u_d_type:
	  nbytes = attribute_size (AT_user_def_type);
	  die_ref = target_to_host (modifiers, nbytes, GET_UNSIGNED,
				    current_objfile);
	  if ((typep = lookup_utype (die_ref)) == NULL)
	    {
	      typep = alloc_utype (die_ref, NULL);
	    }
	  break;
	default:
	  SQUAWK (("botched modified type decoding (mtype 0x%x)", mtype));
	  typep = lookup_fundamental_type (current_objfile, FT_INTEGER);
	  break;
	}
    }
  else
    {
      modifier = *modifiers++;
      typep = decode_modified_type (modifiers, --modcount, mtype);
      switch (modifier)
	{
	  case MOD_pointer_to:
	    typep = lookup_pointer_type (typep);
	    break;
	  case MOD_reference_to:
	    typep = lookup_reference_type (typep);
	    break;
	  case MOD_const:
	    SQUAWK (("type modifier 'const' ignored"));		/* FIXME */
	    break;
	  case MOD_volatile:
	    SQUAWK (("type modifier 'volatile' ignored"));	/* FIXME */
	    break;
	  default:
	    if (!(MOD_lo_user <= (unsigned char) modifier
		  && (unsigned char) modifier <= MOD_hi_user))
	      {
		SQUAWK (("unknown type modifier %u",
			 (unsigned char) modifier));
	      }
	    break;
	}
    }
  return (typep);
}

/*

LOCAL FUNCTION

	decode_fund_type -- translate basic DWARF type to gdb base type

DESCRIPTION

	Given an integer that is one of the fundamental DWARF types,
	translate it to one of the basic internal gdb types and return
	a pointer to the appropriate gdb type (a "struct type *").

NOTES

	If we encounter a fundamental type that we are unprepared to
	deal with, and it is not in the range of those types defined
	as application specific types, then we issue a warning and
	treat the type as an "int".
*/

static struct type *
decode_fund_type (fundtype)
     unsigned int fundtype;
{
  struct type *typep = NULL;
  
  switch (fundtype)
    {

    case FT_void:
      typep = lookup_fundamental_type (current_objfile, FT_VOID);
      break;
    
    case FT_boolean:		/* Was FT_set in AT&T version */
      typep = lookup_fundamental_type (current_objfile, FT_BOOLEAN);
      break;

    case FT_pointer:		/* (void *) */
      typep = lookup_fundamental_type (current_objfile, FT_VOID);
      typep = lookup_pointer_type (typep);
      break;
    
    case FT_char:
      typep = lookup_fundamental_type (current_objfile, FT_CHAR);
      break;
    
    case FT_signed_char:
      typep = lookup_fundamental_type (current_objfile, FT_SIGNED_CHAR);
      break;

    case FT_unsigned_char:
      typep = lookup_fundamental_type (current_objfile, FT_UNSIGNED_CHAR);
      break;
    
    case FT_short:
      typep = lookup_fundamental_type (current_objfile, FT_SHORT);
      break;

    case FT_signed_short:
      typep = lookup_fundamental_type (current_objfile, FT_SIGNED_SHORT);
      break;
    
    case FT_unsigned_short:
      typep = lookup_fundamental_type (current_objfile, FT_UNSIGNED_SHORT);
      break;
    
    case FT_integer:
      typep = lookup_fundamental_type (current_objfile, FT_INTEGER);
      break;

    case FT_signed_integer:
      typep = lookup_fundamental_type (current_objfile, FT_SIGNED_INTEGER);
      break;
    
    case FT_unsigned_integer:
      typep = lookup_fundamental_type (current_objfile, FT_UNSIGNED_INTEGER);
      break;
    
    case FT_long:
      typep = lookup_fundamental_type (current_objfile, FT_LONG);
      break;

    case FT_signed_long:
      typep = lookup_fundamental_type (current_objfile, FT_SIGNED_LONG);
      break;
    
    case FT_unsigned_long:
      typep = lookup_fundamental_type (current_objfile, FT_UNSIGNED_LONG);
      break;
    
    case FT_long_long:
      typep = lookup_fundamental_type (current_objfile, FT_LONG_LONG);
      break;

    case FT_signed_long_long:
      typep = lookup_fundamental_type (current_objfile, FT_SIGNED_LONG_LONG);
      break;

    case FT_unsigned_long_long:
      typep = lookup_fundamental_type (current_objfile, FT_UNSIGNED_LONG_LONG);
      break;

    case FT_float:
      typep = lookup_fundamental_type (current_objfile, FT_FLOAT);
      break;
    
    case FT_dbl_prec_float:
      typep = lookup_fundamental_type (current_objfile, FT_DBL_PREC_FLOAT);
      break;
    
    case FT_ext_prec_float:
      typep = lookup_fundamental_type (current_objfile, FT_EXT_PREC_FLOAT);
      break;
    
    case FT_complex:
      typep = lookup_fundamental_type (current_objfile, FT_COMPLEX);
      break;
    
    case FT_dbl_prec_complex:
      typep = lookup_fundamental_type (current_objfile, FT_DBL_PREC_COMPLEX);
      break;
    
    case FT_ext_prec_complex:
      typep = lookup_fundamental_type (current_objfile, FT_EXT_PREC_COMPLEX);
      break;
    
    }

  if ((typep == NULL) && !(FT_lo_user <= fundtype && fundtype <= FT_hi_user))
    {
      SQUAWK (("unexpected fundamental type 0x%x", fundtype));
      typep = lookup_fundamental_type (current_objfile, FT_VOID);
    }
    
  return (typep);
}

/*

LOCAL FUNCTION

	create_name -- allocate a fresh copy of a string on an obstack

DESCRIPTION

	Given a pointer to a string and a pointer to an obstack, allocates
	a fresh copy of the string on the specified obstack.

*/

static char *
create_name (name, obstackp)
     char *name;
     struct obstack *obstackp;
{
  int length;
  char *newname;

  length = strlen (name) + 1;
  newname = (char *) obstack_alloc (obstackp, length);
  strcpy (newname, name);
  return (newname);
}

/*

LOCAL FUNCTION

	basicdieinfo -- extract the minimal die info from raw die data

SYNOPSIS

	void basicdieinfo (char *diep, struct dieinfo *dip,
			   struct objfile *objfile)

DESCRIPTION

	Given a pointer to raw DIE data, and a pointer to an instance of a
	die info structure, this function extracts the basic information
	from the DIE data required to continue processing this DIE, along
	with some bookkeeping information about the DIE.

	The information we absolutely must have includes the DIE tag,
	and the DIE length.  If we need the sibling reference, then we
	will have to call completedieinfo() to process all the remaining
	DIE information.

	Note that since there is no guarantee that the data is properly
	aligned in memory for the type of access required (indirection
	through anything other than a char pointer), and there is no
	guarantee that it is in the same byte order as the gdb host,
	we call a function which deals with both alignment and byte
	swapping issues.  Possibly inefficient, but quite portable.

	We also take care of some other basic things at this point, such
	as ensuring that the instance of the die info structure starts
	out completely zero'd and that curdie is initialized for use
	in error reporting if we have a problem with the current die.

NOTES

	All DIE's must have at least a valid length, thus the minimum
	DIE size is SIZEOF_DIE_LENGTH.  In order to have a valid tag, the
	DIE size must be at least SIZEOF_DIE_TAG larger, otherwise they
	are forced to be TAG_padding DIES.

	Padding DIES must be at least SIZEOF_DIE_LENGTH in length, implying
	that if a padding DIE is used for alignment and the amount needed is
	less than SIZEOF_DIE_LENGTH, then the padding DIE has to be big
	enough to align to the next alignment boundry.
 */

static void
basicdieinfo (dip, diep, objfile)
     struct dieinfo *dip;
     char *diep;
     struct objfile *objfile;
{
  curdie = dip;
  memset (dip, 0, sizeof (struct dieinfo));
  dip -> die = diep;
  dip -> die_ref = dbroff + (diep - dbbase);
  dip -> die_length = target_to_host (diep, SIZEOF_DIE_LENGTH, GET_UNSIGNED,
				      objfile);
  if (dip -> die_length < SIZEOF_DIE_LENGTH)
    {
      dwarfwarn ("malformed DIE, bad length (%d bytes)", dip -> die_length);
    }
  else if (dip -> die_length < (SIZEOF_DIE_LENGTH + SIZEOF_DIE_TAG))
    {
      dip -> die_tag = TAG_padding;
    }
  else
    {
      diep += SIZEOF_DIE_LENGTH;
      dip -> die_tag = target_to_host (diep, SIZEOF_DIE_TAG, GET_UNSIGNED,
				       objfile);
    }
}

/*

LOCAL FUNCTION

	completedieinfo -- finish reading the information for a given DIE

SYNOPSIS

	void completedieinfo (struct dieinfo *dip, struct objfile *objfile)

DESCRIPTION

	Given a pointer to an already partially initialized die info structure,
	scan the raw DIE data and finish filling in the die info structure
	from the various attributes found.
   
	Note that since there is no guarantee that the data is properly
	aligned in memory for the type of access required (indirection
	through anything other than a char pointer), and there is no
	guarantee that it is in the same byte order as the gdb host,
	we call a function which deals with both alignment and byte
	swapping issues.  Possibly inefficient, but quite portable.

NOTES

	Each time we are called, we increment the diecount variable, which
	keeps an approximate count of the number of dies processed for
	each compilation unit.  This information is presented to the user
	if the info_verbose flag is set.

 */

static void
completedieinfo (dip, objfile)
     struct dieinfo *dip;
     struct objfile *objfile;
{
  char *diep;			/* Current pointer into raw DIE data */
  char *end;			/* Terminate DIE scan here */
  unsigned short attr;		/* Current attribute being scanned */
  unsigned short form;		/* Form of the attribute */
  int nbytes;			/* Size of next field to read */
  
  diecount++;
  diep = dip -> die;
  end = diep + dip -> die_length;
  diep += SIZEOF_DIE_LENGTH + SIZEOF_DIE_TAG;
  while (diep < end)
    {
      attr = target_to_host (diep, SIZEOF_ATTRIBUTE, GET_UNSIGNED, objfile);
      diep += SIZEOF_ATTRIBUTE;
      if ((nbytes = attribute_size (attr)) == -1)
	{
	  SQUAWK (("unknown attribute length, skipped remaining attributes"));;
	  diep = end;
	  continue;
	}
      switch (attr)
	{
	case AT_fund_type:
	  dip -> at_fund_type = target_to_host (diep, nbytes, GET_UNSIGNED,
						objfile);
	  break;
	case AT_ordering:
	  dip -> at_ordering = target_to_host (diep, nbytes, GET_UNSIGNED,
					       objfile);
	  break;
	case AT_bit_offset:
	  dip -> at_bit_offset = target_to_host (diep, nbytes, GET_UNSIGNED,
						 objfile);
	  break;
	case AT_visibility:
	  dip -> at_visibility = target_to_host (diep, nbytes, GET_UNSIGNED,
						 objfile);
	  break;
	case AT_sibling:
	  dip -> at_sibling = target_to_host (diep, nbytes, GET_UNSIGNED,
					      objfile);
	  break;
	case AT_stmt_list:
	  dip -> at_stmt_list = target_to_host (diep, nbytes, GET_UNSIGNED,
						objfile);
	  dip -> has_at_stmt_list = 1;
	  break;
	case AT_low_pc:
	  dip -> at_low_pc = target_to_host (diep, nbytes, GET_UNSIGNED,
					     objfile);
	  dip -> at_low_pc += baseaddr;
	  dip -> has_at_low_pc = 1;
	  break;
	case AT_high_pc:
	  dip -> at_high_pc = target_to_host (diep, nbytes, GET_UNSIGNED,
					      objfile);
	  dip -> at_high_pc += baseaddr;
	  break;
	case AT_language:
	  dip -> at_language = target_to_host (diep, nbytes, GET_UNSIGNED,
					       objfile);
	  break;
	case AT_user_def_type:
	  dip -> at_user_def_type = target_to_host (diep, nbytes,
						    GET_UNSIGNED, objfile);
	  break;
	case AT_byte_size:
	  dip -> at_byte_size = target_to_host (diep, nbytes, GET_UNSIGNED,
						objfile);
	  break;
	case AT_bit_size:
	  dip -> at_bit_size = target_to_host (diep, nbytes, GET_UNSIGNED,
					       objfile);
	  break;
	case AT_member:
	  dip -> at_member = target_to_host (diep, nbytes, GET_UNSIGNED,
					     objfile);
	  break;
	case AT_discr:
	  dip -> at_discr = target_to_host (diep, nbytes, GET_UNSIGNED,
					    objfile);
	  break;
	case AT_import:
	  dip -> at_import = target_to_host (diep, nbytes, GET_UNSIGNED,
					     objfile);
	  break;
	case AT_location:
	  dip -> at_location = diep;
	  break;
	case AT_mod_fund_type:
	  dip -> at_mod_fund_type = diep;
	  break;
	case AT_subscr_data:
	  dip -> at_subscr_data = diep;
	  break;
	case AT_mod_u_d_type:
	  dip -> at_mod_u_d_type = diep;
	  break;
	case AT_element_list:
	  dip -> at_element_list = diep;
	  dip -> short_element_list = 0;
	  break;
	case AT_short_element_list:
	  dip -> at_element_list = diep;
	  dip -> short_element_list = 1;
	  break;
	case AT_discr_value:
	  dip -> at_discr_value = diep;
	  break;
	case AT_string_length:
	  dip -> at_string_length = diep;
	  break;
	case AT_name:
	  dip -> at_name = diep;
	  break;
	case AT_comp_dir:
	  dip -> at_comp_dir = diep;
	  break;
	case AT_producer:
	  dip -> at_producer = diep;
	  break;
	case AT_frame_base:
	  dip -> at_frame_base = target_to_host (diep, nbytes, GET_UNSIGNED,
						 objfile);
	  break;
	case AT_start_scope:
	  dip -> at_start_scope = target_to_host (diep, nbytes, GET_UNSIGNED,
						  objfile);
	  break;
	case AT_stride_size:
	  dip -> at_stride_size = target_to_host (diep, nbytes, GET_UNSIGNED,
						  objfile);
	  break;
	case AT_src_info:
	  dip -> at_src_info = target_to_host (diep, nbytes, GET_UNSIGNED,
					       objfile);
	  break;
	case AT_prototyped:
	  dip -> at_prototyped = diep;
	  break;
	default:
	  /* Found an attribute that we are unprepared to handle.  However
	     it is specifically one of the design goals of DWARF that
	     consumers should ignore unknown attributes.  As long as the
	     form is one that we recognize (so we know how to skip it),
	     we can just ignore the unknown attribute. */
	  break;
	}
      form = FORM_FROM_ATTR (attr);
      switch (form)
	{
	case FORM_DATA2:
	  diep += 2;
	  break;
	case FORM_DATA4:
	case FORM_REF:
	  diep += 4;
	  break;
	case FORM_DATA8:
	  diep += 8;
	  break;
	case FORM_ADDR:
	  diep += TARGET_FT_POINTER_SIZE (objfile);
	  break;
	case FORM_BLOCK2:
	  diep += 2 + target_to_host (diep, nbytes, GET_UNSIGNED, objfile);
	  break;
	case FORM_BLOCK4:
	  diep += 4 + target_to_host (diep, nbytes, GET_UNSIGNED, objfile);
	  break;
	case FORM_STRING:
	  diep += strlen (diep) + 1;
	  break;
	default:
	  SQUAWK (("unknown attribute form (0x%x)", form));
	  SQUAWK (("unknown attribute length, skipped remaining attributes"));;
	  diep = end;
	  break;
	}
    }
}

/*

LOCAL FUNCTION

	target_to_host -- swap in target data to host

SYNOPSIS

	target_to_host (char *from, int nbytes, int signextend,
			struct objfile *objfile)

DESCRIPTION

	Given pointer to data in target format in FROM, a byte count for
	the size of the data in NBYTES, a flag indicating whether or not
	the data is signed in SIGNEXTEND, and a pointer to the current
	objfile in OBJFILE, convert the data to host format and return
	the converted value.

NOTES

	FIXME:  If we read data that is known to be signed, and expect to
	use it as signed data, then we need to explicitly sign extend the
	result until the bfd library is able to do this for us.

 */

static unsigned long
target_to_host (from, nbytes, signextend, objfile)
     char *from;
     int nbytes;
     int signextend;		/* FIXME:  Unused */
     struct objfile *objfile;
{
  unsigned long rtnval;

  switch (nbytes)
    {
      case 8:
        rtnval = bfd_get_64 (objfile -> obfd, (bfd_byte *) from);
	break;
      case 4:
	rtnval = bfd_get_32 (objfile -> obfd, (bfd_byte *) from);
	break;
      case 2:
	rtnval = bfd_get_16 (objfile -> obfd, (bfd_byte *) from);
	break;
      case 1:
	rtnval = bfd_get_8 (objfile -> obfd, (bfd_byte *) from);
	break;
      default:
	dwarfwarn ("no bfd support for %d byte data object", nbytes);
	rtnval = 0;
	break;
    }
  return (rtnval);
}

/*

LOCAL FUNCTION

	attribute_size -- compute size of data for a DWARF attribute

SYNOPSIS

	static int attribute_size (unsigned int attr)

DESCRIPTION

	Given a DWARF attribute in ATTR, compute the size of the first
	piece of data associated with this attribute and return that
	size.

	Returns -1 for unrecognized attributes.

 */

static int
attribute_size (attr)
     unsigned int attr;
{
  int nbytes;			/* Size of next data for this attribute */
  unsigned short form;		/* Form of the attribute */

  form = FORM_FROM_ATTR (attr);
  switch (form)
    {
      case FORM_STRING:		/* A variable length field is next */
        nbytes = 0;
	break;
      case FORM_DATA2:		/* Next 2 byte field is the data itself */
      case FORM_BLOCK2:		/* Next 2 byte field is a block length */
	nbytes = 2;
	break;
      case FORM_DATA4:		/* Next 4 byte field is the data itself */
      case FORM_BLOCK4:		/* Next 4 byte field is a block length */
      case FORM_REF:		/* Next 4 byte field is a DIE offset */
	nbytes = 4;
	break;
      case FORM_DATA8:		/* Next 8 byte field is the data itself */
	nbytes = 8;
	break;
      case FORM_ADDR:		/* Next field size is target sizeof(void *) */
	nbytes = TARGET_FT_POINTER_SIZE (objfile);
	break;
      default:
	SQUAWK (("unknown attribute form (0x%x)", form));
	nbytes = -1;
	break;
      }
  return (nbytes);
}
