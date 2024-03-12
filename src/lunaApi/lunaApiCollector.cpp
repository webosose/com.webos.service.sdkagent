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
#include <algorithm>

LunaApiCollector *LunaApiCollector::_instance = NULL;

#define SDKAGENT_INIT_CONFIG "/var/lib/com.webos.service.sdkagent/initConfig.json"
#define TELEGRAF_CONFIG_DIR "/etc/telegraf/telegraf.d/"
#define TELEGRAF_MAIN_CONFIG "/etc/telegraf/telegraf.conf"

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

// available set configurations
// Protect the other configurations of telegraf
std::unordered_map<std::string, std::unordered_set<std::string>> availableConfiguration =
    {
        {"agent", {"interval", "flush_interval"}},
        {"outputs.influxdb", {"database", "urls"}},
        {"webOS.webProcessSize", {"enabled"}},
        {"webOS.processMonitoring", {"process_name", "enabled"}}};

std::string getSectionConfigPath(std::string sectionName)
{
    if (availableConfiguration.find(sectionName) != availableConfiguration.end())
    {
        return TELEGRAF_CONFIG_DIR + sectionName + ".conf";
    }
    else
        return "";
}

void LunaApiCollector::loadInitConfig()
{
    std::ifstream fp(SDKAGENT_INIT_CONFIG);
    if (!fp || fp.fail())
    {
        fp.close();
        return;
    }

    std::stringstream strBuffer;
    strBuffer << fp.rdbuf();
    auto initConfig = json_tokener_parse(strBuffer.str().c_str());
    if (!initConfig)
    {
        // need SDK_LOG
        return;
    }

    availableConfiguration.clear();
    json_object *availConfig = NULL;
    if (json_object_object_get_ex(initConfig, "availableConfiguration", &availConfig))
    {
        json_object_object_foreach(availConfig, section, configs)
        {
            availableConfiguration[section] = {};
            for (size_t i = 0; i < json_object_array_length(configs); i++)
            {
                json_object *val = json_object_array_get_idx(configs, i);
                availableConfiguration[section].insert(json_object_to_json_string(val));
            }
        }
    }
}

LunaApiCollector::LunaApiCollector()
{
    pCategory = "/collector";
    pMethods = (LSMethod *)collectorMethods;

    loadInitConfig();

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

    Instance()->executeCommand("systemctl start telegraf");
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

    Instance()->executeCommand("systemctl stop telegraf");
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

    Instance()->executeCommand("systemctl restart telegraf");
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
    if (isEnable)
    {
        // Create configuration file for startOnBoot
        Instance()->executeCommand("touch /var/lib/com.webos.service.sdkagent/startOnBoot");
    }
    else
    {
        // Remove configuration file for startOnBoot
        Instance()->executeCommand("rm /var/lib/com.webos.service.sdkagent/startOnBoot");
    }
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
    reply.put("status", Instance()->executeCommand("systemctl is-active telegraf"));
    std::ifstream startOnBoot("/var/lib/com.webos.service.sdkagent/startOnBoot");
    reply.put("startOnBoot", startOnBoot.good());
    startOnBoot.close();

    Instance()->LSMessageReplyPayload(sh, msg, (char *)(reply.stringify().c_str()));

    return true;
}

// if need process filtering except intput process name
bool checkProcessName(std::string processName)
{
    std::string pid = LunaApiCollector::Instance()->executeCommand("pgrep -f " + processName, true);
    return !pid.empty();
}

// write to /etc/telegrad/telegrad.d/procstat.conf
void updateProcstatConfig(pbnjson::JValue tmpConfigJson)
{
    if (tmpConfigJson.hasKey("webOS.processMonitoring") &&
        (tmpConfigJson["webOS.processMonitoring"]).hasKey("enabled") &&
        ((tmpConfigJson["webOS.processMonitoring"])["enabled"]).asBool())
    {
        std::string processList = "";
        pbnjson::JValue process_name = (tmpConfigJson["webOS.processMonitoring"])["process_name"];
        if (process_name.isArray())
        {
            for (int i = 0; i < process_name.arraySize(); i++)
            {
                if (checkProcessName(process_name[i].asString()))
                {
                    processList += process_name[i].asString() + "|";
                }
            }
            if ((!processList.empty()) && (processList.at(processList.length() - 1) == '|'))
            {
                processList.pop_back();
            }

            if (processList.length() > 0)
            {
                std::string procstatCmd = "echo -e '[[inputs.procstat]]\npid_tag=true\npattern=\"" + processList + "\"' > /etc/telegraf/telegraf.d/procstat.conf";
                LunaApiCollector::Instance()->executeCommand(std::move(procstatCmd));
                return;
            }
        }
    }
    LunaApiCollector::Instance()->executeCommand("rm -fr /etc/telegraf/telegraf.d/procstat.conf");
}

