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

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>
#include <tuple>

// tomlSectionObject mapping { config param -> config values }
typedef std::unordered_map<std::string, std::string> tomlSectionObject;

// tomlObject mapping { section -> tomlSectionObject }
typedef std::unordered_map<std::string, tomlSectionObject> tomlObject;

std::string trim_string(const std::string &str, size_t first, size_t last);
std::string trim_string(const std::string &str);

tomlObject readTomlFile(std::string filePath);

bool writeTomlSection(const std::string &filePath, const std::string &sectionName, const tomlSectionObject &obj);

void writeTomlFile(const std::string &filePath, const tomlObject &obj);

// void displayTomlFile(tomlObject & obj, std::string outFile = "");

std::tuple<bool, tomlObject> jsonStringToTomlObject(const char *jsonStr);

std::string tomlObjectToJsonString(const tomlObject &obj, const std::string &initialIndentation);
