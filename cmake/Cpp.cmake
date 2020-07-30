###############################################################################
# Copyright (c) 2019-2020 Advanced Micro Devices, Inc. All rights reserved
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
#
# Author: AMD Developer Tools Team
# Description: CMake code static analysis build directives
###############################################################################

cmake_minimum_required(VERSION 3.4.1)

# Set target build output directory
function(set_cxx_target_output_dir TARGET)
    set(OUTPUT_DIR_DEBUG "${BUILD_DIR_DEBUG}/output")
    set(OUTPUT_DIR_RELEASE "${BUILD_DIR_RELEASE}/output")
    set_target_properties(${TARGET}
            PROPERTIES
            ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${OUTPUT_DIR_DEBUG}/lib
            ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${OUTPUT_DIR_RELEASE}/lib
            LIBRARY_OUTPUT_DIRECTORY_DEBUG ${OUTPUT_DIR_DEBUG}/lib
            LIBRARY_OUTPUT_DIRECTORY_RELEASE ${OUTPUT_DIR_RELEASE}/lib
            RUNTIME_OUTPUT_DIRECTORY_DEBUG ${OUTPUT_DIR_DEBUG}/bin
            RUNTIME_OUTPUT_DIRECTORY_RELEASE ${OUTPUT_DIR_RELEASE}/bin)
endfunction()
