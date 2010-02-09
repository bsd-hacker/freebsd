/* Definitions for PowerPC running FreeBSD using the ELF format
   Copyright (C) 2001, 2003 Free Software Foundation, Inc.
   Contributed by David E. O'Brien <obrien@FreeBSD.org> and BSDi.

   This file is part of GCC.

   GCC is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published
   by the Free Software Foundation; either version 2, or (at your
   option) any later version.

   GCC is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License
   along with GCC; see the file COPYING.  If not, write to the
   Free Software Foundation, 51 Franklin Street, Fifth Floor, Boston,
   MA 02110-1301, USA.  */

/* Override the defaults, which exist to force the proper definition.  */

#ifdef IN_LIBGCC2
#undef TARGET_64BIT
#ifdef __ppc64__
#define TARGET_64BIT 1
#else
#define TARGET_64BIT 0
#endif
#endif

/* On 64-bit systems, use the AIX ABI like Linux and NetBSD */

#undef	DEFAULT_ABI
#define	DEFAULT_ABI (TARGET_64BIT ? ABI_AIX : ABI_V4)
#undef	TARGET_AIX
#define	TARGET_AIX TARGET_64BIT

#undef  FBSD_TARGET_CPU_CPP_BUILTINS
#define FBSD_TARGET_CPU_CPP_BUILTINS()		\
  do						\
    {						\
      builtin_define ("__PPC__");		\
      builtin_define ("__ppc__");		\
      builtin_define ("__PowerPC__");		\
      builtin_define ("__powerpc__");		\
      if (TARGET_64BIT)				\
	{					\
	  builtin_define ("__LP64__");		\
	  builtin_define ("__ppc64__");		\
	  builtin_define ("__powerpc64__");	\
	  builtin_define ("__arch64__");	\
	  builtin_assert ("cpu=powerpc64");	\
	  builtin_assert ("machine=powerpc64");	\
	} else {				\
	  builtin_assert ("cpu=powerpc");	\
	  builtin_assert ("machine=powerpc");	\
	}					\
    }						\
  while (0)

#define INVALID_64BIT "-m%s not supported in this configuration"
#define INVALID_32BIT INVALID_64BIT

#undef	SUBSUBTARGET_OVERRIDE_OPTIONS
#define	SUBSUBTARGET_OVERRIDE_OPTIONS				\
  do								\
    {								\
      if (!rs6000_explicit_options.alignment)			\
	rs6000_alignment_flags = MASK_ALIGN_NATURAL;		\
      if (TARGET_64BIT)						\
	{							\
	  if (DEFAULT_ABI != ABI_AIX)				\
	    {							\
	      rs6000_current_abi = ABI_AIX;			\
	      error (INVALID_64BIT, "call");			\
	    }							\
	  dot_symbols = !strcmp (rs6000_abi_name, "aixdesc");	\
	  if (target_flags & MASK_RELOCATABLE)			\
	    {							\
	      target_flags &= ~MASK_RELOCATABLE;		\
	      error (INVALID_64BIT, "relocatable");		\
	    }							\
	  if (target_flags & MASK_EABI)				\
	    {							\
	      target_flags &= ~MASK_EABI;			\
	      error (INVALID_64BIT, "eabi");			\
	    }							\
	  if (target_flags & MASK_PROTOTYPE)			\
	    {							\
	      target_flags &= ~MASK_PROTOTYPE;			\
	      error (INVALID_64BIT, "prototype");		\
	    }							\
	  if ((target_flags & MASK_POWERPC64) == 0)		\
	    {							\
	      target_flags |= MASK_POWERPC64;			\
	      error ("64 bit CPU required");			\
	    }							\
	}							\
    }								\
  while (0)


#undef	STARTFILE_DEFAULT_SPEC
#define STARTFILE_DEFAULT_SPEC "%(startfile_freebsd)"

#undef	ENDFILE_DEFAULT_SPEC
#define ENDFILE_DEFAULT_SPEC "%(endfile_freebsd)"

#undef	LIB_DEFAULT_SPEC
#define LIB_DEFAULT_SPEC "%(lib_freebsd)"

#undef	LINK_START_DEFAULT_SPEC
#define LINK_START_DEFAULT_SPEC "%(link_start_freebsd)"

#undef	LINK_OS_DEFAULT_SPEC
#define	LINK_OS_DEFAULT_SPEC "%(link_os_freebsd)"

/* XXX: This is wrong for many platforms in sysv4.h.
   We should work on getting that definition fixed.  */
#undef  LINK_SHLIB_SPEC
#define LINK_SHLIB_SPEC "%{shared:-shared} %{!shared: %{static:-static}}"


/************************[  Target stuff  ]***********************************/

/* Define the actual types of some ANSI-mandated types.  
   Needs to agree with <machine/ansi.h>.  GCC defaults come from c-decl.c,
   c-common.c, and config/<arch>/<arch>.h.  */

#undef  SIZE_TYPE
#define SIZE_TYPE	(TARGET_64BIT ? "long unsigned int" : "unsigned int")

#undef  PTRDIFF_TYPE
#define PTRDIFF_TYPE	(TARGET_64BIT ? "long int" : "int")

/* rs6000.h gets this wrong for FreeBSD.  We use the GCC defaults instead.  */
#undef WCHAR_TYPE

#undef  WCHAR_TYPE_SIZE
#define WCHAR_TYPE_SIZE 32

#undef  TARGET_VERSION
#define TARGET_VERSION fprintf (stderr, " (FreeBSD/PowerPC ELF)");

/* Override rs6000.h definition.  */
#undef  ASM_APP_ON
#define ASM_APP_ON "#APP\n"

/* Override rs6000.h definition.  */
#undef  ASM_APP_OFF
#define ASM_APP_OFF "#NO_APP\n"

/* _init and _fini functions are built from bits spread across many
   object files, each potentially with a different TOC pointer.  For
   that reason, place a nop after the call so that the linker can
   restore the TOC pointer if a TOC adjusting call stub is needed.  */
#ifdef __powerpc64__
#define CRT_CALL_STATIC_FUNCTION(SECTION_OP, FUNC)	\
  asm (SECTION_OP "\n"					\
"	bl ." #FUNC "\n"				\
"	nop\n"						\
"	.previous");
#endif

