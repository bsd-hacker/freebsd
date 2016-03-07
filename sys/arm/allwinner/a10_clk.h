/*-
 * Copyright (c) 2013 Ganbold Tsagaankhuu <ganbold@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef	_A10_CLK_H_
#define	_A10_CLK_H_

#define	CCM_PLL1_CFG		0x0000
#define	CCM_PLL1_TUN		0x0004
#define	CCM_PLL2_CFG		0x0008
#define	CCM_PLL2_TUN		0x000c
#define	CCM_PLL3_CFG		0x0010
#define	CCM_PLL3_TUN		0x0014
#define	CCM_PLL4_CFG		0x0018
#define	CCM_PLL4_TUN		0x001c
#define	CCM_PLL5_CFG		0x0020
#define	CCM_PLL5_TUN		0x0024
#define	CCM_PLL6_CFG		0x0028
#define	CCM_PLL6_TUN		0x002c
#define	CCM_PLL7_CFG		0x0030
#define	CCM_PLL7_TUN		0x0034
#define	CCM_PLL1_TUN2		0x0038
#define	CCM_PLL5_TUN2		0x003c
#define	CCM_PLL_LOCK_DBG	0x004c
#define	CCM_OSC24M_CFG		0x0050
#define	CCM_CPU_AHB_APB0_CFG	0x0054
#define	CCM_APB1_CLK_DIV	0x0058
#define	CCM_AXI_GATING		0x005c
#define	CCM_AHB_GATING0		0x0060
#define	CCM_AHB_GATING1		0x0064
#define	CCM_APB0_GATING		0x0068
#define	CCM_APB1_GATING		0x006c
#define	CCM_NAND_SCLK_CFG	0x0080
#define	CCM_MS_SCLK_CFG		0x0084
#define	CCM_MMC0_SCLK_CFG	0x0088
#define	CCM_MMC1_SCLK_CFG	0x008c
#define	CCM_MMC2_SCLK_CFG	0x0090
#define	CCM_MMC3_SCLK_CFG	0x0094
#define	CCM_TS_CLK		0x0098
#define	CCM_SS_CLK		0x009c
#define	CCM_SPI0_CLK		0x00a0
#define	CCM_SPI1_CLK		0x00a4
#define	CCM_SPI2_CLK		0x00a8
#define	CCM_PATA_CLK		0x00ac
#define	CCM_IR0_CLK		0x00b0
#define	CCM_IR1_CLK		0x00b4
#define	CCM_IIS_CLK		0x00b8
#define	CCM_AC97_CLK		0x00bc
#define	CCM_SPDIF_CLK		0x00c0
#define	CCM_KEYPAD_CLK		0x00c4
#define	CCM_SATA_CLK		0x00c8
#define	CCM_USB_CLK		0x00cc
#define	CCM_GPS_CLK		0x00d0
#define	CCM_SPI3_CLK		0x00d4
#define	CCM_DRAM_CLK		0x0100
#define	CCM_BE0_SCLK		0x0104
#define	CCM_BE1_SCLK		0x0108
#define	CCM_FE0_CLK		0x010c
#define	CCM_FE1_CLK		0x0110
#define	CCM_MP_CLK		0x0114
#define	CCM_LCD0_CH0_CLK	0x0118
#define	CCM_LCD1_CH0_CLK	0x011c
#define	CCM_CSI_ISP_CLK		0x0120
#define	CCM_TVD_CLK		0x0128
#define	CCM_LCD0_CH1_CLK	0x012c
#define	CCM_LCD1_CH1_CLK	0x0130
#define	CCM_CS0_CLK		0x0134
#define	CCM_CS1_CLK		0x0138
#define	CCM_VE_CLK		0x013c
#define	CCM_AUDIO_CODEC_CLK	0x0140
#define	CCM_AVS_CLK		0x0144
#define	CCM_ACE_CLK		0x0148
#define	CCM_LVDS_CLK		0x014c
#define	CCM_HDMI_CLK		0x0150
#define	CCM_MALI400_CLK		0x0154
#define	CCM_GMAC_CLK		0x0164

#define	CCM_GMAC_CLK_DELAY_SHIFT	10
#define	CCM_GMAC_CLK_MODE_MASK	0x7
#define	CCM_GMAC_MODE_RGMII	(1 << 2)
#define	CCM_GMAC_CLK_MII	0x0
#define	CCM_GMAC_CLK_EXT_RGMII	0x1
#define	CCM_GMAC_CLK_RGMII	0x2

/* APB0_GATING */
#define	CCM_APB0_GATING_ADDA	(1 << 0)

