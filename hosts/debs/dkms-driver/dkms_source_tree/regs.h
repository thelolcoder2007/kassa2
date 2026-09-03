/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2020 Verisilicon Inc.
 */

#ifndef _SN_REGS_H_
#define _SN_REGS_H_

//#define EMULATOR

/* only for test */
//#define TEST_MSIX_MSI_IRQ
//#define ENABLE_MSI_MODE
//#define ENABLE_HDMA_MSI  /*pass though glue logic */
/* end */

/*
 * Using this command, you can get PCIe information, include BAR address and size.
 * lspci -vd 10ee:5070
 */

/* IP's register offset base on bar0 */
#define PF_HDMA_BASE_OFF 0x00001000

/* DDR offset base on bar4 */
#define S1_DDR_OFF 0x00000000
#define S2_DDR_OFF 0x04000000

/* in PF_MODE, IP's register offset base on bar2 */
#define PCIE_CTL_OFF 0x00000000
#define TOP_SYS_CON_OFF 0x00800000

#define S1_VC8000D_A_OFF 0x00B00000
#define S1_VC8000D_B_OFF 0x00B40000
#define S1_VC8000E_OFF 0x00C00000
#define S1_ABR_SCL_CORE_OFF 0x00CC0000
#define S1_ABR_SCL_DEC400_OFF 0x00CE0000
#define S1_MB_0_AXI_LMB_OFF 0x01000000
#define S1_MB_1_AXI_LMB_OFF 0x01100000
#define S1_MB_2_AXI_LMB_OFF 0x01200000
#define S1_MB_3_AXI_LMB_OFF 0x01300000
#define S1_MBOX_AXIL_OFF 0x01400000
#define S1_MB_0_AXIL_MDM_OFF 0x01410000
#define S1_MB_1_AXIL_MDM_OFF 0x01420000
#define S1_MB_2_AXIL_MDM_OFF 0x01430000
#define S1_MB_3_AXIL_MDM_OFF 0x01440000
#define S1_XLNX_AXIL_ENCA_OFF 0x01450000
#define S1_XLNX_AXIL_ENCB_OFF 0x01460000
#define S1_XLNX_AXIL_LA_OFF 0x01470000
#define S1_XLNX_DEC_OFF 0x01480000
#define S1_AXI_SRAM_OFF 0x01500000
#define S1_SYS_CON_OFF 0x01900000
#define S1_HDMA_BASE_OFF (S1_SYS_CON_OFF+0xF0000)

#define S2_VC8000D_A_OFF 0x01A00000
#define S2_VC8000D_B_OFF 0x01A40000
#define S2_VC8000E_OFF 0x01B00000
#define S2_ABR_SCL_CORE_OFF 0x01BC0000
#define S2_ABR_SCL_DEC400_OFF 0x01BE0000
#define S2_MB_0_AXI_LMB_OFF 0x01F00000
#define S2_MB_1_AXI_LMB_OFF 0x02000000
#define S2_MB_2_AXI_LMB_OFF 0x02100000
#define S2_MB_3_AXI_LMB_OFF 0x02200000
#define S2_MBOX_AXIL_OFF 0x02300000
#define S2_MB_0_AXIL_MDM_OFF 0x02310000
#define S2_MB_1_AXIL_MDM_OFF 0x02320000
#define S2_MB_2_AXIL_MDM_OFF 0x02330000
#define S2_MB_3_AXIL_MDM_OFF 0x02340000
#define S2_XLNX_AXIL_ENCA_OFF 0x02350000
#define S2_XLNX_AXIL_ENCB_OFF 0x02360000
#define S2_XLNX_AXIL_LA_OFF 0x02370000
#define S2_XLNX_DEC_OFF 0x02380000
#define S2_AXI_SRAM_OFF 0x02400000
#define S2_SYS_CON_OFF 0x02800000
#define S2_HDMA_BASE_OFF (S2_SYS_CON_OFF+0xF0000)

