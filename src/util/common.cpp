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

#include "common.h"
#include "logging.h"
#include <fstream>
#include <mutex>
#include <algorithm>
#include <cctype>

#define WEBOS_CONFIG_JSON "/var/lib/com.webos.service.sdkagent/config.json"

std::string executeCommand(const std::string& pszCommand, bool linefeedToSpace)
{
    FILE *fp = popen(pszCommand.c_str(), "r");
    if (!fp)
    {
        SDK_LOG_ERROR(MSGID_SDKAGENT, 0, "Error (!fp) [%d:%s]\n", errno, strerror(errno));
        return NULL;
    }

    std::string retStr = "";
    char *ln = NULL;
    size_t len = 0;
    while (getline(&ln, &len, fp) != -1)
    {
        if (ln == NULL)
        {
            SDK_LOG_INFO(MSGID_SDKAGENT, 0, "[ %s : %d ] %s( ... ), %s", __FILE__, __LINE__, __FUNCTION__, "ln == NULL");
            continue;
        }
        retStr = retStr.append(ln);
        if (retStr.at(retStr.length() - 1) == '\n')
        {
            retStr.pop_back();
            if (linefeedToSpace)
            {
                retStr = retStr.append(" ");
            }
        }
    }
    if ((linefeedToSpace) && (!retStr.empty()) && (retStr.at(retStr.length() - 1) == ' '))
    {
        retStr.pop_back();
    }
    free(ln);
    pclose(fp);

    return retStr;
}

bool is_number(const std::string & s)
{
    return !s.empty() && std::all_of(s.begin(), s.end(), ::isdigit);
}

/*
    Convert manually to avoid the SA Coverity problem when using stoi().
    Some lines of code may not be optimal, but they are compatible with SA.
*/
int string_to_positive_int(const std::string & sPID)
{
    int len = (int)sPID.length();
    if ((len < 10) && is_number(sPID)) {
        int ret = 0;
        int t = 1;
        for (int i = 0; i < len; i++) {
            int pos = len - i - 1;
            if (pos >= 0 && pos < (int)sPID.length()) {
                ret += t * (sPID[pos] - '0');
                t *= 10;
            }
        }
        return ret;
    }
    return -1;
}

float string_to_float(const char * s, int & ret) {
    char *ending = 0;
    float converted_value = strtof(s, &ending);
    if (*ending != 0) {
        SDK_LOG_ERROR(MSGID_SDKAGENT, 0, "Error converting string inside this square bracket [%s] to floating number\n", s);
    }
    return converted_value;
}

std::string trim_string(const std::string &str, int first, int last)
{
    int n = (int)str.size();
    if ((first >= n) || (last < 0))
        return "";
    while ((first < n) && (str[first] == ' '))
        first++;
    while ((last >= 0) && (str[last] == ' '))
        last--;
    if (first > last)
        return "";
    return str.substr(first, last - first + 1);
}

std::string trim_string(const std::string &str)
{
    int n = (int)str.size();
    int first = 0;
    int last = n - 1;

    if ((first >= n) || (last < 0))
        return "";
    while ((first < n) && (str[first] == ' '))
        first++;
    while ((last >= 0) && (str[last] == ' '))
        last--;
    if (first > last)
        return "";
    return str.substr(first, last - first + 1);
}

std::string removeAllSpaces(std::string str)
{
    size_t pos = 0;
    char locked = 0;
    for (size_t i = 0; i < str.size(); i++)
    {
        if (locked) {
            str[pos++] = str[i];
        }
        else {
            if (str[i] != ' ') {
                str[pos++] = str[i];
            }
        }

        if (str[i] == '\"') {
            locked = 1 - locked;
        }
    }
    str.erase(str.begin() + pos, str.end());
    return str;
}

bool fileExists(const char* filePath) {
    std::ifstream fp(filePath);
    bool ret = true;
    if (!fp || (fp.fail())) ret = false;
    fp.close();
    return ret;
}

bool isDevMode()
{
    return fileExists("/var/luna/preferences/devmode_enabled");
}

std::string readTextFile(const char *filePath)
{
    std::ifstream fp(filePath);
    if (!fp || fp.fail())
    {
        fp.close();
        return "";
    }

    std::string strData;
    fp.seekg(0, std::ios::end);
    strData.reserve(fp.tellg());
    fp.seekg(0, std::ios::beg);
    strData.assign(
        (std::istreambuf_iterator<char>(fp)),
        std::istreambuf_iterator<char>()
    );

    return strData;
}

void writeTextFile(const char* filePath, std::string &strData)
{
    std::ofstream fp(filePath);
    fp << strData;
    fp.close();
}

pbnjson::JValue stringToJValue(const char* rawData)
{
    pbnjson::JInput input(rawData);
    pbnjson::JSchema schema = pbnjson::JSchemaFragment("{}");
    pbnjson::JDomParser parser;
    if (!parser.parse(input, schema))
    {
        // return pbnjson::JValue();
        return pbnjson::Object();
    }
    return parser.getDom();
}

//===================================================================
// Read/Write webOS json config

std::mutex webOSConfigMutex;

pbnjson::JValue readWebOSJsonConfig()
{
    if (!fileExists(WEBOS_CONFIG_JSON)) {
        return pbnjson::Object();
    }

    webOSConfigMutex.lock();
    std::ifstream configFile(WEBOS_CONFIG_JSON);
    std::string readAllData;
    std::string readline;
    if (configFile.good())
    {
        while (getline(configFile, readline))
        {
            readAllData += readline;
        }
    }
    configFile.close();
    webOSConfigMutex.unlock();
    return stringToJValue(readAllData.c_str());
}

void writeWebOSConfigJson(pbnjson::JValue webOSConfigJson)
{
    webOSConfigMutex.lock();
    std::ofstream fileOut;
    fileOut.open(WEBOS_CONFIG_JSON);
    const std::string strData = webOSConfigJson.stringify();
    fileOut.write(strData.c_str(), strData.size());
    fileOut.close();
    webOSConfigMutex.unlock();
}

// void cReadTextFile(const char * filePath, char *& buffer) {
//     FILE * fp = fopen(filePath, "r");
//     if (!fp) {
//         perror("Error opening file: ");
//         perror(filePath);
//         return;
//     }

//     fseek(fp, 0, SEEK_END);
//     long int fsize = ftell(fp);
//     fseek(fp, 0, SEEK_SET);     // set pointer to the beginning of the file

//     buffer = (char*) malloc(fsize + 1);
//     fread(buffer, fsize, 1, fp);

//     if (fclose(fp) != 0) {
//         perror("Error closing file: ");
//         perror(filePath);
//         if (buffer != NULL) {
//             free(buffer);
//             buffer = NULL;
//         }
//         return;
//     }
// }