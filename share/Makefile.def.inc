# $FreeBSD: user/edwin/locale/share/msgdef/Makefile 196760 2009-09-02 10:12:13Z edwin $

.if defined(LOCALE_DESTDIR)
DESTDIR=	${LOCALE_DESTDIR}
.endif
.if defined(LOCALE_SHAREOWN)
SHAREOWN=	${LOCALE_SHAREOWN}
.endif
.if defined(LOCALE_SHAREGRP)
SHAREGRP=	${LOCALE_SHAREGRP}
.endif

#
# Predefined variables - Not for "end-users" to look at.
#

# aliases for charactermaps
CMALIAS_ISCII-DEV=	macdevanaga

# libiconv doesn't understand iscii-dev, bsdiconv does
ICONV=	iconv
ICONV_hi_IN.ISCII-DEV=	bsdiconv

# Target selection
TARGET_LC_MESSAGES=	TARGET_CHARACTERMAP
TARGET_LC_MONETARY=	TARGET_CHARACTERMAP
TARGET_LC_NUMERIC=	TARGET_CHARACTERMAP
TARGET_LC_TIME=		TARGET_CHARACTERMAP
TARGET_LC_COLLATE=	TARGET_COLLDEF
.if !defined(TARGET_${LCTYPE})
.error unknown LCTYPE (${LCTYPE}) or not set.
.endif
TARGET=	${TARGET_${LCTYPE}}

#
# Calculated variables
#

# All charactermaps
CMS=UTF-8
.for ccln in ${CCLN}
. for cm in ${CMS_${ccln}}
.  if ${CMS:M${cm}} == ""
CMS+=	${cm}
.  endif
. endfor
. for cm in ${CMSLINK_${ccln}}
.  if ${CMS:M${cm}} == ""
CMS+=	${cm}
.  endif
. endfor
.endfor

# All files generated and their locations. Default UTF-8 is only for
# the TARGET_CHARACTERMAP, not for the TARGET_COLLDEF.
.if ${TARGET} == "TARGET_CHARACTERMAP"
ALLFILES=	${CCLN:S/$/.UTF-8/}
.endif
.for ccln in ${CCLN}
FILESDIR_${ccln}.UTF-8.out= ${LOCALEDIR}/${ccln}.UTF-8
. for cm in ${CMS_${ccln}}
ALLFILES+=	${ccln}.${cm}
FILESDIR_${ccln}.${cm}.out= ${LOCALEDIR}/${ccln}.${cm}
. endfor
.endfor

FILES=		${ALLFILES:S/$/.out/}
CLEANFILES=	${ALLFILES:S/$/.out/}
.if defined(FULL) && ${TARGET} == "TARGET_CHARACTERMAP"
CLEANFILES=	${ALLFILES:S/$/.src/}
.endif
FILESNAME=	${LCTYPE}

# All links
#
# CMS_ca_ES=		ISO8859-1
# CMSLINK_ca_ES=	ISO8859-2
# CMSLINK_ca_ES.ISO8859-1=	ISO8859-15
# CCLNLINK_ca_ES=	ca_FR
# CCLNLINK_ca_ES.UTF-8=	ca_IT.UTF-8
# LEGLINK_ca_ES=	ca_CA
#
# First create the direct CMSLINK symlinks:
#
# foreach cm in CMSLINK_ca_ES
#   ca_ES.cm		-> ca_ES.UTF-8
# endfor
# foreach cm in CMS
#   if defined(CMSLINK_ca_ES.cm)
#     foreach cmi in CMSLINK_ca_ES.cm
#       ca_ES.cmi	-> ca_ES.cm
#     endfor
#   endif
# endfor
#
# Next create the direct CCNLNLINK symlinks:
#
# foreach ccln in CCLNLINK_ca_ES
#   foreach cm in CMSLINK_ca_ES
#     ccln.cm		-> ca_ES.UTF-8
#   endfor
# endfor
# foreach ccln in CCLNLINK_ca_ES
#   foreach cm in CMS
#     if defined(CMSLINK_ca_ES.cm)
#       foreach cmi in CMSLINK_ca_ES.cm
#         ccln.cmi	-> ca_ES.cm
#       endfor
#     endif
#   endfor
# endfor
#
# And the per-charactermap CCNLNLINK symlinks:
#
# foreach ccln in CCLN
#   foreach cm in CMS
#     if defined(CCLNLINK_ccln.cm)
#	foreach cclncm in CCLNLINK_ccln.cm
#	  cclncm	-> ccln.cm
#	endfor
#     endif
#   endfor
# endfor
#
# af_ZA.UTF-8
# af_ZA.ISO8859-1
# af_ZA.ISO8859-15	-> af_ZA.ISO8859-1
# af_ZA.ISO8859-2	-> af_ZA.UTF-8
#
# af_ZB.UTF-8		-> af_ZA.UTF-8
# af_ZB.ISO8859-1	-> af_ZA.ISO8859-1
# af_ZB.ISO8859-15	-> af_ZB.ISO8895-1
# af_ZB.ISO8859-2	-> af_ZB.UTF-8
#
# af_ZC.UTF-8		-> af_ZA.UTF-8
#
# af_ZD.UTF-8		-> af_ZA.UTF-8
# af_ZD.ISO8859-1	-> af_ZA.ISO8859-1
# af_ZD.ISO8859-15	-> af_ZD.ISO8895-1
# af_ZD.ISO8859-2	-> af_ZD.UTF-8
#

