# $FreeBSD: user/edwin/locale/share/msgdef/Makefile 196760 2009-09-02 10:12:13Z edwin $

#
# Predefined variables - Not for "end-users" to look at.
#

# aliases for charactermaps
CMALIAS_ISCII-DEV=	macdevanaga

# libiconv doesn't understand iscii-dev, bsdiconv does
ICONV=	/home/edwin/libiconv/bin/iconv
ICONV_hi_IN.ISCII-DEV=	/usr/bin/iconv

#
# Calculated variables
#

# All charactermaps
CMS=UTF-8
.for ccln in ${CCLN}
. if defined(CMS_${ccln})
.  for cm in ${CMS_${ccln}}
.   if ${CMS:M${cm}} == ""
CMS+=	${cm}
.   endif
.  endfor
. endif
.endfor

# All files generated and their locations
ALLFILES=	${CCLN:S/$/.UTF-8/}
.for ccln in ${CCLN}
FILESDIR_${ccln}.UTF-8.out= ${LOCALEDIR}/${ccln}.UTF-8
. if defined(CMS_${ccln})
.  for cm in ${CMS_${ccln}}
ALLFILES+=	${ccln}.${cm}
FILESDIR_${ccln}.${cm}.out= ${LOCALEDIR}/${ccln}.${cm}
.  endfor
. endif
.endfor

FILES=		${ALLFILES:S/$/.out/}
CLEANFILES=	${ALLFILES:S/$/.out/}
.if defined(FULL)
CLEANFILES+=	${CCLN:S/$/.src/}
.endif
FILESNAME=	${LCTYPE}

# All links
SYMLINKS=
.for ccln in ${CCLN}
. if defined(CMSLINK_${ccln})
.  for cm in ${CMSLINK_${ccln}}
SYMLINKS+=	../${ccln}.UTF-8/${LCTYPE} ${LOCALEDIR}/${ccln}.${cm}
.  endfor
. endif
. for cm in ${CMS}
.  if defined(CMSLINK_${ccln}.${cm})
.   for cclncm in ${CMSLINK_${ccln}.${cm}}
SYMLINKS+=	../${ccln}.${cm}/${LCTYPE} ${LOCALEDIR}/${cclncm}
.   endfor
.  endif
. endfor
.endfor

.for ccln in ${CCLN}

. if defined(LEGLINK_${ccln})
.  for _ccln in ${LEGLINK_${ccln}}
SYMLINKS+=	../${ccln}.UTF-8/${LCTYPE} ${LOCALEDIR}/${_ccln}.UTF-8
.   if defined(CMS_${ccln})
.    for cm in ${CMS_${ccln}}
SYMLINKS+=	../${ccln}.UTF-8/${LCTYPE} ${LOCALEDIR}/${_ccln}.${cm}
.    endfor
.   endif

.   if defined(CMSLINK_${ccln})
.    for cm in ${CMSLINK_${ccln}}
SYMLINKS+=	../${ccln}.UTF-8/${LCTYPE} ${LOCALEDIR}/${_ccln}.${cm}
.    endfor
.   endif

.   for cm in ${CMS}
.    if defined(CMSLINK_${ccln}.${cm})
.     for cclncm in ${CMSLINK_${ccln}.${cm}}
SYMLINKS+=	../${ccln}.${cm}/${LCTYPE} ${LOCALEDIR}/${cclncm}
.     endfor
.    endif
.   endfor
.  endfor
. endif

. for cm in ${CMS}
.  if defined(LEGLINK_${ccln}.${cm})
.   for cclncm in ${LEGLINK_${ccln}.${cm}}
SYMLINKS+=	../${ccln}.${cm}/${LCTYPE} ${LOCALEDIR}/${cclncm}
.   endfor
.  endif
. endfor

. if defined(CCLNLINK_${ccln})
.  for _ccln in ${CCLNLINK_${ccln}}
SYMLINKS+=	../${ccln}.UTF-8/${LCTYPE} ${LOCALEDIR}/${_ccln}.UTF-8
.   if defined(CMS_${ccln})
.    for cm in ${CMS_${ccln}}
SYMLINKS+=	../${ccln}.UTF-8/${LCTYPE} ${LOCALEDIR}/${_ccln}.${cm}
.    endfor
.   endif

.   if defined(CMSLINK_${ccln})
.    for cm in ${CMSLINK_${ccln}}
SYMLINKS+=	../${ccln}.UTF-8/${LCTYPE} ${LOCALEDIR}/${_ccln}.${cm}
.    endfor
.   endif

.   for cm in ${CMS}
.    if defined(CMSLINK_${ccln}.${cm})
.     for cclncm in ${CMSLINK_${ccln}.${cm}}
SYMLINKS+=	../${ccln}.${cm}/${LCTYPE} ${LOCALEDIR}/${cclncm}
.     endfor
.    endif
.   endfor
.  endfor
. endif

. for cm in ${CMS}
.  if defined(CCLNLINK_${ccln}.${cm})
.   for cclncm in ${CCLNLINK_${ccln}.${cm}}
SYMLINKS+=	../${ccln}.${cm}/${LCTYPE} ${LOCALEDIR}/${cclncm}
.   endfor
.  endif
. endfor

#
# Fill in the ICONV_ variables with the default values.
#
. if defined(CMS_${ccln})
.  for cms in ${CMS_${ccln}}
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
.endfor

#
# All targets
#

.for ccln in ${CCLN}

# Normal makes don't need to convert from .unicode to .src
. if defined(FULL)
${ccln}.src: ${ccln}.unicode
	../../cldr/tools/unicode2src.pl \
	    --cldr=/home/edwin/unicode/cldr/1.7.1 \
	    --unidata=/home/edwin/unicode/UNIDATA/5.1.0/ \
	    --input=${.ALLSRC} \
	    --output=${.TARGET}

. endif

${ccln}.UTF-8.out: ${ccln}.src
	grep -v '^#' < ${.ALLSRC} > ${.TARGET}

. if defined(CMS_${ccln})
.  for cms in ${CMS_${ccln}}

${ccln}.${cms}.out: ${ccln}.src
	${ICONV_${ccln}.${cms}} \
	    -f UTF-8 \
	    -t ${CMALIAS_${cms}} \
	    ${.ALLSRC} > ${.TARGET} \
	|| rm ${.TARGET} && exit 0

.  endfor
. endif
.endfor

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
