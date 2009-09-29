# $FreeBSD$

# All locales to be created.
CCLN=           af_ZA am_ET be_BY bg_BG ca_ES cs_CZ da_DK de_AT \
		de_CH de_DE el_GR en_AU en_CA en_GB en_IE en_NZ \
		en_US es_ES et_EE fi_FI fr_BE fr_CA fr_FR he_IL \
		hi_IN hr_HR hu_HU hy_AM is_IS it_IT ja_JP kk_KZ \
		ko_KR lt_LT lv_LV mn_MN nl_BE nl_NL no_NO pl_PL pt_BR \
		pt_PT ro_RO ru_RU sk_SK sl_SI sr_Cyrl_RS sr_Latn_RS \
		sv_SE tr_TR uk_UA zh_Hans_CN zh_Hant_HK zh_Hant_TW

# For these locales, beside creating the UTF-8 map also create these
# charactermaps.
CMS_be_BY=	CP1131 CP1251 ISO8859-5
CMS_bg_BG=	CP1251
CMS_cs_CZ=	ISO8859-2
CMS_bg_BG=	CP1251
CMS_en_GB=	ISO8859-1
CMS_hi_IN=	ISCII-DEV
CMS_hy_AM=	ARMSCII-8
CMS_ja_JP=	eucJP
CMS_kk_KZ=	PT154
CMS_ko_KR=	eucKR
CMS_lv_LV=	ISO8859-13
CMS_pl_PL=	ISO8859-2
CMS_pt_PT=	ISO8859-1 ISO8859-15
CMS_ru_RU=	CP1251 CP866 ISO8859-5 KOI8-R
CMS_uk_UA=	CP1251 ISO8859-5 KOI8-U
CMS_sr_Cyrl_RS=	ISO8859-5
CMS_zh_Hans_CN=	GB18030 GB2312 eucCN

# For these locales, link the following charactermaps to the UTF-8 map.
# (unless the origin charactermap is defined)
CMSLINK_af_ZA=	ISO8859-1 ISO8859-15
CMSLINK_ca_ES=	ISO8859-1 ISO8859-15
CMSLINK_da_DK=	ISO8859-1 ISO8859-15
CMSLINK_de_AT=	ISO8859-1 ISO8859-15
CMSLINK_de_CH=	ISO8859-1 ISO8859-15
CMSLINK_de_DE=	ISO8859-1 ISO8859-15
CMSLINK_el_GR=	ISO8859-7
CMSLINK_en_AU=	ISO8859-1 ISO8859-15 US-ASCII
CMSLINK_en_CA=	ISO8859-1 ISO8859-15 US-ASCII
CMSLINK_en_GB.ISO8859-1=	en_GB.ISO8859-15 en_GB.US-ASCII
CMSLINK_en_NZ=	ISO8859-1 ISO8859-15 US-ASCII
CMSLINK_en_US=	ISO8859-1 ISO8859-15 US-ASCII
CMSLINK_es_ES=	ISO8859-1 ISO8859-15
CMSLINK_et_EE=	ISO8859-15
CMSLINK_fi_FI=	ISO8859-1 ISO8859-15
CMSLINK_fr_BE=	ISO8859-1 ISO8859-15
CMSLINK_fr_CA=	ISO8859-1 ISO8859-15
CMSLINK_fr_FR=	ISO8859-1 ISO8859-15
CMSLINK_hr_HR=	ISO8859-2
CMSLINK_hu_HU=	ISO8859-2
CMSLINK_is_IS=	ISO8859-1 ISO8859-15
CMSLINK_it_IT=	ISO8859-1 ISO8859-15
CMSLINK_ja_JP.eucJP=	ja_JP.SJIS
CMSLINK_ko_KR.eucKR=	ko_KR.CP949
CMSLINK_lt_LT=	ISO8859-4 ISO8859-13
CMSLINK_nl_BE=	ISO8859-1 ISO8859-15
CMSLINK_nl_NL=	ISO8859-1 ISO8859-15
CMSLINK_no_NO=	ISO8859-1 ISO8859-15
CMSLINK_pt_BR=	ISO8859-1
CMSLINK_ro_RO=	ISO8859-2
CMSLINK_sk_SK=	ISO8859-2
CMSLINK_sl_SI=	ISO8859-2
CMSLINK_sr_Latn_RS=	ISO8859-2
CMSLINK_sv_SE=	ISO8859-1 ISO8859-15
CMSLINK_tr_TR=	ISO8859-9
CMSLINK_zh_Hans_CN.GB2312=	zh_Hans_CN.GBK
CMSLINK_zh_Hant_HK=	Big5HKSCS
CMSLINK_zh_Hant_TW=	Big5

# For these locales, also create symlinks to the main locale.
CCLNLINK_ca_ES=	ca_AD ca_FR ca_IT
CCLNLINK_es_ES=	eu_ES
CCLNLINK_de_CH=	fr_CH it_CH
CCLNLINK_no_NO=	nb_NO nn_NO

# Some locales are not fully one-on-one, therefore need to be
# translated manually (multiple allowed)
TRANSLATIONBEFORE_eucKR=	E282A9:5C	# Won sign
TRANSLATIONBEFORE_ISO8859-1=	E282AC:4575	# Euro sign

# For these locales, create symlinks to the main locale for historical reasons.
LEGLINK_ja_JP=		jp_JP
LEGLINK_sr_Latn_RS.UTF-8=	sr_YU.UTF-8 sr_YU.ISO8859-2
LEGLINK_sr_Cyrl_RS.ISO8859-5=	sr_YU.ISO8859-5
LEGLINK_zh_Hans_CN=	zh_CN
LEGLINK_zh_Hant_TW=	zh_TW
LEGLINK_zh_Hant_HK=	zh_HK
LEGLINK_zh_Hans_CN.GBK2312=	zh_CN.GBK

#
# In theory there are no parts after here which should be touched when adding
# or removing locales and charactermaps.
#

# Required variables
LCTYPE=		LC_MONETARY
LOCALEDIR=	/usr/share/locale
#ICONV_pt_PT.ISO8859-1=	bsdiconv
#ICONV_ko_KR.eucKR=	bsdiconv

#
# All variables
#

.include "../Makefile.def.inc"
.include <bsd.prog.mk>
