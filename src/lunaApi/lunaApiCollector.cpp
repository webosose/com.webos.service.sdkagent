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

#include <algorithm>
#include <fstream>
#include <iostream>

#include "logging.h"
#include "lunaApiCollector.h"

lunaApiCollector* lunaApiCollector::pInstance = NULL;

const LSMethod lunaApiCollector::collectorMethods[] = {
    { "start",                      start,                      LUNA_METHOD_FLAGS_NONE},
    { "stop",                       stop,                       LUNA_METHOD_FLAGS_NONE},
    { "restart",                    restart,                    LUNA_METHOD_FLAGS_NONE},
    { "startOnBoot",                startOnBoot,                LUNA_METHOD_FLAGS_NONE},
    { "getStatus",                  getStatus,                  LUNA_METHOD_FLAGS_NONE},

    { "getConfig",                  getConfig,                  LUNA_METHOD_FLAGS_NONE},
    { "setConfig",                  setConfig,                  LUNA_METHOD_FLAGS_NONE},
    
    { "getData",                    getData,                    LUNA_METHOD_FLAGS_NONE},
    { NULL, NULL },
};

std::string availableConfiguration = "{\
    \"agent\": [\
        \"interval\",\
        \"flush_interval\"\
    ],\
    \"outputs.influxdb\": [\
        \"database\",\
        \"urls\"\
    ]\
}";

lunaApiCollector::lunaApiCollector() {
    pCategory = "/collector";
    pMethods = (LSMethod *) collectorMethods;
    
    availableConfigurationJson = json_tokener_parse(availableConfiguration.c_str());
}

lunaApiCollector::~lunaApiCollector() {
}

std::string& ltrim(std::string &s) {
    auto it = std::find_if(s.begin(), s.end(),
                    [](char c) {
                        return !std::isspace<char>(c, std::locale::classic());
                    });
    s.erase(s.begin(), it);
    return s;
}
std::string& rtrim(std::string &s) {
    auto it = std::find_if(s.rbegin(), s.rend(),
                    [](char c) {
                        return !std::isspace<char>(c, std::locale::classic());
                    });
    s.erase(it.base(), s.end());
    return s;
}
std::string& trim(std::string &s) {
    return ltrim(rtrim(s));
}

// API implementations START
bool lunaApiCollector::start(LSHandle *sh, LSMessage *msg, void *data) {
    json_object *object = json_tokener_parse(LSMessageGetPayload(msg));
    if (!object) {
        Instance()->LSMessageReplyErrorBadJSON(sh, msg);
        return false;
    }

    Instance()->executeCommand("systemctl start telegraf");
    Instance()->LSMessageReplyPayload(sh, msg, NULL);

    return true;
}

bool lunaApiCollector::stop(LSHandle *sh, LSMessage *msg, void *data) {
    json_object *object = json_tokener_parse(LSMessageGetPayload(msg));
    if (!object) {
        Instance()->LSMessageReplyErrorBadJSON(sh, msg);
        return false;
    }

    Instance()->executeCommand("systemctl stop telegraf");
    Instance()->LSMessageReplyPayload(sh, msg, NULL);

    return true;
}

bool lunaApiCollector::restart(LSHandle *sh, LSMessage *msg, void *data) {
    json_object *object = json_tokener_parse(LSMessageGetPayload(msg));
    if (!object) {
        Instance()->LSMessageReplyErrorBadJSON(sh, msg);
        return false;
    }

    Instance()->executeCommand("systemctl restart telegraf");
    Instance()->LSMessageReplyPayload(sh, msg, NULL);

    return true;
}

bool lunaApiCollector::cbStartOnBoot(LSHandle *sh, LSMessage *msg, void *user_data)
{
    LSError lserror;
    LSErrorInit(&lserror);

    pbnjson::JValue response = Instance()->convertStringToJson(LSMessageGetPayload(msg));
    if (!response["returnValue"].asBool()) {
        SDK_LOG_INFO(MSGID_SDKAGENT, 0, "[ %s : %d ] %s( ... )", __FILE__, __LINE__, __FUNCTION__);
        Instance()->LSMessageReplyErrorUnknown(sh, (LSMessage *)user_data);
        return false;
    }

    Instance()->LSMessageReplyPayload(sh, (LSMessage *)user_data, NULL);

    return true;
}

