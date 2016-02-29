/******************************************************************************

 � 1995-2003, 2004, 2005-2011 Freescale Semiconductor, Inc.
 All rights reserved.

 This is proprietary source code of Freescale Semiconductor Inc.,
 and its use is subject to the NetComm Device Drivers EULA.
 The copyright notice above does not evidence any actual or intended
 publication of such source code.

 ALTERNATIVELY, redistribution and use in source and binary forms, with
 or without modification, are permitted provided that the following
 conditions are met:
     * Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
     * Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.
     * Neither the name of Freescale Semiconductor nor the
       names of its contributors may be used to endorse or promote products
       derived from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY Freescale Semiconductor ``AS IS'' AND ANY
 EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL Freescale Semiconductor BE LIABLE FOR ANY
 DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 **************************************************************************/
/**

 @File          part_integration_ext.h

 @Description   P2041 external definitions and structures.
*//***************************************************************************/
#ifndef __PART_INTEGRATION_EXT_H
#define __PART_INTEGRATION_EXT_H

#include "std_ext.h"
#include "ddr_std_ext.h"
#include "enet_ext.h"
#include "dpaa_integration_ext.h"


/**************************************************************************//**
 @Group         P2041_chip_id P2041 Application Programming Interface

 @Description   P2041 Chip functions,definitions and enums.

 @{
*//***************************************************************************/

#define CORE_E500MC

#define INTG_MAX_NUM_OF_CORES   4