/* AHB_GATING_REG0 */
#define	CCM_AHB_GATING_USB0	(1 << 0)
#define	CCM_AHB_GATING_EHCI0	(1 << 1)
#define	CCM_AHB_GATING_OHCI0	(1 << 2)
#define	CCM_AHB_GATING_EHCI1	(1 << 3)
#define	CCM_AHB_GATING_OHCI1	(1 << 4)
#define	CCM_AHB_GATING_DMA	(1 << 6)
#define	CCM_AHB_GATING_SDMMC0	(1 << 8)
#define	CCM_AHB_GATING_EMAC	(1 << 17)
#define	CCM_AHB_GATING_SATA	(1 << 25)

/* AHB_GATING_REG1 */
#define	CCM_AHB_GATING_GMAC	(1 << 17)
#define	CCM_AHB_GATING_DE_BE1	(1 << 13)
#define	CCM_AHB_GATING_DE_BE0	(1 << 12)
#define	CCM_AHB_GATING_HDMI	(1 << 11)
#define	CCM_AHB_GATING_LCD1	(1 << 5)
#define	CCM_AHB_GATING_LCD0	(1 << 4)

/* APB1_GATING_REG */
#define	CCM_APB1_GATING_TWI	(1 << 0)

/* USB */
#define	CCM_USB_PHY		(1 << 8)
#define	CCM_SCLK_GATING_OHCI1	(1 << 7)
#define	CCM_SCLK_GATING_OHCI0	(1 << 6)
#define	CCM_USBPHY2_RESET	(1 << 2)
#define	CCM_USBPHY1_RESET	(1 << 1)
#define	CCM_USBPHY0_RESET	(1 << 0)

#define	CCM_PLL_CFG_ENABLE	(1U << 31)
#define	CCM_PLL_CFG_BYPASS	(1U << 30)
#define	CCM_PLL_CFG_PLL5	(1U << 25)
#define	CCM_PLL_CFG_PLL6	(1U << 24)
#define	CCM_PLL_CFG_FACTOR_N		0x1f00
#define	CCM_PLL_CFG_FACTOR_N_SHIFT	8
#define	CCM_PLL_CFG_FACTOR_K		0x30
#define	CCM_PLL_CFG_FACTOR_K_SHIFT	4
#define	CCM_PLL_CFG_FACTOR_M		0x3

#define	CCM_PLL2_CFG_POSTDIV		0x3c000000
#define	CCM_PLL2_CFG_POSTDIV_SHIFT	26
#define	CCM_PLL2_CFG_PREDIV		0x1f
#define	CCM_PLL2_CFG_PREDIV_SHIFT	0

#define	CCM_PLL3_CFG_MODE_SEL_SHIFT	15
#define	CCM_PLL3_CFG_MODE_SEL_FRACT	(0 << CCM_PLL3_CFG_MODE_SEL_SHIFT)
#define	CCM_PLL3_CFG_MODE_SEL_INT	(1 << CCM_PLL3_CFG_MODE_SEL_SHIFT)
#define	CCM_PLL3_CFG_FUNC_SET_SHIFT	14
#define	CCM_PLL3_CFG_FUNC_SET_270MHZ	(0 << CCM_PLL3_CFG_FUNC_SET_SHIFT)
#define	CCM_PLL3_CFG_FUNC_SET_297MHZ	(1 << CCM_PLL3_CFG_FUNC_SET_SHIFT)
#define	CCM_PLL3_CFG_FACTOR_M		0x7f

#define	CCM_PLL5_CFG_OUT_EXT_DIV_P		0x30000
#define	CCM_PLL5_CFG_OUT_EXT_DIV_P_SHIFT	16

#define	CCM_PLL6_CFG_SATA_CLKEN	(1U << 14)

#define	CCM_SD_CLK_SRC_SEL		0x3000000
#define	CCM_SD_CLK_SRC_SEL_SHIFT	24
#define	CCM_SD_CLK_SRC_SEL_OSC24M	0
#define	CCM_SD_CLK_SRC_SEL_PLL6		1
#define	CCM_SD_CLK_PHASE_CTR		0x700000
#define	CCM_SD_CLK_PHASE_CTR_SHIFT	20
#define	CCM_SD_CLK_DIV_RATIO_N		0x30000
#define	CCM_SD_CLK_DIV_RATIO_N_SHIFT	16
#define	CCM_SD_CLK_OPHASE_CTR		0x700
#define	CCM_SD_CLK_OPHASE_CTR_SHIFT	8
#define	CCM_SD_CLK_DIV_RATIO_M		0xf