bool lunaApiCollector::startOnBoot(LSHandle *sh, LSMessage *msg, void *data) {
    json_object *object = json_tokener_parse(LSMessageGetPayload(msg));
    if (!object) {
        Instance()->LSMessageReplyErrorBadJSON(sh, msg);
        return false;
    }

    pbnjson::JValue paramObj = Instance()->convertStringToJson(LSMessageGetPayload(msg));
    if (!paramObj.hasKey("enable") || !paramObj["enable"].isBoolean()) {
        Instance()->LSMessageReplyErrorInvalidParams(sh, msg);
        return false;
    }

    pbnjson::JValue param = pbnjson::Object();
    param.put("appId", "com.webos.service.sdkagent");
    param.put("key", "startOnBoot");
    pbnjson::JValue value = pbnjson::Object();
    value.put("enabled", paramObj["enable"].asBool());
    param.put("value", value);

    LSError lserror;
    LSErrorInit(&lserror);
    LSMessageRef(msg);
    if (!LSCall(Instance()->pLSHandle,
                "luna://com.webos.service.preferences/appProperties/setAppProperty",
                param.stringify().c_str(),
                lunaApiCollector::cbStartOnBoot,
                (void *)msg,
                NULL,
                &lserror))
    {
        LSErrorPrint(&lserror, stderr);
        LSErrorFree(&lserror);
        SDK_LOG_INFO(MSGID_SDKAGENT, 0, "[ %s : %d ] %s( ... )", __FILE__, __LINE__, __FUNCTION__);
        Instance()->LSMessageReplyErrorUnknown(sh, msg);
        return false;
    }

    return true;
}

bool lunaApiCollector::cbGetStatus(LSHandle *sh, LSMessage *msg, void *user_data)
{
    LSError lserror;
    LSErrorInit(&lserror);

    pbnjson::JValue response = Instance()->convertStringToJson(LSMessageGetPayload(msg));
    bool isStartOnBoot = (response["startOnBoot"])["enabled"].asBool();

    pbnjson::JValue reply = pbnjson::Object();
    reply.put("returnValue", true);
    reply.put("status", Instance()->executeCommand("systemctl is-active telegraf"));
    reply.put("startOnBoot", isStartOnBoot);

    Instance()->LSMessageReplyPayload(sh, (LSMessage *)user_data, (char *)(reply.stringify().c_str()));

    return true;
}

bool lunaApiCollector::getStatus(LSHandle *sh, LSMessage *msg, void *data) {
    json_object *object = json_tokener_parse(LSMessageGetPayload(msg));
    if (!object) {
        Instance()->LSMessageReplyErrorBadJSON(sh, msg);
        return false;
    }

    LSError lserror;
    LSErrorInit(&lserror);
    LSMessageRef(msg);
    if (!LSCall(Instance()->pLSHandle,
                "luna://com.webos.service.preferences/appProperties/getAppProperty",
                "{\"appId\":\"com.webos.service.sdkagent\", \"key\":\"startOnBoot\"}",
                lunaApiCollector::cbGetStatus,
                (void *)msg,
                NULL,
                &lserror))
    {
        LSErrorPrint(&lserror, stderr);
        LSErrorFree(&lserror);
        Instance()->LSMessageReplyErrorUnknown(sh, msg);
        return false;
    }

    return true;
}