/**************************************************************************//**
 @Description   Module types.
*//***************************************************************************/
typedef enum e_ModuleId
{
    e_MODULE_ID_DUART_1 = 0,
    e_MODULE_ID_DUART_2,
    e_MODULE_ID_DUART_3,
    e_MODULE_ID_DUART_4,
    e_MODULE_ID_LAW,
    e_MODULE_ID_LBC,
    e_MODULE_ID_PAMU,
    e_MODULE_ID_QM,                 /**< Queue manager module */
    e_MODULE_ID_BM,                 /**< Buffer manager module */
    e_MODULE_ID_QM_CE_PORTAL_0,
    e_MODULE_ID_QM_CI_PORTAL_0,
    e_MODULE_ID_QM_CE_PORTAL_1,
    e_MODULE_ID_QM_CI_PORTAL_1,
    e_MODULE_ID_QM_CE_PORTAL_2,
    e_MODULE_ID_QM_CI_PORTAL_2,
    e_MODULE_ID_QM_CE_PORTAL_3,
    e_MODULE_ID_QM_CI_PORTAL_3,
    e_MODULE_ID_QM_CE_PORTAL_4,
    e_MODULE_ID_QM_CI_PORTAL_4,
    e_MODULE_ID_QM_CE_PORTAL_5,
    e_MODULE_ID_QM_CI_PORTAL_5,
    e_MODULE_ID_QM_CE_PORTAL_6,
    e_MODULE_ID_QM_CI_PORTAL_6,
    e_MODULE_ID_QM_CE_PORTAL_7,
    e_MODULE_ID_QM_CI_PORTAL_7,
    e_MODULE_ID_QM_CE_PORTAL_8,
    e_MODULE_ID_QM_CI_PORTAL_8,
    e_MODULE_ID_QM_CE_PORTAL_9,
    e_MODULE_ID_QM_CI_PORTAL_9,
    e_MODULE_ID_BM_CE_PORTAL_0,
    e_MODULE_ID_BM_CI_PORTAL_0,
    e_MODULE_ID_BM_CE_PORTAL_1,
    e_MODULE_ID_BM_CI_PORTAL_1,
    e_MODULE_ID_BM_CE_PORTAL_2,
    e_MODULE_ID_BM_CI_PORTAL_2,
    e_MODULE_ID_BM_CE_PORTAL_3,
    e_MODULE_ID_BM_CI_PORTAL_3,
    e_MODULE_ID_BM_CE_PORTAL_4,
    e_MODULE_ID_BM_CI_PORTAL_4,
    e_MODULE_ID_BM_CE_PORTAL_5,
    e_MODULE_ID_BM_CI_PORTAL_5,
    e_MODULE_ID_BM_CE_PORTAL_6,
    e_MODULE_ID_BM_CI_PORTAL_6,
    e_MODULE_ID_BM_CE_PORTAL_7,
    e_MODULE_ID_BM_CI_PORTAL_7,
    e_MODULE_ID_BM_CE_PORTAL_8,
    e_MODULE_ID_BM_CI_PORTAL_8,
    e_MODULE_ID_BM_CE_PORTAL_9,
    e_MODULE_ID_BM_CI_PORTAL_9,
    e_MODULE_ID_FM,                 /**< Frame manager module */
    e_MODULE_ID_FM_RTC,             /**< FM Real-Time-Clock */
    e_MODULE_ID_FM_MURAM,           /**< FM Multi-User-RAM */
    e_MODULE_ID_FM_BMI,             /**< FM BMI block */
    e_MODULE_ID_FM_QMI,             /**< FM QMI block */
    e_MODULE_ID_FM_PARSER,          /**< FM parser block */
    e_MODULE_ID_FM_PORT_HO1,        /**< FM Host-command/offline-parsing port block */
    e_MODULE_ID_FM_PORT_HO2,        /**< FM Host-command/offline-parsing port block */
    e_MODULE_ID_FM_PORT_HO3,        /**< FM Host-command/offline-parsing port block */
    e_MODULE_ID_FM_PORT_HO4,        /**< FM Host-command/offline-parsing port block */
    e_MODULE_ID_FM_PORT_HO5,        /**< FM Host-command/offline-parsing port block */
    e_MODULE_ID_FM_PORT_HO6,        /**< FM Host-command/offline-parsing port block */
    e_MODULE_ID_FM_PORT_HO7,        /**< FM Host-command/offline-parsing port block */
    e_MODULE_ID_FM_PORT_1GRx1,      /**< FM Rx 1G MAC port block */
    e_MODULE_ID_FM_PORT_1GRx2,      /**< FM Rx 1G MAC port block */
    e_MODULE_ID_FM_PORT_1GRx3,      /**< FM Rx 1G MAC port block */
    e_MODULE_ID_FM_PORT_1GRx4,      /**< FM Rx 1G MAC port block */
    e_MODULE_ID_FM_PORT_1GRx5,      /**< FM Rx 1G MAC port block */
    e_MODULE_ID_FM_PORT_10GRx,      /**< FM Rx 10G MAC port block */
    e_MODULE_ID_FM_PORT_1GTx1,      /**< FM Tx 1G MAC port block */
    e_MODULE_ID_FM_PORT_1GTx2,      /**< FM Tx 1G MAC port block */
    e_MODULE_ID_FM_PORT_1GTx3,      /**< FM Tx 1G MAC port block */
    e_MODULE_ID_FM_PORT_1GTx4,      /**< FM Tx 1G MAC port block */
    e_MODULE_ID_FM_PORT_1GTx5,      /**< FM Tx 1G MAC port block */
    e_MODULE_ID_FM_PORT_10GTx,      /**< FM Tx 10G MAC port block */
    e_MODULE_ID_FM_PLCR,            /**< FM Policer */
    e_MODULE_ID_FM_KG,              /**< FM Keygen */
    e_MODULE_ID_FM_DMA,             /**< FM DMA */
    e_MODULE_ID_FM_FPM,             /**< FM FPM */
    e_MODULE_ID_FM_IRAM,            /**< FM Instruction-RAM */
    e_MODULE_ID_FM_1GMDIO1,         /**< FM 1G MDIO MAC 1*/
    e_MODULE_ID_FM_1GMDIO2,         /**< FM 1G MDIO MAC 2*/
    e_MODULE_ID_FM_1GMDIO3,         /**< FM 1G MDIO MAC 3*/
    e_MODULE_ID_FM_1GMDIO4,         /**< FM 1G MDIO MAC 4*/
    e_MODULE_ID_FM_1GMDIO5,         /**< FM 1G MDIO MAC 5*/
    e_MODULE_ID_FM_10GMDIO,         /**< FM 10G MDIO */
    e_MODULE_ID_FM_PRS_IRAM,        /**< FM SW-parser Instruction-RAM */
    e_MODULE_ID_FM_1GMAC1,          /**< FM 1G MAC #1 */
    e_MODULE_ID_FM_1GMAC2,          /**< FM 1G MAC #2 */
    e_MODULE_ID_FM_1GMAC3,          /**< FM 1G MAC #3 */
    e_MODULE_ID_FM_1GMAC4,          /**< FM 1G MAC #4 */
    e_MODULE_ID_FM_1GMAC5,          /**< FM 1G MAC #5 */
    e_MODULE_ID_FM_10GMAC,          /**< FM 10G MAC */

    e_MODULE_ID_SEC_GEN,            /**< SEC 4.0 General registers      */
    e_MODULE_ID_SEC_QI,             /**< SEC 4.0 QI registers           */
    e_MODULE_ID_SEC_JQ0,            /**< SEC 4.0 JQ-0 registers         */
    e_MODULE_ID_SEC_JQ1,            /**< SEC 4.0 JQ-1 registers         */
    e_MODULE_ID_SEC_JQ2,            /**< SEC 4.0 JQ-2 registers         */
    e_MODULE_ID_SEC_JQ3,            /**< SEC 4.0 JQ-3 registers         */
    e_MODULE_ID_SEC_RTIC,           /**< SEC 4.0 RTIC registers         */
    e_MODULE_ID_SEC_DECO0_CCB0,     /**< SEC 4.0 DECO-0/CCB-0 registers */
    e_MODULE_ID_SEC_DECO1_CCB1,     /**< SEC 4.0 DECO-1/CCB-1 registers */
    e_MODULE_ID_SEC_DECO2_CCB2,     /**< SEC 4.0 DECO-2/CCB-2 registers */
    e_MODULE_ID_SEC_DECO3_CCB3,     /**< SEC 4.0 DECO-3/CCB-3 registers */
    e_MODULE_ID_SEC_DECO4_CCB4,     /**< SEC 4.0 DECO-4/CCB-4 registers */

    e_MODULE_ID_PIC,                /**< PIC */
    e_MODULE_ID_GPIO,               /**< GPIO */
    e_MODULE_ID_SERDES,             /**< SERDES */
    e_MODULE_ID_CPC,                /**< CoreNet-Platform-Cache */
    e_MODULE_ID_DUMMY_LAST
} e_ModuleId;

#define NUM_OF_MODULES  e_MODULE_ID_DUMMY_LAST

