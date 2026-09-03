#!/bin/bash
# Copyright 2024 Xilinx, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
/opt/amd/ama/ma35/bin/xrmd &
sleep 2
echo -e "loading xrm plugins...."
/opt/amd/ama/ma35/bin/xrmadm /opt/amd/ama/ma35/scripts/load_xrm_plugins_cmd.json
wait