bool lunaApiCollector::setConfig(LSHandle *sh, LSMessage *msg, void *data) {
    json_object *object = json_tokener_parse(LSMessageGetPayload(msg));
    if (!object) {
        Instance()->LSMessageReplyErrorBadJSON(sh, msg);
        return false;
    }

    pbnjson::JValue paramObj = Instance()->convertStringToJson(LSMessageGetPayload(msg));
    
    // check parameter is available
    if (paramObj.objectSize() > 0) {
        json_object *rootObj = json_tokener_parse(paramObj.stringify().c_str());
        json_object_object_foreach(rootObj, section, availableKeys) {
            json_object *keyObj;
            if (json_object_object_get_ex(Instance()->availableConfigurationJson, section, &keyObj)) {
                json_object *childObj = json_tokener_parse(paramObj[section].stringify().c_str());
                json_object_object_foreach(childObj, childKey, childVal) {
                    json_object *availableKeys = json_object_object_get(Instance()->availableConfigurationJson, section);
                    bool availFlag = false;
                    for (int i = 0; i < json_object_array_length(availableKeys); i++) {
                        json_object *dval = json_object_array_get_idx(availableKeys, i);
                        if (strcasecmp(childKey, json_object_get_string(dval)) == 0) {
                            availFlag = true;
                            break;
                        }
                    }
                    if (!availFlag) {
                        Instance()->LSMessageReplyErrorInvalidParams(sh, msg);
                        return false;
                        // paramObj[section].remove(childKey);
                    }
                }
            } else {
                Instance()->LSMessageReplyErrorInvalidParams(sh, msg);
                return false;
                // paramObj.remove(section);
            }
        }
    } else {
        Instance()->LSMessageReplyErrorInvalidParams(sh, msg);
        return false;
    }
    
    pbnjson::JValue reply = pbnjson::Object();

    // read/write file open
    std::string tmpTelegrafConfPath = "/etc/telegraf/tmpTelegraf.conf";
    std::ofstream writeFile(tmpTelegrafConfPath);
    std::string telegrafConfPath = "/etc/telegraf/telegraf.conf";
    std::ifstream openFile(telegrafConfPath);
    if (!openFile || (openFile.fail())) {
	    openFile.close();
        writeFile.close();
        reply.put("returnValue", false);
        reply.put("errorCode", 5);
        reply.put("errorText", "Invalid configuration file : " + std::string(strerror(errno)));
        Instance()->LSMessageReplyPayload(sh, msg, (char *)(reply.stringify().c_str()));
        return false;
    }

    std::string line;
    std::string section;
    while (getline(openFile, line)) {
        if (line.empty()) {
            writeFile.write("\n", 1);
            continue;
        }
        for (int i=0; i<line.length(); i++) {
            if (line.at(i) == ' ') {            // empty line
                continue;
            } else if (line.at(i) == '#') {     // comment line
                writeFile.write(line.c_str(), line.size());
                writeFile.write("\n", 1);
                break;
            } else if (line.at(i) == '[') {     // section line
                writeFile.write(line.c_str(), line.size());
                writeFile.write("\n", 1);

                if (paramObj.hasKey(section)) {
                    // If parameter have section, remove old section data from parameter after write
                    paramObj.remove(section);
                }
                section = line;
                section.erase(std::remove(section.begin(), section.end(), '['), section.end());
                section.erase(std::remove(section.begin(), section.end(), ']'), section.end());
                if (paramObj.hasKey(section)) {
                    // If parameter have section, add new key,value datas
                    pbnjson::JValue tmpObj = paramObj[section];
                    json_object *jobj = json_tokener_parse(tmpObj.stringify().c_str());
                    json_object_object_foreach(jobj, key, val) {
                        std::string newValue = "  " + std::string(key) + " = " + tmpObj[key].stringify();
                        writeFile.write(newValue.c_str(), newValue.size());
                        writeFile.write("\n", 1);
                    }
                } else {
                    // parameter does not have section
                    section = "";
                }
                break;
            } else {
                if (section.empty()) {
                    // parameter does not have section, write original data
                    writeFile.write(line.c_str(), line.size());
                } else {
                    pbnjson::JValue tmpObj = paramObj[section];
                    std::string key = line.substr(0, line.find('='));
                    trim(key);
                    if (!tmpObj.hasKey(key)) {
                        // parameter does not have key, write original data
                        writeFile.write(line.c_str(), line.size());
                    }
                }
                writeFile.write("\n", 1);
                break;
            }
        }
    }
    if (paramObj.objectSize() > 0) {    // write new section and new datas
        json_object *rootObj = json_tokener_parse(paramObj.stringify().c_str());
        json_object_object_foreach(rootObj, section, agentVal) {
            std::string newSection = "[" + std::string(section) + "]";
            if ((std::string(section) != "global_tags") && (std::string(section) != "agent")) {
                newSection = "[" + newSection + "]";
            }
            writeFile.write(newSection.c_str(), newSection.size());
            writeFile.write("\n", 1);
            pbnjson::JValue tmpObj = paramObj[section];
            json_object *jobj = json_tokener_parse(tmpObj.stringify().c_str());
            json_object_object_foreach(jobj, key, val) {
                std::string newValue = "  " + std::string(key) + " = " + tmpObj[key].stringify();
                writeFile.write(newValue.c_str(), newValue.size());
                writeFile.write("\n", 1);
            }
        }
    }
	openFile.close();
    writeFile.close();
    
    // test new telegraf.conf file
    std::string testCmd = "telegraf --config " + tmpTelegrafConfPath + " -test 2>&1";
    std::string cmdResult = Instance()->executeCommand(testCmd);

    std::string tmpCmd;
    if (cmdResult.find("E! [telegraf] Error") != std::string::npos) {
        // Not change conf file
        // tmpCmd = "rm " + tmpTelegrafConfPath;
        reply.put("returnValue", false);
        reply.put("errorCode", 4);
        reply.put("errorText", "Invalid configurations");
    } else {
        // Change conf file
        tmpCmd = "mv " + tmpTelegrafConfPath + " " + telegrafConfPath;
        reply.put("returnValue", true);
    }
    Instance()->executeCommand(tmpCmd);

    Instance()->LSMessageReplyPayload(sh, msg, (char *)(reply.stringify().c_str()));

    return true;
}

