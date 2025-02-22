$! Set the def dir to proper place for use in batch. Works for interactive too.
$flnm = f$enviroment("PROCEDURE")     ! get current procedure name
$set default 'f$parse(flnm,,,"DEVICE")''f$parse(flnm,,,"DIRECTORY")'
$!
$ v=f$verify(0)
$set symbol/scope=(nolocal,noglobal)
$!
$! CAUTION: If you want to link gcc-cc1 to the sharable image library
$! VAXCRTL, see the notes in gcc.texinfo (or INSTALL) first.
$!
$!	Build the GNU "C" compiler on VMS
$!
$!	C compiler
$!
$ CC	:=	gcc
$! CC	:=	cc	!uncomment for VAXC
$ BISON	:=	bison
$ RENAME	:=	rename/new_vers
$ LINK	:=	link
$!
$!	Compiler options
$!
$ CFLAGS =	"/debug/cc1_options=""-mpcc-alignment""/incl=([],[.config])"
$! CFLAGS =	"/noopt/incl=([],[.config])"
$!
$!	Link options
$!
$ LDFLAGS :=	/nomap
$!
$!	Link libraries
$!
$ LIBS :=	gnu_cc:[000000]gcclib.olb/libr,sys$library:vaxcrtl.olb/libr
$! LIBS :=	alloca.obj,sys$library:vaxcrtl.olb/libr
$!
$!
$!  First we figure out what needs to be done.  This is sort of like a limited
$! make facility - the command line options specify exactly what components
$! we want to build.  The following options are understood:
$!
$!	LINK:	Assume that the object modules for the selected compiler(s)
$!		have already been compiled, perform link phase only.
$!
$!	CC1:	Compile and link "C" compiler.
$!
$!	CC1PLUS:Compile and link "C++" compiler.
$!
$!	CC1OBJ:	Compile and link objective C compiler.
$!
$!	ALL:	Compile and link all of the CC1 passes.
$!
$!	INDEPENDENT:
$!		Compile language independent source modules. (On by default).
$!
$!	DEBUG:	Link images with /debug.
$!
$! If you want to list more than one option, you should use a spaces to
$! separate them.
$!
$!	Any one of the above options can be prefaced with a "NO".  For example,
$! if you had already built GCC, and you wanted to build G++, you could use the
$! "CC1PLUS NOINDEPENDENT" options, which would only compile the C++ language
$! specific source files, and then link the C++ compiler.
$!
$! If you do not specify which compiler you want to build, it is assumed that
$! you want to build GNU-C ("CC1").
$!
$! Now figure out what we have been requested to do.
$p1 = p1+" "+p2+" "+p3+" "+p4+" "+p5+" "+p6+" "+p7 
$p1 = f$edit(p1,"COMPRESS")
$i=0
$DO_ALL = 0
$DO_LINK = 0
$DO_DEBUG = 0
$open cfile$ compilers.list
$cinit:read cfile$ compilername/end=cinit_done
$DO_'compilername'=0
$goto cinit
$cinit_done: close cfile$
$DO_INDEPENDENT = 1
$DO_DEFAULT = 1
$loop:
$string = f$element(i," ",p1)
$if string.eqs." " then goto done
$flag = 1
$if string.eqs."CC1PLUS" then DO_DEFAULT = 0
$if string.eqs."CC1OBJ" then DO_DEFAULT = 0
$if f$extract(0,2,string).nes."NO" then goto parse_option
$  string=f$extract(2,f$length(string)-2,string)
$  flag = 0
$parse_option:
$DO_'string' = flag
$i=i+1
$goto loop
$!
$done:
$if DO_DEFAULT.eq.1 then DO_CC1 = 1
$echo := write sys$Output
$echo "This command file will now perform the following actions:
$if DO_LINK.eq.1 then goto link_only
$if DO_ALL.eq.1 then echo "   Compile all language specific object modules."
$if DO_CC1.eq.1 then echo "   Compile C specific object modules."
$if DO_CC1PLUS.eq.1 then echo "   Compile C++ specific object modules."
$if DO_CC1OBJ.eq.1 then echo "   Compile obj-C specific object modules."
$if DO_INDEPENDENT.eq.1 then echo "   Compile language independent object modules."
$link_only:
$if DO_CC1.eq.1 then	echo "   Link C compiler (gcc-cc1.exe)."
$if DO_CC1PLUS.eq.1 then echo "   Link C++ compiler (gcc-cc1plus.exe)."
$if DO_CC1OBJ.eq.1 then echo "   Link objective-C compiler (gcc-cc1obj.exe)."
$if DO_DEBUG.eq.1 then echo  "   Link images to run under debugger."
$!
$! Test and see if we need these messages or not.  The -1 switch gives it away.
$!
$gas := $gnu_cc:[000000]gcc-as.exe
$if f$search(gas-"$").eqs."" then  goto gas_message	!must be VAXC
$define/user sys$error sys$scratch:gas_test.tmp
$gas -1 nla0: -o nla0:
$size=f$file_attributes("sys$scratch:gas_test.tmp","ALQ")
$delete/nolog sys$scratch:gas_test.tmp;*
$if size.eq.0 then goto no_message
$gas_message:
$type sys$input

	Note: GCC 2.0 treats external variables differently than GCC 1.40 does.
Before you use GCC 2.0, you should obtain a version of the assembler which 
contains the patches to work with GCC 2.0 (GCC-AS 1.38 does not contain 
these patches - whatever comes after this probably will).  The assembler
in gcc-vms-1.40.tar.Z from prep does contain the proper patches.

	If you do not update the assembler, the compiler will still work,