/* Offsets relative to CCSR base */
#define P2041_OFFSET_LAW              0x00000c00
#define P2041_OFFSET_DDR              0x00008000
#define P2041_OFFSET_CPC              0x00010000
#define P2041_OFFSET_CCM              0x00018000
#define P2041_OFFSET_PAMU             0x00020000
#define P2041_OFFSET_PIC              0x00040000
#define P2041_OFFSET_GUTIL            0x000e0000
#define P2041_OFFSET_RCPM             0x000e2000
#define P2041_OFFSET_SERDES           0x000ea000
#define P2041_OFFSET_DMA1             0x00100100
#define P2041_OFFSET_DMA2             0x00101100
#define P2041_OFFSET_ESPI             0x00110000
#define P2041_OFFSET_ESDHC            0x00114000
#define P2041_OFFSET_I2C1             0x00118000
#define P2041_OFFSET_I2C2             0x00118100
#define P2041_OFFSET_I2C3             0x00119000
#define P2041_OFFSET_I2C4             0x00119100
#define P2041_OFFSET_DUART1           0x0011c500
#define P2041_OFFSET_DUART2           0x0011c600
#define P2041_OFFSET_DUART3           0x0011d500
#define P2041_OFFSET_DUART4           0x0011d600
#define P2041_OFFSET_LBC              0x00124000
#define P2041_OFFSET_GPIO             0x00130000
#define P2041_OFFSET_PCIE1            0x00200000
#define P2041_OFFSET_PCIE2            0x00201000
#define P2041_OFFSET_PCIE3            0x00202000
#define P2041_OFFSET_USB1             0x00210000
#define P2041_OFFSET_USB2             0x00211000
#define P2041_OFFSET_USB_PHY          0x00214000
#define P2041_OFFSET_SATA1            0x00220000
#define P2041_OFFSET_SATA2            0x00221000
#define P2041_OFFSET_SEC_GEN          0x00300000
#define P2041_OFFSET_SEC_JQ0          0x00301000
#define P2041_OFFSET_SEC_JQ1          0x00302000
#define P2041_OFFSET_SEC_JQ2          0x00303000
#define P2041_OFFSET_SEC_JQ3          0x00304000
#define P2041_OFFSET_SEC_RESERVED     0x00305000
#define P2041_OFFSET_SEC_RTIC         0x00306000
#define P2041_OFFSET_SEC_QI           0x00307000
#define P2041_OFFSET_SEC_DECO0_CCB0   0x00308000
#define P2041_OFFSET_SEC_DECO1_CCB1   0x00309000
#define P2041_OFFSET_PME              0x00316000
#define P2041_OFFSET_QM               0x00318000
#define P2041_OFFSET_BM               0x0031a000
#define P2041_OFFSET_FM               0x00400000

#define P2041_OFFSET_FM_MURAM         P2041_OFFSET_FM
#define P2041_OFFSET_FM_BMI           (P2041_OFFSET_FM + 0x00080000)
#define P2041_OFFSET_FM_QMI           (P2041_OFFSET_FM + 0x00080400)
#define P2041_OFFSET_FM_PARSER        (P2041_OFFSET_FM + 0x00080800)
#define P2041_OFFSET_FM_PORT_HO1      (P2041_OFFSET_FM + 0x00081000)     /* host command/offline parser */
#define P2041_OFFSET_FM_PORT_HO2      (P2041_OFFSET_FM + 0x00082000)
#define P2041_OFFSET_FM_PORT_HO3      (P2041_OFFSET_FM + 0x00083000)
#define P2041_OFFSET_FM_PORT_HO4      (P2041_OFFSET_FM + 0x00084000)
#define P2041_OFFSET_FM_PORT_HO5      (P2041_OFFSET_FM + 0x00085000)
#define P2041_OFFSET_FM_PORT_HO6      (P2041_OFFSET_FM + 0x00086000)
#define P2041_OFFSET_FM_PORT_HO7      (P2041_OFFSET_FM + 0x00087000)
#define P2041_OFFSET_FM_PORT_1GRX1    (P2041_OFFSET_FM + 0x00088000)
#define P2041_OFFSET_FM_PORT_1GRX2    (P2041_OFFSET_FM + 0x00089000)
#define P2041_OFFSET_FM_PORT_1GRX3    (P2041_OFFSET_FM + 0x0008a000)
#define P2041_OFFSET_FM_PORT_1GRX4    (P2041_OFFSET_FM + 0x0008b000)
#define P2041_OFFSET_FM_PORT_1GRX5    (P2041_OFFSET_FM + 0x0008c000)
#define P2041_OFFSET_FM_PORT_10GRX    (P2041_OFFSET_FM + 0x00090000)
#define P2041_OFFSET_FM_PORT_1GTX1    (P2041_OFFSET_FM + 0x000a8000)
#define P2041_OFFSET_FM_PORT_1GTX2    (P2041_OFFSET_FM + 0x000a9000)
#define P2041_OFFSET_FM_PORT_1GTX3    (P2041_OFFSET_FM + 0x000aa000)
#define P2041_OFFSET_FM_PORT_1GTX4    (P2041_OFFSET_FM + 0x000ab000)
#define P2041_OFFSET_FM_PORT_1GTX5    (P2041_OFFSET_FM + 0x000ac000)
#define P2041_OFFSET_FM_PORT_10GTX    (P2041_OFFSET_FM + 0x000b0000)
#define P2041_OFFSET_FM_PLCR          (P2041_OFFSET_FM + 0x000c0000)
#define P2041_OFFSET_FM_KG            (P2041_OFFSET_FM + 0x000c1000)
#define P2041_OFFSET_FM_DMA           (P2041_OFFSET_FM + 0x000c2000)
#define P2041_OFFSET_FM_FPM           (P2041_OFFSET_FM + 0x000c3000)
#define P2041_OFFSET_FM_IRAM          (P2041_OFFSET_FM + 0x000c4000)
#define P2041_OFFSET_FM_PARSER_IRAM   (P2041_OFFSET_FM + 0x000c7000)
#define P2041_OFFSET_FM_1GMAC1        (P2041_OFFSET_FM + 0x000e0000)
#define P2041_OFFSET_FM_1GMDIO        (P2041_OFFSET_FM + 0x000e1000 + 0x120)
#define P2041_OFFSET_FM_1GMAC2        (P2041_OFFSET_FM + 0x000e2000)
#define P2041_OFFSET_FM_1GMAC3        (P2041_OFFSET_FM + 0x000e4000)
#define P2041_OFFSET_FM_1GMAC4        (P2041_OFFSET_FM + 0x000e6000)
#define P2041_OFFSET_FM_1GMAC5        (P2041_OFFSET_FM + 0x000e8000)
#define P2041_OFFSET_FM_10GMAC        (P2041_OFFSET_FM + 0x000f0000)
#define P2041_OFFSET_FM_10GMDIO       (P2041_OFFSET_FM + 0x000f1000 + 0x030)
#define P2041_OFFSET_FM_RTC           (P2041_OFFSET_FM + 0x000fe000)

