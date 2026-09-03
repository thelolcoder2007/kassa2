# Copyright (C) 2022 - 2025 Advanced Micro Devices, Inc. All rights reserved.
# Copyright (C) 2022, Xilinx Inc - All rights reserved
#
# Licensed under the Apache License, Version 2.0 (the "License"). You may
# not use this file except in compliance with the License. A copy of the
# License is located at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations
# under the License.

cmake_minimum_required (VERSION 3.16)

project (ma35_encoder_app DESCRIPTION "ma35_encoder_app" LANGUAGES C)

file (GLOB source "${PROJECT_SOURCE_DIR}/lib/src/*.c" "${PROJECT_SOURCE_DIR}/app/src/*.c")
add_executable (${PROJECT_NAME} "${source}")

target_link_libraries (${PROJECT_NAME}
        PRIVATE apps_common)

target_include_directories (${PROJECT_NAME}
        PRIVATE "${PROJECT_SOURCE_DIR}/lib/include"
        PRIVATE "${PROJECT_SOURCE_DIR}/app/include"
        PUBLIC "/opt/amd/ama/ma35/include/xma/"
        PUBLIC "/opt/amd/ama/ma35/include/xrm/"
        PUBLIC "/opt/amd/ama/ma35/include/xrm_interface/")