/* in TWO_VF_MODE, IP's register offset in vf base on bar2 */
#define VF_VC8000D_A_OFF                        0x00000000
#define VF_VC8000D_B_OFF                      	0x00040000
#define VF_VC8000E_OFF                          0x00100000
#define VF_ABR_SCL_CORE_OFF                     0x001C0000
#define VF_ABR_SCL_DEC400_OFF                   0x001E0000
#define VF_MB_0_AXI_LMB_OFF                     0x00500000
#define VF_MB_1_AXI_LMB_OFF                     0x00600000
#define VF_MB_2_AXI_LMB_OFF                     0x00700000
#define VF_MB_3_AXI_LMB_OFF                     0x00800000
#define VF_MBOX_AXIL_OFF                        0x00900000
#define VF_MB_0_AXIL_MDM_OFF                    0x00910000
#define VF_MB_1_AXIL_MDM_OFF                    0x00920000
#define VF_MB_2_AXIL_MDM_OFF                    0x00930000
#define VF_MB_3_AXIL_MDM_OFF                    0x00940000
#define VF_XLNX_AXIL_ENCA_OFF                   0x00950000
#define VF_XLNX_AXIL_ENCB_OFF                   0x00960000
#define VF_XLNX_AXIL_LA_OFF                     0x00970000
#define VF_XLNX_DEC_OFF                         0x00980000
#define VF_NOC_VCD_SUB_OFF                      0x00A00000
#define VF_NOC_VCE_SUB_OFF                      0x00B00000
#define VF_NOC_VIP_GC_SCL_SUB_OFF               0x00C00000
#define VF_NOC_ABR_SCL_OFF                      0x00D00000
#define VF_NOC_FPS_OFF                          0x00E00000
#define VF_NOC_XLNX_ENC_OFF                     0x00F00000
#define VF_AXI_SRAM_OFF                         0x01000000
#define VF_SYS_CON_OFF                          0x01400000
#define VF_HDMA_BASE_OFF                        (VF_SYS_CON_OFF+0xF0000)
#define VF_VCE_IM_OFF                           (VF_VC8000E_OFF + 0x4000)
#define VF_THS_PCIE_INT_EN_CON_STUS             (VF_SYS_CON_OFF + 0x400)
#define VF_XLNX_ENC_OFF                         0x01500000
#define VF_TOP_SYS_CON_OFF                      (0x01A00000 + 0x00800000)

/* in ONE_VF_MODE, IP's register offset in vf base on bar2 */
#define ONE_VF_S1_VC8000D_A_OFF                  0x00000000
#define ONE_VF_S1_VC8000D_B_OFF                  0x00040000
#define ONE_VF_S1_VC8000E_OFF                    0x00100000
#define ONE_VF_S1_ABR_SCL_CORE_OFF               0x001C0000
#define ONE_VF_S1_ABR_SCL_DEC400_OFF             0x001E0000
#define ONE_VF_S1_MB_0_AXI_LMB_OFF               0x00500000
#define ONE_VF_S1_MB_1_AXI_LMB_OFF               0x00600000
#define ONE_VF_S1_MB_2_AXI_LMB_OFF               0x00700000
#define ONE_VF_S1_MB_3_AXI_LMB_OFF               0x00800000
#define ONE_VF_S1_MBOX_AXIL_OFF                  0x00900000
#define ONE_VF_S1_MB_0_AXIL_MDM_OFF              0x00910000
#define ONE_VF_S1_MB_1_AXIL_MDM_OFF              0x00920000
#define ONE_VF_S1_MB_2_AXIL_MDM_OFF              0x00930000
#define ONE_VF_S1_MB_3_AXIL_MDM_OFF              0x00940000
#define ONE_VF_S1_XLNX_AXIL_ENCA_OFF             0x00950000
#define ONE_VF_S1_XLNX_AXIL_ENCB_OFF             0x00960000
#define ONE_VF_S1_XLNX_AXIL_LA_OFF               0x00970000
#define ONE_VF_S1_XLNX_DEC_OFF                   0x00980000
#define ONE_VF_S1_AXI_SRAM_OFF                   0x01000000
#define ONE_VF_S1_SYS_CON_OFF                    0x01400000
#define ONE_VF_HDMA_BASE_OFF                     (ONE_VF_S1_SYS_CON_OFF+0xF0000)