/* Offsets relative to QM or BM portals base */
#define P2041_OFFSET_PORTALS_CE_AREA  0x000000        /* cache enabled area */
#define P2041_OFFSET_PORTALS_CI_AREA  0x100000        /* cache inhibited area */

#define P2041_CE_PORTAL_SIZE               0x4000
#define P2041_CI_PORTAL_SIZE               0x1000

#define P2041_OFFSET_PORTALS_CE(portal) \
    (P2041_OFFSET_PORTALS_CE_AREA + P2041_CE_PORTAL_SIZE * (portal))
#define P2041_OFFSET_PORTALS_CI(portal) \
    (P2041_OFFSET_PORTALS_CI_AREA + P2041_CI_PORTAL_SIZE * (portal))


/**************************************************************************//**
 @Description   Transaction source ID (for memory conrollers error reporting).
*//***************************************************************************/
typedef enum e_TransSrc
{
    e_TRANS_SRC_PCIE_1          = 0x0,  /**< PCI Express 1                  */
    e_TRANS_SRC_PCIE_2          = 0x1,  /**< PCI Express 2                  */
    e_TRANS_SRC_PCIE_3          = 0x2,  /**< PCI Express 3                  */
    e_TRANS_SRC_SRIO_1          = 0x8,  /**< SRIO 1                         */
    e_TRANS_SRC_SRIO_2          = 0x9,  /**< SRIO 2                         */
    e_TRANS_SRC_BMAN            = 0x18, /**< BMan                           */
    e_TRANS_SRC_PAMU            = 0x1C, /**< PAMU                           */
    e_TRANS_SRC_PME             = 0x20, /**< PME                            */
    e_TRANS_SRC_SEC             = 0x21, /**< Security engine                */
    e_TRANS_SRC_QMAN            = 0x3C, /**< QMan                           */
    e_TRANS_SRC_USB_1           = 0x40, /**< USB 1                          */
    e_TRANS_SRC_USB_2           = 0x41, /**< USB 2                          */
    e_TRANS_SRC_ESDHC           = 0x44, /**< eSDHC                          */
    e_TRANS_SRC_PBL             = 0x48, /**< Pre-boot loader                */
    e_TRANS_SRC_NPC             = 0x4B, /**< Nexus port controller          */
    e_TRANS_SRC_RMAN            = 0x5D, /**< RIO message manager            */
    e_TRANS_SRC_SATA_1          = 0x60, /**< SATA 1                         */
    e_TRANS_SRC_SATA_2          = 0x61, /**< SATA 2                         */
    e_TRANS_SRC_DMA_1           = 0x70, /**< DMA 1                          */
    e_TRANS_SRC_DMA_2           = 0x71, /**< DMA 2                          */
    e_TRANS_SRC_CORE_0_INST     = 0x80, /**< Processor 0 (instruction)      */
    e_TRANS_SRC_CORE_0_DATA     = 0x81, /**< Processor 0 (data)             */
    e_TRANS_SRC_CORE_1_INST     = 0x82, /**< Processor 1 (instruction)      */
    e_TRANS_SRC_CORE_1_DATA     = 0x83, /**< Processor 1 (data)             */
    e_TRANS_SRC_CORE_2_INST     = 0x84, /**< Processor 2 (instruction)      */
    e_TRANS_SRC_CORE_2_DATA     = 0x85, /**< Processor 2 (data)             */
    e_TRANS_SRC_CORE_3_INST     = 0x86, /**< Processor 3 (instruction)      */
    e_TRANS_SRC_CORE_3_DATA     = 0x87, /**< Processor 3 (data)             */
    e_TRANS_SRC_FM_10G          = 0xC0, /**< FM XAUI                        */
    e_TRANS_SRC_FM_HO_1         = 0xC1, /**< FM offline, host 1             */
    e_TRANS_SRC_FM_HO_2         = 0xC2, /**< FM offline, host 2             */
    e_TRANS_SRC_FM_HO_3         = 0xC3, /**< FM offline, host 3             */
    e_TRANS_SRC_FM_HO_4         = 0xC4, /**< FM offline, host 4             */
    e_TRANS_SRC_FM_HO_5         = 0xC5, /**< FM offline, host 5             */
    e_TRANS_SRC_FM_HO_6         = 0xC6, /**< FM offline, host 6             */
    e_TRANS_SRC_FM_HO_7         = 0xC7, /**< FM offline, host 7             */
    e_TRANS_SRC_FM_GETH_1       = 0xC8, /**< FM GETH 1                      */
    e_TRANS_SRC_FM_GETH_2       = 0xC9, /**< FM GETH 2                      */
    e_TRANS_SRC_FM_GETH_3       = 0xCA, /**< FM GETH 3                      */
    e_TRANS_SRC_FM_GETH_4       = 0xCB, /**< FM GETH 4                      */
    e_TRANS_SRC_FM_GETH_5       = 0xCC  /**< FM GETH 5                      */
} e_TransSrc;

