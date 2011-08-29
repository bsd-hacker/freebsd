/* $FreeBSD$ */

#ifndef GLUE_H
#define GLUE_H

#include <limits.h>
#undef RE_DUP_MAX
#include <regex.h>

#define TRE_WCHAR			1
#define TRE_MULTIBYTE			1

#define tre_char_t			wchar_t
#define tre_mbrtowc(pwc, s, n, ps)	(mbrtowc((pwc), (s), (n), (ps)))
#define tre_strlen			wcslen
#define tre_isspace			iswspace
#define tre_isalnum			iswalnum

#define REG_LITERAL			0020
#define REG_WORD			0100
#define REG_GNU				0400

#define REG_OK				0

#define TRE_MB_CUR_MAX			MB_CUR_MAX

#define DPRINT(msg)			
#define MIN(a,b)			((a > b) ? (b) : (a))
#define MAX(a,b)			((a > b) ? (a) : (b))

typedef enum { STR_WIDE, STR_BYTE, STR_MBS, STR_USER } tre_str_type_t;
#endif
