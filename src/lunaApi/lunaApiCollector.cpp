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
#include "threadForInterval.h"
#include "threadForSocket.h"
#include "lunaApiCollector.h"
#include "tomlParser.h"
#include "common.h"
#include "telegrafController.h"
#include <algorithm>
#include <json-c/json.h>
#include <pbnjson.hpp>

LunaApiCollector *LunaApiCollector::_instance = nullptr;

#define TELEGRAF_CONFIG_DIR "/var/lib/com.webos.service.sdkagent/telegraf/telegraf.d/"
#define TELEGRAF_MAIN_CONFIG "/var/lib/com.webos.service.sdkagent/telegraf.d/telegraf.conf"
#define START_ON_BOOT_FLAG "/var/lib/com.webos.service.sdkagent/startOnBoot"

// luna API lists
const LSMethod LunaApiCollector::collectorMethods[] = {
    {"start", start, LUNA_METHOD_FLAGS_NONE},
    {"stop", stop, LUNA_METHOD_FLAGS_NONE},
    {"restart", restart, LUNA_METHOD_FLAGS_NONE},
    {"startOnBoot", startOnBoot, LUNA_METHOD_FLAGS_NONE},
    {"getStatus", getStatus, LUNA_METHOD_FLAGS_NONE},

    {"getConfig", getConfig, LUNA_METHOD_FLAGS_NONE},
    {"setConfig", setConfig, LUNA_METHOD_FLAGS_NONE},

    {"getData", getData, LUNA_METHOD_FLAGS_NONE},
    {NULL, NULL},
};

// Called when starting the service
void LunaApiCollector::initialize()
{
    if (fileExists(START_ON_BOOT_FLAG)) {
        TelegrafController::getInstance()->start();
    }
}

LunaApiCollector::LunaApiCollector()
{
    pCategory = "/collector";
    pMethods = (LSMethod *)collectorMethods;

    pThreadForInterval = new ThreadForInterval();
    pThreadForSocket = new ThreadForSocket();
}

LunaApiCollector::~LunaApiCollector()
{
    delete pThreadForInterval;
    delete pThreadForSocket;
}

// luna-send -f -n 1 luna://com.webos.service.sdkagent/collector/start '{}'
bool LunaApiCollector::start(LSHandle *sh, LSMessage *msg, void *data)
{
    if (!json_tokener_parse(LSMessageGetPayload(msg)))
    {
        Instance()->LSMessageReplyErrorBadJSON(sh, msg);
        return false;
    }

    TelegrafController::getInstance()->start();
    Instance()->LSMessageReplyPayload(sh, msg, NULL);

    return true;
}

// luna-send -f -n 1 luna://com.webos.service.sdkagent/collector/stop '{}'
bool LunaApiCollector::stop(LSHandle *sh, LSMessage *msg, void *data)
{
    if (!json_tokener_parse(LSMessageGetPayload(msg)))
    {
        Instance()->LSMessageReplyErrorBadJSON(sh, msg);
        return false;
    }

    TelegrafController::getInstance()->stop();
    Instance()->LSMessageReplyPayload(sh, msg, NULL);

    return true;
}

// luna-send -f -n 1 luna://com.webos.service.sdkagent/collector/restart '{}'
bool LunaApiCollector::restart(LSHandle *sh, LSMessage *msg, void *data)
{
    if (!json_tokener_parse(LSMessageGetPayload(msg)))
    {
        Instance()->LSMessageReplyErrorBadJSON(sh, msg);
        return false;
    }

    TelegrafController::getInstance()->restart();
    Instance()->LSMessageReplyPayload(sh, msg, NULL);

    return true;
}

// luna-send -f -n 1 luna://com.webos.service.sdkagent/collector/startOnBoot '{"enable": false}'
bool LunaApiCollector::startOnBoot(LSHandle *sh, LSMessage *msg, void *data)
{
    if (!json_tokener_parse(LSMessageGetPayload(msg)))
    {
        Instance()->LSMessageReplyErrorBadJSON(sh, msg);
        return false;
    }

    pbnjson::JValue paramObj = stringToJValue(LSMessageGetPayload(msg));
    if (!paramObj.hasKey("enable") || !paramObj["enable"].isBoolean())
    {
        Instance()->LSMessageReplyErrorInvalidParams(sh, msg);
        return false;
    }

    bool isEnable = paramObj["enable"].asBool();
    std::string cmd = isEnable ? "touch " + std::string(START_ON_BOOT_FLAG) : "rm " + std::string(START_ON_BOOT_FLAG);
    executeCommand(cmd.c_str());
    Instance()->LSMessageReplyPayload(sh, msg, NULL);

    return true;
}