/**************************************************************************//**
 @Description   Local Access Window Target interface ID
*//***************************************************************************/
typedef enum e_P2041LawTargetId
{
    e_P2041_LAW_TARGET_PCIE_1          = 0x0,   /**< PCI Express 1 */
    e_P2041_LAW_TARGET_PCIE_2          = 0x1,   /**< PCI Express 2 */
    e_P2041_LAW_TARGET_PCIE_3          = 0x2,   /**< PCI Express 3 */
    e_P2041_LAW_TARGET_SRIO_1          = 0x8,   /**< SRIO 1 */
    e_P2041_LAW_TARGET_SRIO_2          = 0x9,   /**< SRIO 2 */
    e_P2041_LAW_TARGET_DDR_CPC         = 0x10,  /**< DDR controller or CPC SRAM */
    e_P2041_LAW_TARGET_BMAN            = 0x18,  /**< BMAN target interface ID */
    e_P2041_LAW_TARGET_DCSR            = 0x1D,  /**< DCSR */
    e_P2041_LAW_TARGET_LBC             = 0x1F,  /**< Local Bus target interface ID */
    e_P2041_LAW_TARGET_QMAN            = 0x3C,  /**< QMAN target interface ID */
    e_P2041_LAW_TARGET_NONE            = 0xFF   /**< None */
} e_P2041LawTargetId;

/***************************************************************
    P2041 general routines
****************************************************************/
/**************************************************************************//**
 @Group         P2041_init_grp P2041 Initialization Unit

 @Description   P2041 initialization unit API functions, definitions and enums

 @{
*//***************************************************************************/

/**************************************************************************//**
 @Description   Part ID and revision number
*//***************************************************************************/
typedef enum e_P2041DeviceName
{
    e_P2041_REV_INVALID     = 0x00000000,       /**< Invalid revision                     */
    e_P2040_REV_1_0         = (int)0x82180010,  /**< P2040 with security,    revision 1.0 */
    e_P2040_REV_1_0_NO_SEC  = (int)0x82100010,  /**< P2040 without security, revision 1.0 */
    e_P2041_REV_1_0         = (int)0x82180110,  /**< P2041 with security,    revision 1.0 */
    e_P2041_REV_1_0_NO_SEC  = (int)0x82100110   /**< P2041 without security, revision 1.0 */
} e_P2041DeviceName;

/**************************************************************************//**
 @Description   Device Disable Register
*//***************************************************************************/
typedef enum e_P2041DeviceDisable
{
    e_P2041_DEV_DISABLE_PCIE_1  = 0,    /**< PCI Express controller 1 disable */
    e_P2041_DEV_DISABLE_PCIE_2,         /**< PCI Express controller 2 disable */
    e_P2041_DEV_DISABLE_PCIE_3,         /**< PCI Express controller 3 disable */
    e_P2041_DEV_DISABLE_RMAN    = 4,    /**< RapidIO message manager disable */
    e_P2041_DEV_DISABLE_SRIO_1,         /**< Serial RapidIO controller 1 disable */
    e_P2041_DEV_DISABLE_SRIO_2,         /**< Serial RapidIO controller 2 disable */
    e_P2041_DEV_DISABLE_DMA_1   = 9,    /**< DMA controller 1 disable */
    e_P2041_DEV_DISABLE_DMA_2,          /**< DMA controller 2 disable */
    e_P2041_DEV_DISABLE_DDR,            /**< DDR controller disable */
    e_P2041_DEV_DISABLE_SATA_1  = 17,   /**< SATA controller 1 disable */
    e_P2041_DEV_DISABLE_SATA_2,         /**< SATA controller 2 disable */
    e_P2041_DEV_DISABLE_LBC,            /**< eLBC controller disable */
    e_P2041_DEV_DISABLE_USB_1,          /**< USB controller 1 disable */
    e_P2041_DEV_DISABLE_USB_2,          /**< USB controller 2 disable */
    e_P2041_DEV_DISABLE_ESDHC   = 23,   /**< eSDHC controller disable */
    e_P2041_DEV_DISABLE_GPIO,           /**< GPIO controller disable */
    e_P2041_DEV_DISABLE_ESPI,           /**< eSPI controller disable */
    e_P2041_DEV_DISABLE_I2C_1,          /**< I2C module 1 (controllers 1 and 2) disable */
    e_P2041_DEV_DISABLE_I2C_2,          /**< I2C module 2 (controllers 3 and 4) disable */
    e_P2041_DEV_DISABLE_DUART_1 = 30,   /**< DUART controller 1 disable */
    e_P2041_DEV_DISABLE_DUART_2,        /**< DUART controller 2 disable */
    e_P2041_DEV_DISABLE_DISR1_DUMMY_LAST = 32,
                                        /**< Dummy entry signing end of DEVDISR1 register controllers */
    e_P2041_DEV_DISABLE_PME     = e_P2041_DEV_DISABLE_DISR1_DUMMY_LAST,
                                        /**< Pattern match engine disable */
    e_P2041_DEV_DISABLE_SEC,            /**< Security disable */
    e_P2041_DEV_DISABLE_QM_BM   = e_P2041_DEV_DISABLE_DISR1_DUMMY_LAST + 4,
                                        /**< Queue manager/buffer manager disable */
    e_P2041_DEV_DISABLE_FM      = e_P2041_DEV_DISABLE_DISR1_DUMMY_LAST + 6,
                                        /**< Frame manager disable */
    e_P2041_DEV_DISABLE_10G,            /**< 10G Ethernet controller disable */
    e_P2041_DEV_DISABLE_DTSEC_1,
                                        /**< dTSEC controller 1 disable */
    e_P2041_DEV_DISABLE_DTSEC_2,        /**< dTSEC controller 2 disable */
    e_P2041_DEV_DISABLE_DTSEC_3,        /**< dTSEC controller 3 disable */
    e_P2041_DEV_DISABLE_DTSEC_4,        /**< dTSEC controller 4 disable */
    e_P2041_DEV_DISABLE_DTSEC_5         /**< dTSEC controller 5 disable */
} e_P2041DeviceDisable;