#define ONE_VF_S2_VC8000D_A_OFF                  0x01500000
#define ONE_VF_S2_VC8000D_B_OFF                  0x01540000
#define ONE_VF_S2_VC8000E_OFF                    0x01600000
#define ONE_VF_S2_ABR_SCL_CORE_OFF               0x016C0000
#define ONE_VF_S2_ABR_SCL_DEC400_OFF             0x016E0000
#define ONE_VF_S2_MB_0_AXI_LMB_OFF               0x01A00000
#define ONE_VF_S2_MB_1_AXI_LMB_OFF               0x01B00000
#define ONE_VF_S2_MB_2_AXI_LMB_OFF               0x01C00000
#define ONE_VF_S2_MB_3_AXI_LMB_OFF               0x01D00000
#define ONE_VF_S2_MBOX_AXIL_OFF                  0x01E00000
#define ONE_VF_S2_MB_0_AXIL_MDM_OFF              0x01E10000
#define ONE_VF_S2_MB_1_AXIL_MDM_OFF              0x01E20000
#define ONE_VF_S2_MB_2_AXIL_MDM_OFF              0x01E30000
#define ONE_VF_S2_MB_3_AXIL_MDM_OFF              0x01E40000
#define ONE_VF_S2_XLNX_AXIL_ENCA_OFF             0x01E50000
#define ONE_VF_S2_XLNX_AXIL_ENCB_OFF             0x01E60000
#define ONE_VF_S2_XLNX_AXIL_LA_OFF               0x01E70000
#define ONE_VF_S2_XLNX_DEC_OFF                   0x01E80000
#define ONE_VF_S2_AXI_SRAM_OFF                   0x02500000
#define ONE_VF_S2_SYS_CON_OFF                    0x02900000

#define ONE_VF_S1_VCE_IM_OFF                     (ONE_VF_S1_VC8000E_OFF + 0x4000)
#define ONE_VF_S2_VCE_IM_OFF                     (ONE_VF_S2_VC8000E_OFF + 0x4000)

#define ONE_VF_THS1_PCIE_INT_EN_CON_STUS         (ONE_VF_S1_SYS_CON_OFF + 0x400)
#define ONE_VF_THS2_PCIE_INT_EN_CON_STUS         (ONE_VF_S2_SYS_CON_OFF + 0x400)
#define ONE_VF_TOP_SYS_CON_OFF                   (0x03400000 + 0x00800000)

#define SRAM_SC_OFF 0x02900000
#define ROM_SC_OFF 0x02A00000
#define SPI_0_OFF 0x02A10000
#define SPI_1_OFF 0x02A11000
#define SPI_2_OFF 0x02A12000
#define DMA_OFF 0x02A13000
#define WDT_SC_OFF 0x02A14000
#define I2C_0_OFF 0x02A15000
#define I2C_1_OFF 0x02A16000
#define I2C_3_OFF 0x02A17000
#define INTC_OFF 0x02A18000
#define UART_0_OFF 0x02A19000
#define UART_1_OFF 0x02A1A000
#define TIMER_0_OFF 0x02A1B000
#define TIMER_1_OFF 0x02A1B800
#define THERMAL_CTRL_OFF 0x02A1C000
#define OTP_CTRL_OFF 0x02A1D000
#define GPIO_0_OFF 0x02A1E000
#define GPIO_1_OFF 0x02A1F000
#define GPIO_2_OFF 0x02A20000
#define GPIO_3_OFF 0x02A21000
#define GPIO_4_OFF 0x02A22000
#define GPIO_5_OFF 0x02A23000
#define ZSP_ITCM_OFF 0x02BC0000
#define ZSP_DTCM_OFF 0x02CC0000

#define S1_INT_SD_VCE_ISR (S1_SYS_CON_OFF + 0x9F4)
#define S1_INT_SD_VCE_LA_ISR (S1_SYS_CON_OFF + 0x9F0)

#define S1_INT_SD_XABR_ISR (S1_SYS_CON_OFF + 0x9EC)
#define S1_INT_SD_XENC_STUS (S1_SYS_CON_OFF + 0x9E8)
#define S1_INT_SD_XENC_ISR (S1_SYS_CON_OFF + 0x9E4)