bool updateSectionConfig(const std::string &section, tomlObject &inputConfig)
{
    if (section.compare(0, 6, "webOS.") == 0)
        return true;

    std::string configPath = getSectionConfigPath(section);
    if (configPath.empty())
        return true;

    tomlObject currentConfig = readTomlFile(configPath.c_str());

    // remove all the current configuration, leave it blank
    if (inputConfig[section].empty())
    {
        writeTomlSection(configPath, section, inputConfig[section]);
    }
    else
    {
        for (auto &cfg : inputConfig[section])
        {
            currentConfig[section][cfg.first] = cfg.second;
        }
        writeTomlSection(configPath, section, currentConfig[section]);
    }
    return true;
}

void updateWebOSConfig(tomlObject &inputConfig)
{
    pbnjson::JValue webOSConfigJson = LunaApiCollector::Instance()->readwebOSConfigJson();

    for (auto &section : inputConfig)
    {
        if (section.first.compare(0, 6, "webOS.") == 0)
        {
            webOSConfigJson.put(section.first, pbnjson::Object());
            for (auto &configParam : section.second)
            {
                webOSConfigJson[section.first].put(configParam.first, stringToJValue(configParam.second.c_str()));
            }
        }
    }
    updateProcstatConfig(webOSConfigJson);

    LunaApiCollector::Instance()->writewebOSConfigJson(std::move(webOSConfigJson));
}

bool updateConfig(tomlObject &inputConfig)
{
    for (auto &it : inputConfig)
    {
        updateSectionConfig(it.first, inputConfig);
    }
    updateWebOSConfig(inputConfig);

    return true;
}

bool checkInputConfig(tomlObject &inputConfig)
{
    if (inputConfig.empty())
        return false;
    for (auto &section : inputConfig)
    {
        auto &sectionName = section.first;
        if (availableConfiguration.find(sectionName) == availableConfiguration.end())
            return false;
        for (auto &config : section.second)
        {
            auto &configName = config.first;
            if (availableConfiguration[sectionName].find(configName) == availableConfiguration[sectionName].end())
                return false;
        }
    }

    return true;
}

bool telegrafIsActive()
{
    std::string stateResult = LunaApiCollector::Instance()->executeCommand("systemctl status telegraf -l | grep \"Active: active (running)\"");
    return (!stateResult.empty());
}

bool LunaApiCollector::setConfig(LSHandle *sh, LSMessage *msg, void *data)
{
    if (telegrafIsActive())
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

    if (!checkInputConfig(inputConfig))
    {
        Instance()->LSMessageReplyErrorInvalidParams(sh, msg);
        return false;
    }

    if (!updateConfig(inputConfig))
    {
        Instance()->LSMessageReplyErrorInvalidConfigurations(sh, msg);
        return false;
    }

    Instance()->LSMessageReplyPayload(sh, msg, (char *)"{\"returnValue\": true}");
    return true;
}

std::tuple<bool, tomlObject> getTelegrafConfig()
{
    tomlObject telegrafConfig;
    std::ifstream openFile(TELEGRAF_MAIN_CONFIG);
    if (!openFile || (openFile.fail()))
    {
        openFile.close();
        return {false, telegrafConfig};
    }

    telegrafConfig = readTomlFile(TELEGRAF_MAIN_CONFIG);
    for (auto &it : availableConfiguration)
    {
        auto &sectionName = it.first;

        // webOS config will be updated later from webOS config file
        if (sectionName.compare(0, 6, "webOS.") == 0)
            continue;

        // override subConfig to mainConfig
        auto subConfig = readTomlFile(getSectionConfigPath(sectionName));
        telegrafConfig[sectionName] = subConfig[sectionName];
    }

    return {true, telegrafConfig};
}