/**************************************************************************//*
 @Description   structure representing P2041 devices configuration
*//***************************************************************************/
typedef struct t_P2041Devices
{
    struct
    {
        struct
        {
            bool                    enabled;
            uint8_t                 serdesBank;
            uint16_t                serdesLane;     /**< Most significant bits represent lanes used by this bank,
                                                         one bit for lane, lane A is the first and so on, e.g.,
                                                         set 0xF000 for ABCD lanes */
            e_EnetInterface         ethIf;
            uint8_t                 ratio;
            bool                    divByTwo;
            bool                    isTwoHalfSgmii;
        } dtsecs[FM_MAX_NUM_OF_1G_MACS];
        struct
        {
            bool                    enabled;
            uint8_t                 serdesBank;
            uint16_t                serdesLane;
        } tgec;
    } fm;
} t_P2041Devices;

/**************************************************************************//**
 @Function      P2041_GetRevInfo

 @Description   Obtain revision information.

 @Param[in]     gutilBase       - Gutil memory map virtual base address.

 @Return        Part ID and revision.
*//***************************************************************************/
e_P2041DeviceName P2041_GetRevInfo(uintptr_t gutilBase);

/**************************************************************************//**
 @Function      P2041_GetE500Factor

 @Description   Obtain core's multiplication factors.

 @Param[in]     gutilBase       - Gutil memory map virtual base address.
 @Param[in]     coreIndex       - Core index.
 @Param[out]    p_E500MulFactor - E500 to CCB multification factor.
 @Param[out]    p_E500DivFactor - E500 to CCB division factor.

*//***************************************************************************/
void P2041_GetE500Factor(uintptr_t  gutilBase,
                         uint8_t    coreIndex,
                         uint32_t   *p_E500MulFactor,
                         uint32_t   *p_E500DivFactor);

/**************************************************************************//**
 @Function      P2041_GetCcbFactor

 @Description   Obtain system multiplication factor.

 @Param[in]     gutilBase       - Gutil memory map virtual base address.

 @Return        System multiplication factor.
*//***************************************************************************/
uint32_t P2041_GetCcbFactor(uintptr_t gutilBase);

/**************************************************************************//**
 @Function      P2041_GetDdrFactor

 @Description   Obtain DDR clock multiplication factor.

 @Param[in]     gutilBase       - Gutil memory map virtual base address.

 @Return        DDR clock multiplication factor.
*//***************************************************************************/
uint32_t P2041_GetDdrFactor(uintptr_t gutilBase);

/**************************************************************************//**
 @Function      P2041_GetDdrType

 @Description   Obtain DDR memory type.

 @Param[in]     gutilBase       - Gutil memory map virtual base address.

 @Return        DDR type.
*//***************************************************************************/
e_DdrType  P2041_GetDdrType(uintptr_t gutilBase);

/**************************************************************************//**
 @Function      P2041_GetFmFactor

 @Description   returns FM multiplication factors. (This value is returned using
                two parameters to avoid using float parameter).

 @Param[in]     gutilBase       - Gutil memory map virtual base address.
 @Param[out]    p_FmMulFactor   - FM to CCB multification factor.
 @Param[out]    p_FmDivFactor   - FM to CCB division factor.

*//***************************************************************************/
void  P2041_GetFmFactor(uintptr_t gutilBase,
                        uint32_t  *p_FmMulFactor,
                        uint32_t  *p_FmDivFactor);


void P2041_CoreTimeBaseEnable(uintptr_t rcpmBase);
void P2041_CoreTimeBaseDisable(uintptr_t rcpmBase);

typedef enum e_SerdesProtocol
{
    SRDS_PROTOCOL_NONE = 0,
    SRDS_PROTOCOL_PCIE1,
    SRDS_PROTOCOL_PCIE2,
    SRDS_PROTOCOL_PCIE3,
    SRDS_PROTOCOL_SRIO1,
    SRDS_PROTOCOL_SRIO2,
    SRDS_PROTOCOL_SGMII_FM,
    SRDS_PROTOCOL_XAUI_FM,
    SRDS_PROTOCOL_SATA1,
    SRDS_PROTOCOL_SATA2,
    SRDS_PROTOCOL_AURORA
} e_SerdesProtocol;

t_Error  P2041_DeviceDisable(uintptr_t gutilBase, e_P2041DeviceDisable device, bool disable);
void     P2041_GetDevicesConfiguration(uintptr_t gutilBase, t_P2041Devices *p_Devices);
t_Error  P2041_PamuDisableBypass(uintptr_t gutilBase, uint8_t pamuId, bool disable);
void     P2041_SetDmaLiodn(uintptr_t gutilBase, uint8_t dmaId, uint16_t liodn);
uint32_t P2041_SerdesRcwGetProtocol(uintptr_t gutilBase);
bool     P2041_SerdesRcwIsDeviceConfigured(uintptr_t gutilBase, e_SerdesProtocol device);
bool     P2041_SerdesRcwIsLaneEnabled(uintptr_t gutilBase, uint32_t lane);

/** @} */ /* end of P2041_init_grp group */
/** @} */ /* end of P2041_grp group */


/*****************************************************************************
 INTEGRATION-SPECIFIC MODULE CODES
******************************************************************************/
#define MODULE_UNKNOWN          0x00000000
#define MODULE_MEM              0x00010000
#define MODULE_MM               0x00020000
#define MODULE_CORE             0x00030000
#define MODULE_P2041            0x00040000
#define MODULE_P2041_PLATFORM   0x00050000
#define MODULE_PM               0x00060000
#define MODULE_MMU              0x00070000
#define MODULE_PIC              0x00080000
#define MODULE_CPC              0x00090000
#define MODULE_DUART            0x000a0000
#define MODULE_SERDES           0x000b0000
#define MODULE_PIO              0x000c0000
#define MODULE_QM               0x000d0000
#define MODULE_BM               0x000e0000
#define MODULE_SEC              0x000f0000
#define MODULE_LAW              0x00100000
#define MODULE_LBC              0x00110000
#define MODULE_PAMU             0x00120000
#define MODULE_FM               0x00130000
#define MODULE_FM_MURAM         0x00140000
#define MODULE_FM_PCD           0x00150000
#define MODULE_FM_RTC           0x00160000
#define MODULE_FM_MAC           0x00170000
#define MODULE_FM_PORT          0x00180000
#define MODULE_DPA_PORT         0x00190000
#define MODULE_MII              0x001a0000
#define MODULE_I2C              0x001b0000
#define MODULE_DMA              0x001c0000
#define MODULE_DDR              0x001d0000
#define MODULE_ESPI             0x001e0000

