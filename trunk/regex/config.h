/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.ac by autoheader.  */

/* Define to 1 if you have `alloca', as a function or macro. */
#define HAVE_ALLOCA 1

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define to 1 if you have the <getopt.h> header file. */
#define HAVE_GETOPT_H 1

/* Define to 1 if you have the `getopt_long' function. */
#define HAVE_GETOPT_LONG 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the `isascii' function. */
#define HAVE_ISASCII 1

/* Define to 1 if you have the `isblank' function. */
#define HAVE_ISBLANK 1

/* Define to 1 if you have the `iswascii' function or macro. */
#define HAVE_ISWASCII 1

/* Define to 1 if you have the `iswblank' function or macro. */
#define HAVE_ISWBLANK 1

/* Define to 1 if you have the `iswctype' function or macro. */
#define HAVE_ISWCTYPE 1

/* Define to 1 if you have the `iswlower' function or macro. */
#define HAVE_ISWLOWER 1

/* Define to 1 if you have the `iswupper' function or macro. */
#define HAVE_ISWUPPER 1

/* Define to 1 if you have the `mbrtowc' function or macro. */
#define HAVE_MBRTOWC 1

/* Define to 1 if the system has the type `mbstate_t'. */
#define HAVE_MBSTATE_T 1

/* Define to 1 if you have the `mbtowc' function or macro. */
#define HAVE_MBTOWC 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the `towlower' function or macro. */
#define HAVE_TOWLOWER 1

/* Define to 1 if you have the `towupper' function or macro. */
#define HAVE_TOWUPPER 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if you have the <wchar.h> header file. */
#define HAVE_WCHAR_H 1

/* Define to 1 if the system has the type `wchar_t'. */
#define HAVE_WCHAR_T 1

/* Define to 1 if you have the `wcschr' function or macro. */
#define HAVE_WCSCHR 1

/* Define to 1 if you have the `wcscpy' function or macro. */
#define HAVE_WCSCPY 1

/* Define to 1 if you have the `wcslen' function or macro. */
#define HAVE_WCSLEN 1

/* Define to 1 if you have the `wcsncpy' function or macro. */
#define HAVE_WCSNCPY 1

/* Define to 1 if you have the `wcsrtombs' function or macro. */
#define HAVE_WCSRTOMBS 1

/* Define to 1 if you have the `wcstombs' function or macro. */
#define HAVE_WCSTOMBS 1

/* Define to 1 if you have the `wctype' function or macro. */
#define HAVE_WCTYPE 1

/* Define to 1 if you have the <wctype.h> header file. */
#define HAVE_WCTYPE_H 1

/* Define to 1 if the system has the type `wint_t'. */
#define HAVE_WINT_T 1

/* Define if you want to disable debug assertions. */
#define NDEBUG 1

/* If using the C implementation of alloca, define if you know the
   direction of stack growth for your system; otherwise it will be
   automatically deduced at runtime.
	STACK_DIRECTION > 0 => grows toward higher addresses
	STACK_DIRECTION < 0 => grows toward lower addresses
	STACK_DIRECTION = 0 => direction of growth unknown */
/* #undef STACK_DIRECTION */

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Define if you want to enable approximate matching functionality. */
#undef TRE_APPROX

/* Define to enable multibyte character set support. */
#define TRE_MULTIBYTE 1

/* Define to a field in the regex_t struct where TRE should store a pointer to
   the internal tre_tnfa_t structure */
#define TRE_REGEX_T_FIELD value

/* Define if you want TRE to use alloca() instead of malloc() when allocating
   memory needed for regexec operations. */
#define TRE_USE_ALLOCA 1

/* TRE version string. */
#define TRE_VERSION "0.8.0"

/* TRE version level 1. */
#define TRE_VERSION_1 0

/* TRE version level 2. */
#define TRE_VERSION_2 8

/* TRE version level 3. */
#define TRE_VERSION_3 0

/* Define to enable wide character (wchar_t) support. */
#define TRE_WCHAR 1

/* Version number of package */
#define VERSION "0.8.0"