// luna-send -f -n 1 luna://com.webos.service.sdkagent/collector/getStatus '{}'
bool LunaApiCollector::getStatus(LSHandle *sh, LSMessage *msg, void *data)
{
    if (!json_tokener_parse(LSMessageGetPayload(msg)))
    {
        Instance()->LSMessageReplyErrorBadJSON(sh, msg);
        return false;
    }

    pbnjson::JValue reply = pbnjson::Object();
    reply.put("returnValue", true);
    std::string status = TelegrafController::getInstance()->isRunning() ? "active" : "inactive";
    reply.put("status", status);
    reply.put("startOnBoot", fileExists(START_ON_BOOT_FLAG));

    Instance()->LSMessageReplyPayload(sh, msg, reply.stringify().c_str());

    return true;
}

// luna-send -f -n 1 luna://com.webos.service.sdkagent/collector/getConfig '{}'
bool LunaApiCollector::getConfig(LSHandle *sh, LSMessage *msg, void *data)
{
    if (!json_tokener_parse(LSMessageGetPayload(msg)))
    {
        Instance()->LSMessageReplyErrorBadJSON(sh, msg);
        return false;
    }

    tomlObject telegrafConfig = TelegrafController::getInstance()->getConfig();

    std::string replyConfig = tomlObjectToJsonString(telegrafConfig, std::string("    "));
    replyConfig = "{\n    \"returnValue\": true,\n    \"config\": " + replyConfig + "\n}";

    Instance()->LSMessageReplyPayload(sh, msg, replyConfig.c_str());
    return true;
}

bool LunaApiCollector::setConfig(LSHandle *sh, LSMessage *msg, void *data)
{
    if (TelegrafController::getInstance()->isRunning())
    {
        // cannot set the config while telegraf is running
        bool retVal = LSMessageReply(sh, msg, "{\"returnValue\":false,\"errorCode\":5,\"errorText\":\"Collector is active (running).\"}", NULL);
        if (!retVal)
        {
            LSError lserror;
            LSErrorInit(&lserror);
            LSErrorPrint(&lserror, stderr);
            LSErrorFree(&lserror);
        }
        return false;
    }

    bool validJsonPayload = true;
    tomlObject inputConfig;
    std::tie(validJsonPayload, inputConfig) = jsonStringToTomlObject(LSMessageGetPayload(msg));

    if (!validJsonPayload)
    {
        Instance()->LSMessageReplyErrorBadJSON(sh, msg);
        return false;
    }

    // check if all configurations in inputConfig are in availableConfigurations
    if (!TelegrafController::getInstance()->checkInputConfig(inputConfig))
    {
        Instance()->LSMessageReplyErrorInvalidParams(sh, msg);
        return false;
    }

    if (!TelegrafController::getInstance()->updateConfig(inputConfig))
    {
        Instance()->LSMessageReplyErrorInvalidConfigurations(sh, msg);
        return false;
    }

    Instance()->LSMessageReplyPayload(sh, msg, "{\"returnValue\": true}");
    return true;
}

size_t findIndex(std::string inputString, std::string divider, int startIndex)
{
    size_t retIndex = inputString.find(divider, startIndex);
    if (retIndex == std::string::npos)
    {
        retIndex = inputString.length();
    }
    else if (retIndex > 0)
    {
        while (inputString.substr(retIndex - 1, 2).compare("\\" + divider) == 0)
        {
            retIndex = inputString.find(divider, (retIndex + 1));
        }
    }
    return retIndex;
}

