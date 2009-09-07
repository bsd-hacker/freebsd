# $FreeBSD$

# All locales to be created.
CCLN=		am_ET be_BY bg_BG ca_ES cs_CZ da_DK de_AT de_DE \
		el_GR en_GB en_US es_ES et_EE eu_ES fi_FI fr_FR \
		he_IL hi_IN hr_HR hu_HU hy_AM is_IS \
		it_IT ja_JP kk_KZ ko_KR la_LN lt_LT mn_MN nb_NO \
		nl_NL nn_NO pl_PL pt_BR pt_PT ro_RO ru_RU sk_SK \
		sl_SI sr_Cyrl_RS sr_Latn_RS sv_SE tr_TR uk_UA \
		zh_Hans_CN zh_Hant_TW

# For these locales, beside creating the UTF-8 map also create these
# charactermaps.
CMS_be_BY=	CP1131 CP1251 ISO8859-5
CMS_bg_BG=	CP1251
CMS_ca_ES=	ISO8859-1
CMS_cs_CZ=	ISO8859-2
CMS_da_DK=	ISO8859-1
CMS_de_AT=	ISO8859-1
CMS_de_DE=	ISO8859-1
CMS_el_GR=	ISO8859-7
CMS_en_GB=	ISO8859-1
CMS_en_US=	ISO8859-1
CMS_es_ES=	ISO8859-1
CMS_et_EE=	ISO8859-15
CMS_eu_ES=	ISO8859-1
CMS_fi_FI=	ISO8859-1
CMS_fr_FR=	ISO8859-1
CMS_hi_IN=	ISCII-DEV
CMS_hr_HR=	ISO8859-2
CMS_hu_HU=	ISO8859-2
CMS_hy_AM=	ARMSCII-8
CMS_is_IS=	ISO8859-1
CMS_it_IT=	ISO8859-1
CMS_ja_JP=	SJIS eucJP
CMS_kk_KZ=	PT154
CMS_ko_KR=	eucKR
CMS_la_LN=	ISO8859-1
CMS_lt_LT=	ISO8859-4 ISO8859-13
CMS_nb_NO=	ISO8859-1
CMS_nl_NL=	ISO8859-1
CMS_nn_NO=	ISO8859-1
CMS_pl_PL=	ISO8859-2
CMS_pt_BR=	ISO8859-1
CMS_pt_PT=	ISO8859-1
CMS_ro_RO=	ISO8859-2
CMS_ru_RU=	CP1251 CP866 ISO8859-5 KOI8-R
CMS_sk_SK=	ISO8859-2
CMS_sl_SI=	ISO8859-2
CMS_sr_Cyrl_RS=	ISO8859-5
CMS_sr_Latn_RS=	ISO8859-2
CMS_sv_SE=	ISO8859-1
CMS_tr_TR=	ISO8859-9
CMS_uk_UA=	CP1251 ISO8859-5 KOI8-U
CMS_zh_Hans_CN=	GB18030 GB2312 eucCN
CMS_zh_Hant_TW=	Big5

# For these locales, link the following charactermaps to the UTF-8 map.
# (unless the origin charactermap is defined)
CMSLINK_af_ZA=	ISO8859-1 ISO8859-15
CMSLINK_ca_ES.ISO8859-1=	ca_ES.ISO8859-15
CMSLINK_da_DK.ISO8859-1=	ca_ES.ISO8859-15
CMSLINK_de_AT.ISO8859-1=	de_AT.ISO8859-15
CMSLINK_de_DE.ISO8859-1=	de_DE.ISO8859-15
CMSLINK_en_GB=	ISO8859-1 ISO8859-15 US-ASCII
CMSLINK_en_US=	ISO8859-1 ISO8859-15 US-ASCII
CMSLINK_es_ES=	ISO8859-1
CMSLINK_eu_ES=	ISO8859-1 ISO8859-15
CMSLINK_fi_FI.ISO8859-1=	fi_FI.ISO8859-15
CMSLINK_fr_FR.ISO8859-1=	fr_FR.ISO8859-15
CMSLINK_is_IS.ISO8859-1=	is_IS.ISO8859-15
CMSLINK_it_IT.ISO8859-1=	it_IT.ISO8859-15
CMSLINK_ko_KR.eucKR=	ko_KR.CP949
CMSLINK_la_LN.ISO8859-1=	la_LN.ISO8859-2 la_LN.ISO8859-4 la_LN.ISO8859-15 la_LN.US-ASCII
CMSLINK_nb_NO.ISO8859-1=	nb_NO.ISO8859-15
CMSLINK_nl_NL=	ISO8859-1 ISO8859-15
CMSLINK_nn_NO.ISO8859-1=	nn_NO.ISO8859-15
CMSLINK_pt_PT.ISO8859-1=	pt_PT.ISO8859-15
CMSLINK_sv_SE.ISO8859-1=	sv_SE.ISO8859-15
CMSLINK_zh_Hans_CN.GB2312=	zh_Hans_CN.GBK

# For these locales, also create symlinks to the main locale.
CCLNLINK_en_US.UTF-8=	af_ZA.UTF-8
CCLNLINK_ca_ES=	ca_AD ca_IT ca_IT
CCLNLINK_de_DE=	de_CH
CCLNLINK_en_GB=	en_AU en_CA en_NZ
CCLNLINK_en_IE.UTF-8=	en_GB.UTF-8
CCLNLINK_fr_FR=	fr_BE fr_CA fr_CH
CCLNLINK_it_IT=	it_CH
CCLNLINK_nl_NL=	nl_BE
CCLNLINK_nb_NO=	no_NO
CCLNLINK_zh_Hant_TW=	zh_Hant_HK

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
LCTYPE=		LC_TIME
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