# LEGLINK_xxXX is nothing more than a CCLNLINK_xxXX
.for ccln in ${CCLN}
. for _ccln in ${LEGLINK_${ccln}}
CCLNLINK_${ccln}+=	${_ccln}
. endfor
. for cm in ${CMS}
.  ifdef LEGLINK_${ccln}.${cm}
CCLNLINK_${ccln}.${cm}+=	${LEGLINK_${ccln}.${cm}}
.  endif
. endfor
.endfor

#
# First create the direct CMSLINK symlinks:
#
# foreach cm in CMSLINK_ca_ES
#   ca_ES.cm		-> ca_ES.UTF-8
# endfor
.for ccln in ${CCLN}
. for cm in ${CMSLINK_${ccln}}
SYMLINKS+=	../${ccln}.UTF-8/${LCTYPE} ${LOCALEDIR}/${ccln}.${cm}
. endfor
.endfor

# foreach cm in CMS
#   if defined(CMSLINK_ca_ES.cm)
#     foreach cmi in CMSLINK_ca_ES.cm
#       ca_ES.cmi	-> ca_ES.cm
#     endfor
#   endif
# endfor
.for ccln in ${CCLN}
. for cm in ${CMS}
.  ifdef CMSLINK_${ccln}.${cm}
.   for cmi in ${CMSLINK_${ccln}.${cm}}
SYMLINKS+=	../${ccln}.${cm}/${LCTYPE} ${LOCALEDIR}/${ccln}.${cmi}
.   endfor
.  endif
. endfor
.endfor

#
# Next create the direct CCNLNLINK symlinks:
#
# foreach ccln in CCLNLINK_ca_ES
#   ccln.UTF-8		-> ca_ES.UTF-8
#   foreach cm in CMSLINK_ca_ES
#     ccln.cm		-> ca_ES.UTF-8
#   endfor
# endfor
.for ccln in ${CCLN}
. for cclni in ${CCLNLINK_${ccln}}
SYMLINKS+=	../${ccln}.UTF-8/${LCTYPE} ${LOCALEDIR}/${cclni}.UTF-8
.  for cm in ${CMS_${ccln}}
SYMLINKS+=	../${ccln}.${cm}/${LCTYPE} ${LOCALEDIR}/${cclni}.${cm}
.  endfor
.  for cm in ${CMSLINK_${ccln}}
SYMLINKS+=	../${ccln}.UTF-8/${LCTYPE} ${LOCALEDIR}/${cclni}.${cm}
.  endfor
. endfor
.endfor
# foreach ccln in CCLNLINK_ca_ES
#   foreach cm in CMS
#     if defined(CMSLINK_ca_ES.cm)
#       foreach cclncmi in CMSLINK_ca_ES.cm
#         cclncmi(ca_ES -> ccln)	-> ca_ES.cm
#       endfor
#     endif
#   endfor
# endfor
.for ccln in ${CCLN}
. for cclni in ${CCLNLINK_${ccln}}
.  for cm in ${CMS}
.   ifdef CMSLINK_${ccln}.${cm}
.    for cmi in ${CMSLINK_${ccln}.${cm}}
SYMLINKS+=	../${ccln}.${cm}/${LCTYPE} ${LOCALEDIR}/${cclni}.${cmi}
.    endfor
.   endif
.  endfor
. endfor
.endfor

#
# And the per-charactermap CCNLNLINK symlinks:
#
# foreach ccln in CCLN
#   foreach cm in CMS
#     if defined(CCLNLINK_ccln.cm)
#	foreach cclncm in CCLNLINK_ccln.cm
#	  cclncm	-> ccln.cm
#	endfor
#     endif
#   endfor
# endfor
.for ccln in ${CCLN}
. for cm in ${CMS}
.  ifdef CCLNLINK_${ccln}.${cm}
.   for cclncm in ${CCLNLINK_${ccln}.${cm}}
SYMLINKS+=	../${ccln}.${cm}/${LCTYPE} ${LOCALEDIR}/${cclncm}
.   endfor
.  endif
. endfor
.endfor