bool lunaApiCollector::getConfig(LSHandle *sh, LSMessage *msg, void *data) {
    json_object *object = json_tokener_parse(LSMessageGetPayload(msg));
    if (!object) {
        Instance()->LSMessageReplyErrorBadJSON(sh, msg);
        return false;
    }

    pbnjson::JValue reply = pbnjson::Object();
    
    std::ifstream openFile("/etc/telegraf/telegraf.conf");
    if (!openFile || (openFile.fail())) {
        openFile.close();
        reply.put("returnValue", false);
        reply.put("errorCode", 5);
        reply.put("errorText", "Invalid configuration file : " + std::string(strerror(errno)));
        Instance()->LSMessageReplyPayload(sh, msg, (char *)(reply.stringify().c_str()));
        return false;
    }

    pbnjson::JValue replyConfig = pbnjson::Object();
    std::string line;
    std::string section;
    while (getline(openFile, line)) {
        for (int i=0; i<line.length(); i++) {
            if (line.at(i) == ' ') {
                continue;
            } else if (line.at(i) == '#') {
                break;
            } else if (line.at(i) == '[') {
                section = line;
                section.erase(std::remove(section.begin(), section.end(), '['), section.end());
                section.erase(std::remove(section.begin(), section.end(), ']'), section.end());
                trim(section);
                replyConfig.put(section, pbnjson::Object());
                break;
            } else {
                if (line.find("=") == std::string::npos) {
                    // SDK_LOG_INFO(MSGID_SDKAGENT, 0, "npos line : %s", line.c_str());
                    break;
                }

                // TODO : Multi line configuration not support parsing
                int doubleQuotationCount = 0;
                for (char& c : line) {
                    if (c == '"') {
                        doubleQuotationCount++;
                    }
                    if ((doubleQuotationCount%2 == 0) && (c == '=')) {
                        c = ':';
                    }
                }
                int firstColonIndex = line.find(":");
                std::string key = line.substr(0, firstColonIndex);
                std::string value = line.substr(firstColonIndex+1, line.length());
                pbnjson::JValue tmpObj = Instance()->convertStringToJson(std::string("{\""+ trim(key) +"\":"+ trim(value) +"}").c_str());
                replyConfig[section].put(key, tmpObj[key]);
                break;
            }
        }
    }
    openFile.close();

    reply.put("returnValue", true);
    reply.put("config", replyConfig);
    Instance()->LSMessageReplyPayload(sh, msg, (char *)(reply.stringify().c_str()));

    return true;
}

pbnjson::JValue convertDataToJson(std::string data) {
    pbnjson::JValue reply = pbnjson::Object();
    int prev = 0;
    int cur = 0;
    std::string divider = ",";
    while (true) {
        cur = data.find(divider, prev);
        if (cur == std::string::npos) {
            cur = data.length();
        }
        std::string subStr = data.substr(prev, cur - prev);
        int doubleQuotationCount = 0;
        int divideIndex = 0;
        for (divideIndex=0; divideIndex<subStr.length(); divideIndex++) {
            char& c = subStr.at(divideIndex);
            if (c == '"') {
                doubleQuotationCount++;
            }
            if ((doubleQuotationCount%2 == 0) && (c == '=')) {
                c = ':';
                break;
            }
        }
        reply.put(subStr.substr(0, divideIndex), subStr.substr(divideIndex+1, subStr.length()-1));
        prev = cur + divider.length();
        if (cur == data.length()) {
            break;
        }
    }
    return reply;
}

