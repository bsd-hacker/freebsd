# $FreeBSD$

# All locales to be created.
CCLN=           af_ZA am_ET be_BY bg_BG ca_ES cs_CZ da_DK de_CH \
		de_DE el_GR en_US es_ES et_EE eu_ES fi_FI fr_BE \
		fr_FR hi_IN hr_HR hu_HU hy_AM is_IS it_IT ja_JP \
		kk_KZ ko_KR lt_LT mn_MN nl_NL no_NO pl_PL pt_BR \
		pt_PT ro_RO ru_RU sk_SK sl_SI sr_Cyrl_YU sr_Latn_RS \
		sv_SE tr_TR uk_UA zh_CN

# For these locales, beside creating the UTF-8 map also create these
# charactermaps.
CMS_ca_ES=	ca_AD ca_FR ca_IT
CMS_de_DE=	de_AT
CMS_en_US=	en_AU en_CA en_GB en_NZ
CMS_fr_BE=	nl_BE
CMS_fr_FR=	fr_CA fr_CH
CMS_hi_IN=	ISCII-DEV
CMS_it_IT=	it_CH
CMS_no_NO=	nb_NO nn_NO

# For these locales, link the following charactermaps to the UTF-8 map.
# (unless the origin charactermap is defined)
CMSLINK_af_ZA=	ISO8859-1 ISO8859-15
CMSLINK_be_BY=	CP1131 CP1251 ISO8859-5
CMSLINK_bg_BG=	CP1251
CMSLINK_ca_ES=	ISO8859-1 ISO8859-15
CMSLINK_cs_CZ=	ISO8859-2
CMSLINK_da_DK=	ISO8859-1 ISO8859-15
CMSLINK_de_CH=	ISO8859-1 ISO8859-15
CMSLINK_de_DE=	ISO8859-1 ISO8859-15
CMSLINK_el_GR=	ISO8859-7
CMSLINK_en_US=	ISO8859-1 ISO8859-15 US-ASCII
CMSLINK_es_ES=	ISO8859-1 ISO8859-15
CMSLINK_et_EE=	ISO8859-15
CMSLINK_eu_ES=	ISO8859-1 ISO8859-15
CMSLINK_fi_FI=	ISO8859-1 ISO8859-15
CMSLINK_fr_FR=	ISO8859-1 ISO8859-15
CMSLINK_fr_BE=	ISO8859-1 ISO8859-15
CMSLINK_hr_HR=	ISO8859-2
CMSLINK_hu_HU=	ISO8859-2
CMSLINK_hy_AM=	ARMSCII-8
CMSLINK_is_IS=	ISO8859-1 ISO8859-15
CMSLINK_it_IT=	ISO8859-1 ISO8859-15
CMSLINK_ja_JP=	SJIS eucJP
CMSLINK_kk_KZ=	PT154
CMSLINK_ko_KR=	CP949 eucKR
CMSLINK_lt_LT=	ISO8859-4 ISO8859-13
CMSLINK_nl_NL=	ISO8859-1 ISO8859-15
CMSLINK_no_NO=	ISO8859-1 ISO8859-15
CMSLINK_pl_PL=	ISO8859-2
CMSLINK_pt_BR=	ISO8859-1
CMSLINK_pt_PT=	ISO8859-1 ISO8859-15
CMSLINK_ro_RO=	ISO8859-2
CMSLINK_ru_RU=	CP1251 CP886 ISO8859-5 KOI8-R
CMSLINK_sk_SK=	ISO8859-2
CMSLINK_si_SI=	ISO8859-2
CMSLINK_sr_Latn_RS=	ISO8859-2
CMSLINK_sr_Cyrl_RS=	ISO8859-5
CMSLINK_sv_SE=	ISO8859-1 ISO8859-15
CMSLINK_tr_TR=	ISO8859-9
CMSLINK_uk_UA=	CP1251 ISO8859-5 KOI8-U
CMSLINK_zh_Hans_CN=	GB18030 GB2312 GBK eucCN

# For these locales, also create symlinks to the main locale.
CCLNLINK_en_GB.UTF-8=	en_IE.UTF-8
CCLNLINK_en_US.UTF-8=	he_IL.UTF-8
CCLNLINK_zh_Hans_CN.UTF-8=	zh_Hant_HK.Big5HKSCS zh_Hant_HK.UTF-8 zh_Hant_TW.Big5 zh_Hant_TW.UTF-8

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

# Required variables
LCTYPE=		LC_NUMERIC
LOCALEDIR=	/usr/share/locale

# For testing only
DESTDIR=	/home/edwin/locale/new/
SHAREOWN=	edwin
SHAREGRP=	edwin

#
# All variables
#

.include "../Makefile.def.inc"
.include <bsd.prog.mk>