#define	CCM_AUDIO_CODEC_ENABLE	(1U << 31)

#define	CCM_LCD_CH0_SCLK_GATING			(1U << 31)
#define	CCM_LCD_CH0_RESET			(1U << 30)
#define	CCM_LCD_CH0_SRC_SEL			0x03000000
#define	CCM_LCD_CH0_SRC_SEL_SHIFT		24
#define	CCM_LCD_CH0_SRC_SEL_PLL3		0
#define	CCM_LCD_CH0_SRC_SEL_PLL7		1
#define	CCM_LCD_CH0_SRC_SEL_PLL3_2X		2
#define	CCM_LCD_CH0_SRC_SEL_PLL6_2X		3

#define	CCM_LCD_CH1_SCLK2_GATING		(1U << 31)
#define	CCM_LCD_CH1_SRC_SEL			0x03000000
#define	CCM_LCD_CH1_SRC_SEL_SHIFT		24
#define	CCM_LCD_CH1_SRC_SEL_PLL3		0
#define	CCM_LCD_CH1_SRC_SEL_PLL7		1
#define	CCM_LCD_CH1_SRC_SEL_PLL3_2X		2
#define	CCM_LCD_CH1_SRC_SEL_PLL7_2X		3
#define	CCM_LCD_CH1_SCLK1_GATING		(1U << 15)
#define	CCM_LCD_CH1_SCLK1_SRC_SEL_SHIFT		11
#define	CCM_LCD_CH1_SCLK1_SRC_SEL_SCLK2		0
#define	CCM_LCD_CH1_SCLK1_SRC_SEL_SCLK2_DIV2	1
#define	CCM_LCD_CH1_CLK_DIV_RATIO_M		0xf

#define	CCM_DRAM_CLK_BE1_CLK_ENABLE	(1U << 27)
#define	CCM_DRAM_CLK_BE0_CLK_ENABLE	(1U << 26)

#define	CCM_BE_CLK_SCLK_GATING		(1U << 31)
#define	CCM_BE_CLK_RESET		(1U << 30)
#define	CCM_BE_CLK_SRC_SEL		0x03000000
#define	CCM_BE_CLK_SRC_SEL_SHIFT	24
#define	CCM_BE_CLK_SRC_SEL_PLL3		0
#define	CCM_BE_CLK_SRC_SEL_PLL7		1
#define	CCM_BE_CLK_SRC_SEL_PLL5		2
#define	CCM_BE_CLK_DIV_RATIO_M		0xf

#define	CCM_HDMI_CLK_SCLK_GATING	(1U << 31)
#define	CCM_HDMI_CLK_SRC_SEL		0x03000000
#define	CCM_HDMI_CLK_SRC_SEL_SHIFT	24
#define	CCM_HDMI_CLK_SRC_SEL_PLL3	0
#define	CCM_HDMI_CLK_SRC_SEL_PLL7	1
#define	CCM_HDMI_CLK_SRC_SEL_PLL3_2X	2
#define	CCM_HDMI_CLK_SRC_SEL_PLL7_2X	3
#define	CCM_HDMI_CLK_DIV_RATIO_M	0xf

#define	CCM_CLK_REF_FREQ	24000000U

int a10_clk_ehci_activate(void);
int a10_clk_ehci_deactivate(void);
int a10_clk_ohci_activate(void);
int a10_clk_ohci_deactivate(void);
int a10_clk_emac_activate(void);
int a10_clk_gmac_activate(phandle_t);
int a10_clk_ahci_activate(void);
int a10_clk_mmc_activate(int);
int a10_clk_mmc_cfg(int, int);
int a10_clk_i2c_activate(int);
int a10_clk_dmac_activate(void);
int a10_clk_codec_activate(unsigned int);
int a10_clk_debe_activate(void);
int a10_clk_lcd_activate(void);
int a10_clk_tcon_activate(unsigned int);
int a10_clk_tcon_get_config(int *, int *);
int a10_clk_hdmi_activate(void);

#endif /* _A10_CLK_H_ */