bool lunaApiCollector::getData(LSHandle *sh, LSMessage *msg, void *data) {
    json_object *object = json_tokener_parse(LSMessageGetPayload(msg));
    if (!object) {
        Instance()->LSMessageReplyErrorBadJSON(sh, msg);
        return false;
    }

    std::string tmpCmd = "telegraf";
    pbnjson::JValue paramObj = Instance()->convertStringToJson(LSMessageGetPayload(msg));
    std::string tmpArguments = " --input-filter ";
    if (paramObj.objectSize() > 0) {
        pbnjson::JValue tmpArr = paramObj["inputs"];
        for (int i=0; i<tmpArr.arraySize(); i++) {
            pbnjson::JValue value = tmpArr[i];
            tmpArguments += value.asString();
            if (i < tmpArr.arraySize()-1) {
                tmpArguments += ":";
            }
        }
        tmpCmd += tmpArguments;
    }
    tmpCmd += " -test 2>&1";

    pbnjson::JValue reply = pbnjson::Object();
    std::string cmdResult = Instance()->executeCommand(tmpCmd);
    if (cmdResult.find("E! [telegraf] Error") != std::string::npos) {
        reply.put("returnValue", false);
        reply.put("errorCode", 4);
        reply.put("errorText", "Invalid configurations");
        Instance()->LSMessageReplyPayload(sh, msg, (char *)(reply.stringify().c_str()));
        return false;
    }
    pbnjson::JValue dataArray = pbnjson::Array();
    int prev = 0;
    int cur = 0;
    std::string divider = "> ";
    cmdResult = cmdResult.substr(cmdResult.find(divider, prev), cmdResult.length()-1);
    while (true) {
        pbnjson::JValue resultOneObject = pbnjson::Object();
        cur = cmdResult.find(divider, prev);
        if (cur == std::string::npos) {
            cur = cmdResult.length();
        }
        std::string subStr = cmdResult.substr(prev, cur - prev);
        if (subStr.length() > 0) {
            pbnjson::JValue resultObj = pbnjson::Object();
            int headerIndex = subStr.find(" ");
            std::string header = subStr.substr(0, headerIndex);
            std::string resultTitle = header.substr(0, header.find(","));
            resultObj = convertDataToJson(header.substr(header.find(",")+1, header.length()-1));
            
            int timeIndex = subStr.rfind(" ");
            pbnjson::JValue dataObj = convertDataToJson(subStr.substr(headerIndex+1, timeIndex-(headerIndex+1)));
            resultObj.put("data", dataObj);
            std::string time = subStr.substr(timeIndex+1, (subStr.length()-1)-(timeIndex+1));
            resultObj.put("time", time);
            
            resultOneObject.put(resultTitle, resultObj);
            dataArray.append(resultOneObject);
        }
        prev = cur + divider.length();
        if (cur == cmdResult.length()) {
            break;
        }
    }

    reply.put("returnValue", true);
    reply.put("dataArray", dataArray);
    Instance()->LSMessageReplyPayload(sh, msg, (char *)(reply.stringify().c_str()));

    return true;
}

bool lunaApiCollector::cbInitStartOnBoot(LSHandle *sh, LSMessage *msg, void *user_data)
{
    LSError lserror;
    LSErrorInit(&lserror);

    pbnjson::JValue response = Instance()->convertStringToJson(LSMessageGetPayload(msg));
    if (response["returnValue"].asBool()) {
        if ((response["startOnBoot"])["enabled"].asBool()) {
            Instance()->executeCommand("systemctl start telegraf");
        }
    } else {
        Instance()->initialize();
    }

    return true;
}

void lunaApiCollector::initialize() {
    LSError lserror;
    LSErrorInit(&lserror);
    if (!LSCall(Instance()->pLSHandle,
                "luna://com.webos.service.preferences/appProperties/getAppProperty",
                "{\"appId\":\"com.webos.service.sdkagent\", \"key\":\"startOnBoot\"}",
                lunaApiCollector::cbInitStartOnBoot,
                NULL,
                NULL,
                &lserror))
    {
        LSErrorPrint(&lserror, stderr);
        LSErrorFree(&lserror);
    }
}
// API implementations END

void lunaApiCollector::postEvent(void *subscribeKey, void *payload) {
    lunaApiBaseCategory::postEvent(Instance()->pLSHandle, subscribeKey, payload);
}
