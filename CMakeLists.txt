# Copyright (c) 2020 LG Electronics, Inc.
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
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 2.8.7)
project(com.webos.service.sdkagent CXX)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_BUILD_TYPE Debug)

include(webOS/webOS)
webos_modules_init(1 6 3)
webos_component(0 0 1)
webos_include_install_paths()

include(FindPkgConfig)

# setup for linking necessary libraries
pkg_check_modules(GLIB2 REQUIRED glib-2.0)
include_directories(${GLIB2_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${GLIB2_CFLAGS_OTHER})

pkg_check_modules(JSONC REQUIRED json-c)
include_directories(${JSONC_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${JSONC_CFLAGS_OTHER})

pkg_check_modules(LUNASERVICE2 REQUIRED luna-service2)
include_directories(${LUNASERVICE2_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${LUNASERVICE2_CFLAGS_OTHER})

pkg_check_modules(PMLOGLIB REQUIRED PmLogLib)
include_directories(${PMLOGLIB_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${PMLOGLIB_CFLAGS_OTHER})

pkg_check_modules(PBNJSON_CPP REQUIRED pbnjson_cpp)
include_directories(${PBNJSON_CPP_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${PBNJSON_CPP_CFLAGS_OTHER})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -Wall -g -O3 -fpermissive")

# add include files
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/lunaApi
    ${CMAKE_SOURCE_DIR}/include/util
)

set(SRC_DIR "${CMAKE_SOURCE_DIR}/src")
set(SYSTEMD_FILE_DIR "${CMAKE_SOURCE_DIR}/files/systemd")

set(SRC_LIST
    ${SRC_DIR}/lunaApi/lunaApiBaseCategory.cpp
    ${SRC_DIR}/lunaApi/lunaApiCollector.cpp
    ${SRC_DIR}/util/logging.cpp
    ${SRC_DIR}/main.cpp
)

# create excutable file
set(BIN_NAME ${PROJECT_NAME})
add_executable(${BIN_NAME} ${SRC_LIST})

# link
target_link_libraries(
    ${BIN_NAME}
    ${GLIB2_LDFLAGS}
    ${LUNASERVICE2_LDFLAGS}
    ${JSONC_LDFLAGS}
    ${PMLOGLIB_LDFLAGS}
    ${PBNJSON_CPP_LDFLAGS}
)

# install binary
install(TARGETS ${BIN_NAME} DESTINATION ${CMAKE_INSTALL_SBINDIR})

# install systemd files
#install(FILES ${SYSTEMD_FILE_DIR}/${BIN_NAME}.service DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/systemd/system)
webos_build_configured_file(files/systemd/${PROJECT_NAME}.service SYSCONFDIR systemd/system)

# add dbus and lunaservice2 from  files/sysbus/*
webos_build_system_bus_files()
