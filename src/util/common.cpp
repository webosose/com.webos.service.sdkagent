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

std::string executeCommand(std::string pszCommand, bool linefeedToSpace)
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

bool fileExists(const char* filePath) {
    std::ifstream fp(filePath);
    bool ret = true;
    if (!fp || (fp.fail())) ret = false;
    fp.close();
    return ret;
}

pbnjson::JValue stringToJValue(const char* rawData)
{
    pbnjson::JInput input(rawData);
    pbnjson::JSchema schema = pbnjson::JSchemaFragment("{}");
    pbnjson::JDomParser parser;
    if (!parser.parse(input, schema))
    {
        return pbnjson::JValue();
    }
    return parser.getDom();
}