/*****************************************************************************
 PAMU INTEGRATION-SPECIFIC DEFINITIONS
******************************************************************************/
#define PAMU_NUM_OF_PARTITIONS  4


/*****************************************************************************
 LAW INTEGRATION-SPECIFIC DEFINITIONS
******************************************************************************/
#define LAW_NUM_OF_WINDOWS      32
#define LAW_MIN_WINDOW_SIZE     0x0000000000001000LL    /**< 4KB */
#define LAW_MAX_WINDOW_SIZE     0x0000002000000000LL    /**< 64GB */


/*****************************************************************************
 LBC INTEGRATION-SPECIFIC DEFINITIONS
******************************************************************************/
/**************************************************************************//**
 @Group         lbc_exception_grp LBC Exception Unit

 @Description   LBC Exception unit API functions, definitions and enums

 @{
*//***************************************************************************/

/**************************************************************************//**
 @Anchor        lbc_exbm

 @Collection    LBC Errors Bit Mask

                These errors are reported through the exceptions callback..
                The values can be or'ed in any combination in the errors mask
                parameter of the errors report structure.

                These errors can also be passed as a bit-mask to
                LBC_EnableErrorChecking() or LBC_DisableErrorChecking(),
                for enabling or disabling error checking.
 @{
*//***************************************************************************/
#define LBC_ERR_BUS_MONITOR     0x80000000  /**< Bus monitor error */
#define LBC_ERR_PARITY_ECC      0x20000000  /**< Parity error for GPCM/UPM */
#define LBC_ERR_WRITE_PROTECT   0x04000000  /**< Write protection error */
#define LBC_ERR_CHIP_SELECT     0x00080000  /**< Unrecognized chip select */

#define LBC_ERR_ALL             (LBC_ERR_BUS_MONITOR | LBC_ERR_PARITY_ECC | \
                                 LBC_ERR_WRITE_PROTECT | LBC_ERR_CHIP_SELECT)
                                            /**< All possible errors */
/* @} */
/** @} */ /* end of lbc_exception_grp group */

#define LBC_INCORRECT_ERROR_REPORT_ERRATA

#define LBC_NUM_OF_BANKS            4
#define LBC_MAX_CS_SIZE             0x0000000100000000LL  /* Up to 4G memory block size */
#define LBC_PARITY_SUPPORT
#define LBC_ADDRESS_HOLD_TIME_CTRL
#define LBC_HIGH_CLK_DIVIDERS
#define LBC_FCM_AVAILABLE

/*****************************************************************************
 GPIO INTEGRATION-SPECIFIC DEFINITIONS
******************************************************************************/
#define GPIO_NUM_OF_PORTS   1   /**< Number of ports in GPIO module;
                                     Each port contains up to 32 I/O pins. */

#define GPIO_VALID_PIN_MASKS  \
    { /* Port A */ 0xFFFFFFFF }

#define GPIO_VALID_INTR_MASKS \
    { /* Port A */ 0xFFFFFFFF }


/*****************************************************************************
 SERDES INTEGRATION-SPECIFIC DEFINITIONS
******************************************************************************/
#define SRDS_MAX_LANES      10  /* Lanes C - H on bank 1, lanes A - D on bank 2  */
#define SRDS_MAX_BANK       2

/* Serdes lanes general information provided in the following form:
   1) Lane index in Serdes Control Registers Map
   2) Lane enable/disable bit number in RCW
   3) Lane bank index */
#define SRDS_LANES  \
{                   \
    { 2,  154, 0 }, \
    { 3,  155, 0 }, \
    { 4,  156, 0 }, \
    { 5,  157, 0 }, \
    { 6,  158, 0 }, \
    { 7,  159, 0 }, \
    { 16, 162, 1 }, \
    { 17, 163, 1 }, \
    { 18, 164, 1 }, \
    { 19, 165, 1 }  \
}

#define SRDS_PROTOCOL_OPTIONS \
/* Protocol  Lane assignment */ \
{ \
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},  \
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},  \
/* 0x02 */  {SRDS_PROTOCOL_PCIE1, SRDS_PROTOCOL_PCIE1, \
             SRDS_PROTOCOL_SRIO1, SRDS_PROTOCOL_SRIO1, SRDS_PROTOCOL_SRIO1, SRDS_PROTOCOL_SRIO1, \
             SRDS_PROTOCOL_SGMII_FM, SRDS_PROTOCOL_SGMII_FM, SRDS_PROTOCOL_SGMII_FM, SRDS_PROTOCOL_SGMII_FM}, \
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},  \
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},  \
/* 0x05 */  {SRDS_PROTOCOL_PCIE1, SRDS_PROTOCOL_PCIE3, \
             SRDS_PROTOCOL_PCIE2, SRDS_PROTOCOL_PCIE2, SRDS_PROTOCOL_PCIE2, SRDS_PROTOCOL_PCIE2, \
             SRDS_PROTOCOL_SGMII_FM, SRDS_PROTOCOL_SGMII_FM, SRDS_PROTOCOL_SGMII_FM, SRDS_PROTOCOL_SGMII_FM}, \
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},  \
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},  \
/* 0x08 */  {SRDS_PROTOCOL_SGMII_FM, SRDS_PROTOCOL_SGMII_FM, \
             SRDS_PROTOCOL_PCIE2, SRDS_PROTOCOL_PCIE2, SRDS_PROTOCOL_PCIE2, SRDS_PROTOCOL_PCIE2, \
             0, 0, SRDS_PROTOCOL_SATA1, SRDS_PROTOCOL_SATA2}, \