pbnjson::JValue convertDataToJson(std::string pData)
{
    pbnjson::JValue reply = pbnjson::Object();
    size_t prev = 0;
    size_t cur = 0;
    std::string divider = ",";
    while (true)
    {
        cur = findIndex(pData, divider, prev);
        std::string subStr = pData.substr(prev, cur - prev);
        int doubleQuotationCount = 0;
        size_t divideIndex = 0;
        for (divideIndex = 0; divideIndex < subStr.length(); divideIndex++)
        {
            char &c = subStr.at(divideIndex);
            if (c == '"')
            {
                doubleQuotationCount++;
            }
            if ((doubleQuotationCount % 2 == 0) && (c == '='))
            {
                c = ':';
                break;
            }
        }
        reply.put(subStr.substr(0, divideIndex), pbnjson::JValue(subStr.substr(divideIndex + 1, subStr.length() - 1)));
        prev = cur + divider.length();
        if (cur == pData.length())
        {
            break;
        }
    }
    return reply;
}

/**
 * Execute shell command for read data once
 * Parse to JSON
 * Return
 */
bool LunaApiCollector::getData(LSHandle *sh, LSMessage *msg, void *data)
{
    json_object *object = json_tokener_parse(LSMessageGetPayload(msg));
    if (!object)
    {
        Instance()->LSMessageReplyErrorBadJSON(sh, msg);
        return false;
    }

    std::string tmpCmd = "telegraf";
    pbnjson::JValue paramObj = stringToJValue(LSMessageGetPayload(msg));
    std::string tmpArguments = " --input-filter ";
    if (paramObj.objectSize() > 0)
    {
        pbnjson::JValue tmpArr = paramObj["inputs"];
        for (int i = 0; i < tmpArr.arraySize(); i++)
        {
            pbnjson::JValue value = tmpArr[i];
            tmpArguments += value.asString();
            if (i < tmpArr.arraySize() - 1)
            {
                tmpArguments += ":";
            }
        }
        tmpCmd += tmpArguments;
    }
    tmpCmd += " --test 2>&1";

    pbnjson::JValue reply = pbnjson::Object();
    std::string cmdResult = executeCommand(std::move(tmpCmd));
    if (cmdResult.find("E! [telegraf] Error") != std::string::npos)
    {
        Instance()->LSMessageReplyErrorInvalidConfigurations(sh, msg);
        return false;
    }
    pbnjson::JValue dataArray = pbnjson::Array();
    size_t prev = 0;
    size_t cur = 0;
    std::string divider = "> ";
    cmdResult = cmdResult.substr(cmdResult.find(divider, prev), cmdResult.length() - 1);
    while (true)
    {
        pbnjson::JValue resultOneObject = pbnjson::Object();
        cur = cmdResult.find(divider, prev);
        if (cur == std::string::npos)
        {
            cur = cmdResult.length();
        }
        std::string subStr = cmdResult.substr(prev, cur - prev);
        if (subStr.length() > 0)
        {
            int headerIndex = findIndex(subStr, " ", 0);
            int timeIndex = subStr.rfind(" ");

            std::string header = subStr.substr(0, headerIndex);
            std::string resultTitle = header.substr(0, header.find(","));
            pbnjson::JValue resultObj = convertDataToJson(header.substr(header.find(",") + 1, header.length() - 1));

            std::string tdata = subStr.substr(headerIndex + 1, timeIndex - (headerIndex + 1));
            pbnjson::JValue dataObj = convertDataToJson(std::move(tdata));
            resultObj.put("data", dataObj);

            std::string time = subStr.substr(timeIndex + 1, (subStr.length() - 1) - (timeIndex + 1));
            resultObj.put("time", pbnjson::JValue(time));

            resultOneObject.put(resultTitle, resultObj);
            dataArray.append(resultOneObject);
        }
        prev = cur + divider.length();
        if (cur == cmdResult.length())
        {
            break;
        }
    }

    reply.put("returnValue", true);
    reply.put("dataArray", dataArray);
    Instance()->LSMessageReplyPayload(sh, msg, reply.stringify().c_str());

    return true;
}

void LunaApiCollector::sendToTelegraf(std::string &msg)
{
    if (pThreadForSocket) {
        pThreadForSocket->sendToMSGQ(msg);
    }
}

// For LSSubscriptionReply
void LunaApiCollector::postEvent(void *subscribeKey, void *payload)
{
    LunaApiBaseCategory::postEvent(Instance()->pLSHandle, subscribeKey, payload);
}
