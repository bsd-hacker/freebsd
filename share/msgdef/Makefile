# $FreeBSD$

# All locales to be created.
CCLN=           af_ZA am_ET be_BY bg_BG ca_ES cs_CZ da_DK de_DE \
		el_GR en_CA en_GB en_US es_ES et_EE eu_ES fi_FI \
		fr_BE fr_CH fr_FR he_IL hr_HR hu_HU hy_AM is_IS \
		it_IT ja_JP kk_KZ ko_KR lt_LT mn_MN nl_NL no_NO \
		pl_PL pt_PT ro_RO ru_RU sl_SI sr_Cyrl_RS sr_Latn_RS \
		sv_SE tr_TR uk_UA zh_Hans_CN zh_Hant_TW
# hi_IN.ISCII-DEV.src

# For these locales, beside creating the UTF-8 map also create these
# charactermaps.
CMS_be_BY=	CP1131 CP1251 ISO8859-5
CMS_bg_BG=	CP1251
CMS_el_GR=	ISO8859-7
CMS_hy_AM=	ARMSCII-8
CMS_ja_JP=	SJIS eucJP
CMS_kk_KZ=	PT154
CMS_ko_KR=	eucKR
CMS_ru_RU=	CP1251 CP866 ISO8859-5 KOI8-R
CMS_sr_Cyrl_RS=	ISO8859-5
CMS_sr_Latn_RS=	ISO8859-2
CMS_uk_UA=	CP1251 ISO8859-5 KOI8-U
CMS_zh_Hans_CN=	GB18030 GB2312 eucCN
CMS_zh_Hant_TW=	Big5

# For these locales, link the following charactermaps to the UTF-8 map.
# (unless the origin charactermap is defined)
CMSLINK_af_ZA=	ISO8859-1 ISO8859-15
CMSLINK_ca_ES=	ISO8859-1 ISO8859-15
CMSLINK_cs_CZ=	ISO8859-2
CMSLINK_da_DK=	ISO8859-1 ISO8859-15
CMSLINK_de_DE=	ISO8859-1 ISO8859-15
CMSLINK_en_CA=	ISO8859-1 ISO8859-15 US-ASCII
CMSLINK_en_GB=	ISO8859-1 ISO8859-15 US-ASCII
CMSLINK_en_US=	ISO8859-1 ISO8859-15 US-ASCII
CMSLINK_es_ES=	ISO8859-1 ISO8859-15
CMSLINK_et_EE=	ISO8859-15
CMSLINK_eu_ES=	ISO8859-1 ISO8859-15
CMSLINK_fi_FI=	ISO8859-1 ISO8859-15
CMSLINK_fr_BE=	ISO8859-1 ISO8859-15
CMSLINK_fr_CH=	ISO8859-1 ISO8859-15
CMSLINK_fr_FR=	ISO8859-1 ISO8859-15
CMSLINK_hr_HR=	ISO8859-2
CMSLINK_hu_HU=	ISO8859-2
CMSLINK_is_IS=	ISO8859-1 ISO8859-15
CMSLINK_it_IT=	ISO8859-1 ISO8859-15
CMSLINK_ko_KR.eucKR=	ko_KR.CP949
CMSLINK_lt_LT=	ISO8859-4 ISO8859-13
CMSLINK_nl_NL=	ISO8859-1 ISO8859-15
CMSLINK_no_NO=	ISO8859-1 ISO8859-15
CMSLINK_pl_PL=	ISO8859-2
CMSLINK_pt_PT=	ISO8859-1 ISO8859-15
CMSLINK_ro_RO=	ISO8859-2
CMSLINK_sl_SI=	ISO8859-2
CMSLINK_sv_SE=	ISO8859-1 ISO8859-15
CMSLINK_tr_TR=	ISO8859-9
CMSLINK_zh_Hans_CN=	GBK
CMSLINK_zh_Hant_TW=	Big5HKSCS

# For these locales, also create symlinks to the main locale.
CCLNLINK_ca_ES=		ca_AD ca_FR ca_IT
CCLNLINK_cs_CZ=		sk_SK
CCLNLINK_de_DE=		de_AT de_CH
CCLNLINK_en_GB=		en_AU en_NZ
CCLNLINK_fr_FR=		fr_CA
CCLNLINK_it_IT=		it_CH
CCLNLINK_nl_NL=		nl_BE
CCLNLINK_no_NO=		nb_NO nn_NO
CCLNLINK_pt_PT=		pt_BR
CCLNLINK_zh_Hant_TW=	zh_Hant_HK
CCLNLINK_en_GB.UTF-8=	en_IE.UTF-8

# For these locales, create symlinks to the main locale for historical reasons.
LEGLINK_ja_JP=		jp_JP
LEGLINK_zh_Hans_CN=	zh_CN
LEGLINK_zh_Hant_TW=	zh_TW zh_HK
LEGLINK_sr_Cyrl_RS=	sr_YU
LEGLINK_sr_Latn_RS.ISO8859-2=	sr_YU.ISO8859-2
LEGLINK_zh_Hans_CN.GBK2312=	zh_CN.GBK

#
# In theory there are no parts after here which should be touched when adding
# or removing locales and charactermaps.
#

LOCALEDIR=	/usr/share/locale
LCTYPE=		LC_MESSAGES
DESTDIR=	/home/edwin/locale/new/
SHAREOWN=	edwin
SHAREGRP=	edwin

#
# All variables
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
CLEANFILES=	${ALLFILES:S/$/.out/} ${CCLN:S/$/.src/}
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

.endfor

#
# All targets
#

.for ccln in ${CCLN}
${ccln}.src: ${ccln}.unicode
	echo ${.ALLSRC} > ${.TARGET}
${ccln}.UTF-8.out: ${ccln}.src
	grep -v '^#' < ${.ALLSRC} > ${.TARGET}
.if defined(CMS_${ccln})
.for cms in ${CMS_${ccln}}
${ccln}.${cms}.out: ${ccln}.src
	grep -v '^#' < ${.ALLSRC} > ${.TARGET}
.endfor
.endif
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