/* 0x09 */  {SRDS_PROTOCOL_SGMII_FM, SRDS_PROTOCOL_SGMII_FM, \
             SRDS_PROTOCOL_PCIE2, SRDS_PROTOCOL_PCIE2, SRDS_PROTOCOL_PCIE2, SRDS_PROTOCOL_PCIE2, \
             SRDS_PROTOCOL_XAUI_FM, SRDS_PROTOCOL_XAUI_FM, SRDS_PROTOCOL_XAUI_FM, SRDS_PROTOCOL_XAUI_FM}, \
/* 0x0A */  {SRDS_PROTOCOL_SGMII_FM, SRDS_PROTOCOL_SGMII_FM, \
             SRDS_PROTOCOL_PCIE2, SRDS_PROTOCOL_PCIE2, SRDS_PROTOCOL_PCIE2, SRDS_PROTOCOL_PCIE2, \
             SRDS_PROTOCOL_PCIE3, SRDS_PROTOCOL_PCIE3, SRDS_PROTOCOL_PCIE3, SRDS_PROTOCOL_PCIE3}, \
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},  \
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},  \
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},  \
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},  \
/* 0x0F */  {SRDS_PROTOCOL_SGMII_FM, SRDS_PROTOCOL_SGMII_FM, \
             SRDS_PROTOCOL_SRIO2, SRDS_PROTOCOL_SRIO2, SRDS_PROTOCOL_SRIO1, SRDS_PROTOCOL_SRIO1, \
             SRDS_PROTOCOL_PCIE3, SRDS_PROTOCOL_SGMII_FM, SRDS_PROTOCOL_SGMII_FM, SRDS_PROTOCOL_SGMII_FM}, \
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},  \
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},  \
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},  \
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},  \
/* 0x14 */  {SRDS_PROTOCOL_SGMII_FM, SRDS_PROTOCOL_SGMII_FM, \
             SRDS_PROTOCOL_PCIE2, SRDS_PROTOCOL_PCIE2, SRDS_PROTOCOL_SRIO1, SRDS_PROTOCOL_SRIO1, \
             SRDS_PROTOCOL_AURORA, SRDS_PROTOCOL_SGMII_FM, SRDS_PROTOCOL_SGMII_FM, SRDS_PROTOCOL_SGMII_FM}, \
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},  \
/* 0x16 */  {SRDS_PROTOCOL_PCIE1, SRDS_PROTOCOL_PCIE3, \
             SRDS_PROTOCOL_PCIE2, SRDS_PROTOCOL_PCIE2, SRDS_PROTOCOL_SGMII_FM, SRDS_PROTOCOL_SGMII_FM, \
             0, 0, SRDS_PROTOCOL_SATA1, SRDS_PROTOCOL_SATA2}, \
/* 0x17 */  {SRDS_PROTOCOL_PCIE1, SRDS_PROTOCOL_PCIE3, \
             SRDS_PROTOCOL_PCIE2, SRDS_PROTOCOL_PCIE2, SRDS_PROTOCOL_SGMII_FM, SRDS_PROTOCOL_SGMII_FM, \
             SRDS_PROTOCOL_XAUI_FM, SRDS_PROTOCOL_XAUI_FM, SRDS_PROTOCOL_XAUI_FM, SRDS_PROTOCOL_XAUI_FM}, \
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},  \
/* 0x19 */  {SRDS_PROTOCOL_SGMII_FM, SRDS_PROTOCOL_SGMII_FM, \
             SRDS_PROTOCOL_PCIE2, SRDS_PROTOCOL_PCIE2, SRDS_PROTOCOL_SGMII_FM, SRDS_PROTOCOL_SGMII_FM, \
             0, 0, SRDS_PROTOCOL_SATA1, SRDS_PROTOCOL_SATA2}, \
/* 0x1A */  {SRDS_PROTOCOL_SGMII_FM, SRDS_PROTOCOL_SGMII_FM, \
             SRDS_PROTOCOL_SRIO2, SRDS_PROTOCOL_SRIO2, SRDS_PROTOCOL_SGMII_FM, SRDS_PROTOCOL_SGMII_FM, \
             0, 0, SRDS_PROTOCOL_SATA1, SRDS_PROTOCOL_SATA2}, \
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},  \
/* 0x1C */  {SRDS_PROTOCOL_PCIE1, SRDS_PROTOCOL_SGMII_FM, \
             SRDS_PROTOCOL_PCIE2, SRDS_PROTOCOL_PCIE2, SRDS_PROTOCOL_SGMII_FM, SRDS_PROTOCOL_SGMII_FM, \
             SRDS_PROTOCOL_AURORA, SRDS_PROTOCOL_SGMII_FM, 0, 0} \
}


/*****************************************************************************
 DDR INTEGRATION-SPECIFIC DEFINITIONS
******************************************************************************/
#define DDR_NUM_OF_VALID_CS     4

/*****************************************************************************
 DMA INTEGRATION-SPECIFIC DEFINITIONS
******************************************************************************/
#define DMA_NUM_OF_CONTROLLERS  2

/*****************************************************************************
 CPC INTEGRATION-SPECIFIC DEFINITIONS
******************************************************************************/

#define CPC_MAX_SIZE_SRAM_ERRATA_CPC4
#define CPC_HARDWARE_FLUSH_ERRATA_CPC10


#endif /* __PART_INTEGRATION_EXT_H */