#
# Fill in the ICONV_ variables with the default values.
#
.for ccln in ${CCLN}
. if defined(CMS_${ccln})
.  for cms in ${CMS_${ccln}}
.   if !defined(ICONV_${ccln}.${cms})
ICONV_${ccln}.${cms}=	${ICONV}
.   endif
.  endfor
. endif
. if defined(CMSLINK_${ccln})
.  for cms in ${CMSLINK_${ccln}}
.   if !defined(ICONV_${ccln}.${cms})
ICONV_${ccln}.${cms}=	${ICONV}
.   endif
.  endfor
. endif
.endfor

.for cm in ${CMS}
. if !defined(CMALIAS_${cm})
CMALIAS_${cm}=	${cm}
. endif

. if !defined(TRANSLATIONBEFORE_${cm})
TRANSLATIONBEFORE_${cm}=
. endif
. if !defined(TRANSLATIONAFTER_${cm})
TRANSLATIONAFTER_${cm}=
. endif
. for TR in ${TRANSLATIONBEFORE_${cm}}
_TRANSLATIONBEFORE_${cm}+=	 | awk '{ gsub("${TR:C/:.*//:C/([0-9a-fA-F][0-9a-fA-F])/\\\\x\1/g}", "${TR:C/^.*://:C/([0-9a-fA-F][0-9a-fA-F])/\\\\x\1/g}"); print }'
. endfor
. for TR in ${TRANSLATIONAFTER_${cm}}
_TRANSLATIONAFTER_${cm}+=	 | awk '{ gsub("${TR:C/:.*//:C/([0-9a-fA-F][0-9a-fA-F])/\\\\x\1/g}", "${TR:C/^.*://:C/([0-9a-fA-F][0-9a-fA-F])/\\\\x\1/g}"); print }'
. endfor
.endfor

#
# All targets for TARGET_CHARACTERMAP
#
# .unicode -> utf-8.src -> utf-8.out
#                 \__ iso8859-1.src -> iso8859-1.out
# <----1---><--2---><------3--------><----4----->
#
# 1. The files .unicode are stored in the SCM and are the source
#    for the whole further system
# 2. The Perl script converts the .unicode files and the Unicode
#    CLDR database into UTF-8 code
# 3. The UTF-8 gets converted by libiconv or bsdiconv in the specific
#    charactermap.
# 4. Get rid of the comments.
#
# As long as there is no bsdiconv, the files with the extension
# .unicode and .src must be stored in the SCM and will not be
# generated as part of the build process.
#
.if ${TARGET} == "TARGET_CHARACTERMAP"
. for ccln in ${CCLN}

# Normal makes don't need to convert from .unicode to .src
.  if defined(FULL)
${ccln}.UTF-8.src: ${ccln}.unicode
	../../usr.bin/unicode2utf8/unicode2utf8 \
	    --cldr=${CLDRDIR} \
	    --input=${.ALLSRC} \
	    --output=${.TARGET}
.  endif

${ccln}.UTF-8.out: ${ccln}.UTF-8.src
	grep -v '^#' < ${.ALLSRC} > ${.TARGET}

.  if defined(CMS_${ccln})
.   for cms in ${CMS_${ccln}}
.    if defined(FULL)

${ccln}.${cms}.src: ${ccln}.UTF-8.src
	cat ${.ALLSRC} \
	${_TRANSLATIONBEFORE_${cms}} | \
	${ICONV_${ccln}.${cms}} \
	    -f UTF-8 \
	    -t ${CMALIAS_${cms}} \
	${_TRANSLATIONAFTER_${cms}} \
	    > ${.TARGET} \
	|| (rm ${.TARGET} && exit 1)
.    endif

${ccln}.${cms}.out: ${ccln}.${cms}.src
	grep -v '^#' < ${.ALLSRC} > ${.TARGET}

.   endfor
.  endif
. endfor
.endif

#
# All targets for TARGET_COLLDEF
#
.if ${TARGET} == "TARGET_COLLDEF"
. for ccln in ${CCLN}
.  for cm in ${CMS_${ccln}}
${ccln}.${cm}.out: ${ccln}.${cm}.src
	colldef -I ${.CURDIR} -o ${.TARGET} ${.ALLSRC}
.  endfor
. endfor
.endif

#
# General targets.
#
beforeinstall:
.for f in ${FILES:S/.out//}
	rm -f ${DESTDIR}/${LOCALEDIR}/${f}/${FILESNAME}
.endfor
	set ${SYMLINKS}; \
	while [ ! -z "$1" ]; do \
		shift; \
		rm -f ${DESTDIR}/$$1/${FILESNAME}; \
		shift; \
	done

.include <bsd.prog.mk>
