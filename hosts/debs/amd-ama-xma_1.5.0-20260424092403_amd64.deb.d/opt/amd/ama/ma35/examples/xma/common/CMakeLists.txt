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

project (apps_common DESCRIPTION "xma apps common library" LANGUAGES C)

file (GLOB source "${PROJECT_SOURCE_DIR}/src/*.c")
add_library (${PROJECT_NAME} STATIC "${source}")

target_link_libraries (${PROJECT_NAME}
        PUBLIC "/opt/amd/ama/ma35/lib/libxma.so"
        PUBLIC "/opt/amd/ama/ma35/lib/libxrm_interface.so")
target_include_directories (${PROJECT_NAME} 
        PUBLIC "${PROJECT_SOURCE_DIR}/include"
        PUBLIC "/opt/amd/ama/ma35/include/xma/"
        PUBLIC "/opt/amd/ama/ma35/include/xrm/" 
        PUBLIC "/opt/amd/ama/ma35/include/xrm_interface/")

