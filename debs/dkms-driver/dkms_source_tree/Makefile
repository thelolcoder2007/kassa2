# SPDX-License-Identifier: GPL-2.0-or-later */
#
# Copyright (C) 2023 Verisilicon Inc.
#

.PHONY: all clean

MODULE_NAME ?= ama_transcoder

ifeq ($(KERNEL_DIR),)
$(warning "-------------------------------------------------")
$(warning "Warning: KERNEL_DIR not set")
$(warning "Warning: kernel driver will not be built")
$(warning "-------------------------------------------------")

else
all:
	$(MAKE) -f Kbuild M=`pwd` V=0 $(BUILD_OPTIONS) AQROOT=`pwd`
clean:
	$(MAKE) -f Kbuild M=`pwd` $(BUILD_OPTIONS) AQROOT=`pwd` clean
	rm -rf hdmatest  mem_test
install:
	sudo cp $(MODULE_NAME).ko $(DRV_INSTALL_DIR)
uninstall:
	sudo rm -f $(DRV_INSTALL_DIR)/$(MODULE_NAME).ko
deploy:
	cp $(MODULE_NAME).ko $(TRANS_DEP_PATH)/driver/lib/modules/$(shell uname -r)
test:
	gcc -o hdmatest hdmatest.c -lhugetlbfs
	gcc -o hdma_copy hdma_copy.c -lhugetlbfs
	gcc -o mem_test mem_test.c
	gcc -o pcie_ddr_memtest -g pcie_ddr_memtest.c -lhugetlbfs
endif
