#!/bin/bash

#
# Copyright (C) 2020 -2021 Xilinx, Inc. All rights reserved.
#

echo "systemctl stop xrmd"
systemctl stop xrmd

echo "systemctl start xrmd"
systemctl start xrmd
