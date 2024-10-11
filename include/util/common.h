// Copyright (c) 2024 LG Electronics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#ifndef __COMMON_UTILS_H__
#define __COMMON_UTILS_H__

#include <string>
#include <pbnjson.hpp>

std::string executeCommand(const std::string & pszCommand, bool linefeedToSpace = false);

std::string trim_string(const std::string &str, int first, int last);

std::string trim_string(const std::string &str);

bool is_number(const std::string &s);

int string_to_positive_int(const std::string &sPID);

float string_to_float(const char * s);

bool fileExists(const char* filePath);

bool isDevMode();

std::string removeAllSpaces(std::string str);

std::string readTextFile(const char* filePath);

void writeTextFile(const char* filePath, std::string & strBuffer);

pbnjson::JValue stringToJValue(const char* rawData);

pbnjson::JValue readWebOSJsonConfig();

void writeWebOSConfigJson(pbnjson::JValue);

#endif