#define S2_INT_SD_VCE_ISR (S2_SYS_CON_OFF + 0x9F4)
#define S2_INT_SD_VCE_LA_ISR (S2_SYS_CON_OFF + 0x9F0)
#define S2_INT_SD_XABR_ISR (S2_SYS_CON_OFF + 0x9EC)
#define S2_INT_SD_XENC_STUS (S2_SYS_CON_OFF + 0x9E8)
#define S2_INT_SD_XENC_ISR (S2_SYS_CON_OFF + 0x9E4)
#define PCIE_GLUE_LOGIC_OFF (TOP_SYS_CON_OFF + 0x60200)
/* VCE modify */
#define VF_INT_SD_VCE_ISR (VF_SYS_CON_OFF + 0x9F4)
#define VF_INT_SD_VCE_LA_ISR (VF_SYS_CON_OFF + 0x9F0)

#define ONE_VF_S1_INT_SD_VCE_ISR (ONE_VF_S1_SYS_CON_OFF + 0x9F4)
#define ONE_VF_S1_INT_SD_VCE_LA_ISR (ONE_VF_S1_SYS_CON_OFF + 0x9F0)
#define ONE_VF_S2_INT_SD_VCE_ISR (ONE_VF_S2_SYS_CON_OFF + 0x9F4)
#define ONE_VF_S2_INT_SD_VCE_LA_ISR (ONE_VF_S2_SYS_CON_OFF + 0x9F0)


#define PCIE_INT_EN_CON_STUS (TOP_SYS_CON_OFF + 0x400)
#define THS1_PCIE_INT_EN_CON_STUS (S1_SYS_CON_OFF + 0x400)
#define THS2_PCIE_INT_EN_CON_STUS (S2_SYS_CON_OFF + 0x400)

/* TWO_VF MODE VF <-->PF: */
#define ALL_VF_TO_PF_INT_CON_STUS          (VF_SYS_CON_OFF + 0x40C)
#define ALL_VF_FROM_PF_INT_CON_STUS        (VF_SYS_CON_OFF + 0x41C)
#define ALL_VF_TO_PF_MAILBOX               (VF_SYS_CON_OFF + 0x800)
#define ALL_VF_FROM_PF_MAILBOX             (VF_SYS_CON_OFF + 0x900)
/* PF MODE VF <-->PF: */
/* share memory between VF1 and PF slice1, total size:0x200 */
#define VF1_TO_PF_MAILBOX (S1_SYS_CON_OFF + 0x800)
#define PF_TO_VF1_MAILBOX (S1_SYS_CON_OFF + 0x900)
/* share memory between VF2 and PF slice2, total size:0x200 */
#define VF2_TO_PF_MAILBOX (S2_SYS_CON_OFF + 0x800)
#define PF_TO_VF2_MAILBOX (S2_SYS_CON_OFF + 0x900)

#define THS1_APB_AXI_ERR_BYPASS_CON_STUS                                       \
	(S1_SYS_CON_OFF + 0x308) /* 0 ~ 18 bit*/
#define THS2_APB_AXI_ERR_BYPASS_CON_STUS                                       \
	(S2_SYS_CON_OFF + 0x308) /* 0 ~ 18 bit*/


#define VF_MODE_SEL (TOP_SYS_CON_OFF + 0x3f8)
#define THS_ISOLATE_SEL (TOP_SYS_CON_OFF + 0x3fc)
#define CHIP_SFT_INT_CON_STUS (TOP_SYS_CON_OFF + 0x410)
#define FLR_HANDLING_CON_STUS (TOP_SYS_CON_OFF + 0x1ec)
#define BOOT_MODE_STUS (TOP_SYS_CON_OFF + 0x398)

/* pcie index */
#define PF_INDEX                     0x0
#define VF1_INDEX                    0x1
#define VF2_INDEX                    0x2
#define DEVICE_TYPE_PF               0x0
#define DEVICE_TYPE_VF               0x1
/* zsp test */
#define ZSP_INT_0_STUS		(TOP_SYS_CON_OFF + 0x418)
#define VF_ID_CON_STUS     ((VF_SYS_CON_OFF)+0x300)
/* HDMA channel*/
#define PCIE_SIDEBAND_CON_STUS (TOP_SYS_CON_OFF + 0x3f4)

#endif /* _SN_REGS_H_ */
