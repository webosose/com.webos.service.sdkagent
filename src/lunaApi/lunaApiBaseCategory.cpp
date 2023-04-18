// Copyright (c) 2022 LG Electronics, Inc.
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

#include "logging.h"
#include "lunaApiBaseCategory.h"

#define WEBOS_CONFIG_JSON "/var/lib/com.webos.service.sdkagent/config.json"

LunaApiBaseCategory::LunaApiBaseCategory() : pLSHandle(NULL),
                                             pCategory(NULL),
                                             pMethods(NULL)
{
}

LunaApiBaseCategory::~LunaApiBaseCategory()
{
}

bool LunaApiBaseCategory::initLunaServiceCategory(LSHandle *lsHandle)
{
    LSError lserror;
    LSErrorInit(&lserror);

    SDK_LOG_INFO(MSGID_SDKAGENT, 0, "[ %s : %d ] %s( ... )", __FILE__, __LINE__, __FUNCTION__);

    if (!LSRegisterCategory(lsHandle, pCategory, pMethods, NULL, NULL, &lserror))
    {
        LSErrorPrint(&lserror, stderr);
        LSErrorFree(&lserror);
        return false;
    }
    pLSHandle = lsHandle;

    return true;
}

void LunaApiBaseCategory::LSMessageReplyErrorInvalidConfigurations(LSHandle *sh, LSMessage *msg)
{
    LSError lserror;
    LSErrorInit(&lserror);

    bool retVal = LSMessageReply(sh, msg, "{\"returnValue\":false,\"errorCode\":4,\"errorText\":\"Invalid configurations.\"}", NULL);
    if (!retVal)
    {
        LSErrorPrint(&lserror, stderr);
        LSErrorFree(&lserror);
    }

    return;
}

void LunaApiBaseCategory::LSMessageReplyErrorUnknown(LSHandle *sh, LSMessage *msg)
{
    LSError lserror;
    LSErrorInit(&lserror);

    bool retVal = LSMessageReply(sh, msg, "{\"returnValue\":false,\"errorCode\":1,\"errorText\":\"Unknown error.\"}", NULL);
    if (!retVal)
    {
        LSErrorPrint(&lserror, stderr);
        LSErrorFree(&lserror);
    }

    return;
}

void LunaApiBaseCategory::LSMessageReplyErrorInvalidParams(LSHandle *sh, LSMessage *msg)
{
    LSError lserror;
    LSErrorInit(&lserror);

    bool retVal = LSMessageReply(sh, msg, "{\"returnValue\":false,\"errorCode\":2,\"errorText\":\"Invalid parameters.\"}", NULL);
    if (!retVal)
    {
        LSErrorPrint(&lserror, stderr);
        LSErrorFree(&lserror);
    }

    return;
}

void LunaApiBaseCategory::LSMessageReplyErrorBadJSON(LSHandle *sh, LSMessage *msg)
{
    LSError lserror;
    LSErrorInit(&lserror);

    bool retVal = LSMessageReply(sh, msg, "{\"returnValue\":false,\"errorCode\":3,\"errorText\":\"Malformed json.\"}", NULL);
    if (!retVal)
    {
        LSErrorPrint(&lserror, stderr);
        LSErrorFree(&lserror);
    }

    return;
}

void LunaApiBaseCategory::LSMessageReplyPayload(LSHandle *sh, LSMessage *msg, char *payload)
{
    LSError lserror;
    LSErrorInit(&lserror);

    bool retVal;

    if (payload == NULL)
    {
        retVal = LSMessageReply(sh, msg, "{\"returnValue\":true}", NULL);
    }
    else
    {
        retVal = LSMessageReply(sh, msg, payload, NULL);
    }

    if (!retVal)
    {
        LSErrorPrint(&lserror, stderr);
        LSErrorFree(&lserror);
    }

    return;
}

void LunaApiBaseCategory::postEvent(LSHandle *handle, void *subscribeKey, void *payload)
{
    LSError lserror;
    LSErrorInit(&lserror);

    // Post event message to the clients
    bool retVal = LSSubscriptionReply(
        handle,
        (char *)subscribeKey,
        (char *)payload,
        &lserror);

    if (!retVal)
    {
        LSErrorPrint(&lserror, stderr);
        LSErrorFree(&lserror);
    }

    return;
}

std::string LunaApiBaseCategory::executeCommand(std::string pszCommand)
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
            retStr = retStr.erase(retStr.length() - 1, 1);
        }
    }
    free(ln);
    pclose(fp);
    return retStr;
}

pbnjson::JValue LunaApiBaseCategory::convertStringToJson(const char *rawData)
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

std::mutex webOSConfigMutex;

pbnjson::JValue LunaApiBaseCategory::readwebOSConfigJson()
{
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
        configFile.close();
    }
    else
    {
        configFile.close();
        std::string cmd = "echo \"{}\" > ";
        cmd = cmd.append(WEBOS_CONFIG_JSON);
        executeCommand(cmd);
        readAllData = "{}";
    }
    webOSConfigMutex.unlock();
    return convertStringToJson((char *)(readAllData.c_str()));
}

bool LunaApiBaseCategory::writewebOSConfigJson(pbnjson::JValue webOSConfigJson)
{
    webOSConfigMutex.lock();
    std::ofstream fileOut;
    fileOut.open(WEBOS_CONFIG_JSON);
    fileOut.write(webOSConfigJson.stringify().c_str(), webOSConfigJson.stringify().size()); // Need Styled Writer?
    fileOut.close();
    webOSConfigMutex.unlock();
    return true;
}