but `extern const' variables will be treated as `extern'.  This will result
in linker warning messages about mismatched psect attributes, and these
variables will be placed in read/write storage.

$!
$no_message:
$!
$!
$ if DO_DEBUG.eq.1 then LDFLAGS :='LDFLAGS'/debug
$!
$if DO_LINK.eq.1 then goto compile_cc1
$if DO_INDEPENDENT.eq.1 
$	THEN 
$!
$! Build alloca if necessary (in 'LIBS for use with VAXC)
$!
$ if f$locate("alloca.obj",f$edit(LIBS,"lowercase")).ge.f$length(LIBS) then -
	goto skip_alloca
$ if f$search("alloca.obj").nes."" then -  !does .obj exist? is it up to date?
    if f$cvtime(f$file_attributes("alloca.obj","RDT")).gts.-
       f$cvtime(f$file_attributes("alloca.c","RDT")) then  goto skip_alloca
$set verify
$ 'CC 'CFLAGS /define="STACK_DIRECTION=(-1)" alloca.c
$!'f$verify(0)
$skip_alloca:
$!
$! First build a couple of header files from the machine description
$! These are used by many of the source modules, so we build them now.
$!
$set verify
$ 'CC 'CFLAGS rtl.C
$ 'CC 'CFLAGS obstack.C
$!'f$verify(0)
$! Generate insn-attr.h
$	call generate insn-attr.h
$	call generate insn-flags.h
$	call generate insn-codes.h
$	call generate insn-config.h
$!
$call compile independent.opt "rtl,obstack,insn-attrtab"
$!
$	call generate insn-attrtab.c "rtlanal,"
$set verify
$ 'CC 'CFLAGS insn-attrtab.c
$!'f$verify(0)
$	endif
$!
$compile_cc1:
$open cfile$ compilers.list
$cloop:read cfile$ compilername/end=cdone
$! language specific modules
$!
$if (DO_ALL + DO_'compilername').eq.0 then goto cloop
$if DO_LINK.eq.0 then call compile 'compilername'-objs.opt "obstack"
$!
$! CAUTION: If you want to link gcc-cc1* to the sharable image library
$! VAXCRTL, see the notes in gcc.texinfo (or INSTALL) first.
$!
$set verify
$ link 'LDFLAGS' /exe=gcc-'compilername'  version.opt/opt, -
	  'compilername'-objs.opt/opt, independent.opt/opt, -
	  'LIBS'
$!'f$verify(0)
$goto cloop
$!
$!
$cdone: close cfile$
$!
$!	Done
$!
$! 'f$verify(v)
$exit
$!
$!  Various DCL subroutines follow...
$!
$!  This routine takes parameter p1 to be a linker options file with a list
$!  of object files that are needed.  It extracts the names, and compiles
$!  each source module, one by one.  File names that begin with an
$!  "INSN-" are assumed to be generated by a GEN*.C program.
$!
$!  Parameter P2 is a list of files which will appear in the options file
$!  that should not be compiled.  This allows us to handle special cases.
$!
$compile:
$subroutine
$on error then goto c_err
$on control_y then goto c_err
$open ifile$ 'p1'
$loop: read ifile$ line/end=c_done
$!
$i=0
$loop1:
$flnm=f$element(i,",",line)
$i=i+1
$if flnm.eqs."" then goto loop
$if flnm.eqs."," then goto loop
$if f$locate(flnm,"''p2'").nes.f$length("''p2'") then goto loop1
$!
$if f$locate("-parse",flnm).nes.f$length(flnm)
$	then
$	if (f$search("''flnm'.C") .eqs. "") then goto yes_bison
$	if (f$cvtime(f$file_attributes("''flnm'.Y","RDT")).les. -
 	    f$cvtime(f$file_attributes("''flnm'.C","RDT")))  -
		then goto no_bison
$yes_bison:
$set verify
$	 'BISON' /define /verbose 'flnm'.y
$	 'RENAME' 'flnm'_tab.c 'flnm'.c
$	 'RENAME' 'flnm'_tab.h 'flnm'.h
$!'f$verify(0)
$no_bison:
$	endif
$!
$if f$extract(0,5,flnm).eqs."insn-" then call generate 'flnm'.c
$!
$set verify
$ 'CC 'CFLAGS 'flnm'.c
$!'f$verify(0)
$goto loop1
$!
$goto loop
$!
$! In case of error or abort, go here (In order to close file).
$!
$c_err: !'f$verify(0)
$close ifile$
$exit %x2c
$!
$c_done:
$close ifile$
$endsubroutine
$!
$! This subroutine generates the insn-* files.  The first argument is the
$! name of the insn-* file to generate.  The second argument contains a 
$! list of any other object modules which must be linked to the gen*.c
$! program.
$!
$! If a previous version of insn-* exists, it is compared to the new one,
$! and if it has not changed, then the new one is discarded.  This is
$! done so that make like programs do not get thrown off.
$!
$generate:
$subroutine
$if f$extract(0,5,p1).nes."INSN-"
$	then
$	write sys$error "Unknown file passed to generate."
$	exit 1
$	endif
$root1=f$parse(f$extract(5,255,p1),,,"NAME")
$	set verify
$ 'CC 'CFLAGS GEN'root1'.C
$ link 'LDFLAGS' GEN'root1',rtl,obstack,'p2' -
	  'LIBS'
$!	'f$verify(0)
$!
$set verify
$	assign/user 'p1' sys$output:
$	mcr sys$disk:[]GEN'root1' md
$!'f$verify(0)
$endsubroutine
