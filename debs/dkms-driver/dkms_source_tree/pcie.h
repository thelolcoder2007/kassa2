/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2020 Verisilicon Inc.
 */

#ifndef _SN_PCIE_H_
#define _SN_PCIE_H_

#include <linux/types.h>
#include <linux/ioctl.h>

#include "common.h"

int sn_pci_init(struct sn_tranx_t *tdev);
long sn_pci_ioctl(struct file *filp, unsigned int cmd, unsigned long arg,
		  struct sn_tranx_t *tdev);
void sn_pci_release(struct sn_tranx_t *tdev);

void enable_interrupt_data_path(struct sn_tranx_t *tdev);
void disable_interrupt_data_path(struct sn_tranx_t *tdev);

pci_ers_result_t sn_err_detected(struct pci_dev *pdev,
				 pci_channel_state_t state);
pci_ers_result_t sn_slot_reset(struct pci_dev *pdev);
void sn_resume(struct pci_dev *pdev);
void sn_reset_prepare(struct pci_dev *pdev);
void sn_reset_done(struct pci_dev *pdev);
int sn_sriov_configure(struct pci_dev *pdev, int numvfs);
void pf_function_modules_release(struct sn_tranx_t *tdev);
int pf_function_modules_reinit(struct sn_tranx_t *tdev);
#endif /* _SN_PCIE_H_ */
