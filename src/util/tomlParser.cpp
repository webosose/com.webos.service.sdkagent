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

#include "tomlParser.h"
#include "common.h"
#include <fstream>
#include <iostream> // to be removed
#include <sstream>
#include <unordered_set>
#include <json-c/json.h>

// just a toml reader version for use in sdkagent service, not for general toml parsing
tomlObject readTomlFile(std::string filePath)
{
    tomlObject ret;

    std::ifstream fp(filePath);
    if (!fp || (fp.fail()))
    {
        fp.close();
        return ret;
    }

    std::string line;
    std::string section;

    while (std::getline(fp, line))
    {
        auto pos = line.find_first_not_of(' ');

        // ignore empty and comment line
        if ((pos >= line.size()) || (line[pos] == '#'))
            continue;

        // get section name
        if (line[pos] == '[')
        {
            section = "";
            auto sb = line.find_first_not_of("[ ", pos + 1);
            auto se = line.find_last_not_of("] ");
            section = line.substr(sb, se - sb + 1);
            ret[section] = {};
            continue;
        }

        auto equalPos = line.find('=');
        if (equalPos == std::string::npos)
        {
            ret = {};
            break;
        }
        else
        {
            std::string configParam = trim_string(line, 0, (int)equalPos - 1);
            ret[section][configParam] = removeAllSpaces(line.substr(equalPos + 1, line.size() - equalPos - 1));
        }
    }
    fp.close();

    return ret;
}

bool writeTomlSection(const std::string &filePath, const std::string &sectionName, const tomlSectionObject &obj)
{
    std::string strBuffer;
    strBuffer += "[" + sectionName + "]\n";
    for (const auto &sectionData : obj)
    {
        // ss << sectionData.first << "=" << sectionData.second << '\n';
        strBuffer += sectionData.first + "=" + sectionData.second + "\n";
    }

    std::ofstream fp(filePath);
    fp << strBuffer;
    fp.close();

    return true;
}

void writeTomlFile(const std::string &filePath, const tomlObject &obj)
{
    std::string strBuffer;
    std::string spc = "  ";
    for (auto &it : obj)
    {
        strBuffer += "[" + it.first + "]\n";
        for (auto &configParam : it.second)
        {
            strBuffer += spc + configParam.first + "=" + configParam.second + "\n";
        }
    }

    std::ofstream fp(filePath);
    fp << strBuffer;
    fp.close();
}

void disableTomlSection(const char * filePath, const std::unordered_set<std::string> &sections)
{
    std::string strBuffer = readTextFile(filePath);
    if (strBuffer.empty()) return;

    std::string section;
    bool commentOut = false;

    size_t currPos = 0;
    size_t lineBegPos = 0;

    while (lineBegPos < strBuffer.size())
    {
        auto pos = strBuffer.find_first_not_of(' ', currPos);
        lineBegPos = strBuffer.find('\n', currPos+1);
        if (lineBegPos == std::string::npos) {
            lineBegPos = strBuffer.size();
        } else {
            lineBegPos++;
        }
       
        if (pos < lineBegPos) {

            // new section found
            if (strBuffer[pos] == '[') {
                section = "";
                auto sb = strBuffer.find_first_not_of("[", pos + 1);
                size_t se = sb;
                while ((se < strBuffer.size()) && (strBuffer[se] != ']')) se++;
                section = strBuffer.substr(sb, se - sb);

                if (sections.find(section) != sections.end()) {
                    commentOut = true;
                    strBuffer.insert(currPos, "# ");
                    lineBegPos += 2;
                }
                else {
                    commentOut = false;
                }
            }
            else {
                if (commentOut) {
                    strBuffer.insert(currPos, "# ");
                    lineBegPos += 2;
                }
            }
        }

        currPos = lineBegPos;
    }

    writeTextFile(filePath, strBuffer);
}

std::string fixDoubleForwardSlash(std::string str)
{
    std::string token; //    92 \       47 /
    token.push_back(92);
    token.push_back(47);
    token.push_back(92);
    token.push_back(47);

    size_t pos = str.find(token);
    if (pos != std::string::npos)
    {
        str.replace(pos, token.size(), "//");
        return str;
    }

    return str;
}

std::string toStringArr(json_object *jValues)
{
    std::string cvalue = "[";
    for (size_t i = 0; i < json_object_array_length(jValues); i++)
    {
        json_object *val = json_object_array_get_idx(jValues, i);
        cvalue += fixDoubleForwardSlash(std::string(json_object_to_json_string(val)));
        cvalue += ",";
    }
    cvalue.back() = ']';
    return cvalue;
}

std::tuple<bool, tomlObject> jsonStringToTomlObject(const char *jsonStr)
{
    tomlObject ret;
    json_object *obj = json_tokener_parse(jsonStr);
    if (!obj)
    {
        return std::make_tuple(false, ret);
    }

    json_object_object_foreach(obj, section, sectionValue)
    {
        ret[section] = {};
        json_object_object_foreach(sectionValue, configParam, configValue)
        {
            switch (json_object_get_type(configValue))
            {
            case json_type::json_type_array:
                ret[section][configParam] = toStringArr(configValue);
                break;

            default:
                ret[section][configParam] = fixDoubleForwardSlash(std::string(json_object_to_json_string(configValue)));
                break;
            }
        }
    }

    return std::make_tuple(true, ret);
}

std::string tomlObjectToJsonString(const tomlObject &obj, const std::string &initialIndentation)
{
    std::string ret = "";
    const std::string tab = "    ";
    std::string indent = initialIndentation;

    ret = indent + "{\n";
    indent += tab;
    for (auto &section : obj)
    {
        ret += (indent + "\"" + section.first + "\": {\n");
        indent += tab;
        for (auto &config : section.second)
        {
            if (section.second.empty())
                continue;
            ret += indent + "\"" + config.first + "\": " + config.second + ",\n";
        }

        // remove last ,
        if (!section.second.empty())
        {
            ret[ret.size() - 2] = ' ';
        }

        // adjust the indent
        indent = indent.substr(0, indent.size() - tab.size());

        // add closing bracket here
        ret += (indent + "},\n");
    }
    // remove last ,
    if (!obj.empty())
    {
        ret[ret.size() - 2] = ' ';
    }
    indent = indent.substr(0, indent.size() - tab.size());
    ret += (indent + "}");

    return ret;
}

// void displayTomlFile(tomlObject & obj, std::string outFile)
// {
//     if (outFile.empty()) {
//         for (auto & section : obj) {
//             std::cout << '[' << section.first << ']' << std::endl;
//             for (auto & config : section.second) {
//                 std::cout << "    " << config.first << ": " << config.second << '\n';
//             }
//         }
//     }
//     else {
//         std::ofstream fp(outFile, fstream::app|fstream::out);
//         for (auto & section : obj) {
//             fp << '[' << section.first << "]\n";
//             for (auto & config : section.second) {
//                 fp << "    " << config.first << ": " << config.second << '\n';
//             }
//         }
//         fp.close();
//     }
// }