bool LunaApiCollector::getConfig(LSHandle *sh, LSMessage *msg, void *data)
{
    if (!json_tokener_parse(LSMessageGetPayload(msg)))
    {
        Instance()->LSMessageReplyErrorBadJSON(sh, msg);
        return false;
    }

    bool ret = true;
    tomlObject telegrafConfig;
    std::tie(ret, telegrafConfig) = getTelegrafConfig();

    if (!ret)
    {
        Instance()->LSMessageReplyPayload(sh, msg, (char *)"{\"returnValue\":false,\"errorCode\":4,\"errorText\":\"Invalid configurations.\"}");
        return true;
    }

    bool validJsonString = true;
    tomlObject webOSConfig;
    std::tie(validJsonString, webOSConfig) = jsonStringToTomlObject(LunaApiCollector::Instance()->readwebOSConfigJson().stringify().c_str());
    for (auto &it : webOSConfig)
    {
        telegrafConfig[it.first] = it.second;
    }

    std::string replyConfig = tomlObjectToJsonString(telegrafConfig, std::string("    "));
    replyConfig = "{\n    \"returnValue\": true,\n    \"config\": " + replyConfig + "\n}";

    Instance()->LSMessageReplyPayload(sh, msg, (char *)replyConfig.c_str());
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

pbnjson::JValue convertDataToJson(std::string data)
{
    pbnjson::JValue reply = pbnjson::Object();
    size_t prev = 0;
    size_t cur = 0;
    std::string divider = ",";
    while (true)
    {
        cur = findIndex(data, divider, prev);
        std::string subStr = data.substr(prev, cur - prev);
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
        if (cur == data.length())
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
    pbnjson::JValue paramObj = Instance()->convertStringToJson(LSMessageGetPayload(msg));
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
    tmpCmd += " -test 2>&1";

    pbnjson::JValue reply = pbnjson::Object();
    std::string cmdResult = Instance()->executeCommand(std::move(tmpCmd));
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

            // CID 9603867: CERT-CPP Expressions (CERT EXP60-CPP)       --> check to remove after static analysis
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
    Instance()->LSMessageReplyPayload(sh, msg, (char *)(reply.stringify().c_str()));

    return true;
}

void splitMainConfig()
{
    auto mainConfig = readTomlFile(TELEGRAF_MAIN_CONFIG);
    std::vector<std::string> toBeRemovedSections;

    for (auto &it : mainConfig)
    {
        std::string section = it.first;
        SDK_LOG_INFO(MSGID_SDKAGENT, 1, PMLOGKS("SECTION", section.c_str()), "com.webos.service.sdkagent loop through %s", __FUNCTION__);
        
        if (section.compare(0, 6, "webOS.") == 0)
            continue;

        if (availableConfiguration.find(section) != availableConfiguration.end())
        {
            auto configPath = getSectionConfigPath(section);
            auto subConfig = readTomlFile(configPath);
            for (const auto & configParam : it.second)
            {
                // ignore if subfile has this configParam
                if (subConfig[section].find(configParam.first) != subConfig[section].end()) continue;

                subConfig[section][configParam.first] = configParam.second;
            }
            SDK_LOG_INFO(MSGID_SDKAGENT, 0, "%s( ... ) , com.webos.service.sdkagent write to %s", __FUNCTION__, configPath.c_str());
            writeTomlSection(configPath, section, subConfig[section]);
            toBeRemovedSections.push_back(section);
        }
    }

    for (const auto & section : toBeRemovedSections)
    {
        mainConfig.erase(section);
    }
    SDK_LOG_INFO(MSGID_SDKAGENT, 0, "%s( ... ) , rewrite main config %s", __FUNCTION__, TELEGRAF_MAIN_CONFIG);
    writeTomlFile(TELEGRAF_MAIN_CONFIG, mainConfig);
}

/**
 * When the service initialize
 */
void LunaApiCollector::initialize()
{
    SDK_LOG_INFO(MSGID_SDKAGENT, 0, "%s( ... ) , com.webos.service.sdkagent LunaApiCollector.initialize()", __FUNCTION__);

    // for create the configuration directory for sdkagent service
    Instance()->executeCommand("mkdir -p /var/lib/com.webos.service.sdkagent");
    // Check the flag using startOnBoot file for auto-start on boot

    // if [agent] or [outputs.influxdb] still remain in telegraf.conf -> try to move sub config file
    SDK_LOG_INFO(MSGID_SDKAGENT, 0, "%s( ... ) , com.webos.service.sdkagent call splitMainConfig()", __FUNCTION__);
    splitMainConfig();

    std::ifstream startOnBoot("/var/lib/com.webos.service.sdkagent/startOnBoot");
    SDK_LOG_INFO(MSGID_SDKAGENT, 1, PMLOGKFV("startOnBoot exists", "%d", startOnBoot.good()), "com.webos.service.sdkagent check startOnBoot %s", __FUNCTION__);
    if (startOnBoot.good())
    {
        SDK_LOG_INFO(MSGID_SDKAGENT, 0, "%s( ... ) , com.webos.service.sdkagent call systemctl start telegraf", __FUNCTION__);
        Instance()->executeCommand("systemctl start telegraf");
    }
    startOnBoot.close();
}

// For LSSubscriptionReply
void LunaApiCollector::postEvent(void *subscribeKey, void *payload)
{
    LunaApiBaseCategory::postEvent(Instance()->pLSHandle, subscribeKey, payload);
}

/**
 * Send data to socket thread using msg queue
 */
void LunaApiCollector::sendToTelegraf(std::string &data)
{
    if (pThreadForSocket != NULL)
        pThreadForSocket->sendToMSGQ(data);
}
