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

#include "telegrafController.h"
#include "common.h"
#include "logging.h"
#include "tomlParser.h"
#include <spawn.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <fstream>
#include <pbnjson.hpp>
#include <json-c/json.h>
#include <sys/resource.h>

#define TELEGRAF_BIN "/usr/bin/telegraf"
#define SDKAGENT_DIR "/var/lib/com.webos.service.sdkagent/"
#define TELEGRAF_AVAILABLE_CONFIG "/var/lib/com.webos.service.sdkagent/availableConfigurations.json"

#define TELEGRAF_DIR "/var/lib/com.webos.service.sdkagent/telegraf/"
#define TELEGRAF_CONFIG_DIR "/var/lib/com.webos.service.sdkagent/telegraf/telegraf.d/"
#define TELEGRAF_MAIN_CONFIG "/var/lib/com.webos.service.sdkagent/telegraf/telegraf.conf"
#define TELEGRAF_CONSOLE_LOG "/var/lib/com.webos.service.sdkagent/telegraf/telegraf.log"

// available set configurations
// Protect the other configurations of telegraf
std::unordered_map<std::string, std::unordered_set<std::string>> availableConfiguration =
{
    {"agent", {"interval", "flush_interval"}},
    {"outputs.influxdb", {"database", "urls"}},
    {"webOS.webProcessSize", {"enabled"}},
    {"webOS.processMonitoring", {"process_name", "enabled"}}
};

std::string getSectionConfigPath(std::string sectionName)
{
    if (availableConfiguration.find(sectionName) != availableConfiguration.end()) {
        return std::string(TELEGRAF_CONFIG_DIR) + sectionName + ".conf";
    }
    return "";
}

TelegrafController::TelegrafController()
{
    splitMainConfig();
    initAvailableConfigurations();
}

TelegrafController::~TelegrafController()
{
    if (pid_ <= 0) stop();
}

static std::tuple<bool, tomlObject> getTelegrafConfig()
{
    tomlObject telegrafConfig;
    if (!fileExists(TELEGRAF_MAIN_CONFIG)) {
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
        for (auto & configParams : subConfig[sectionName]) {
            telegrafConfig[sectionName][configParams.first] = configParams.second;
        }
    }

    return {true, telegrafConfig};
}

void TelegrafController::initAvailableConfigurations()
{
    SDK_LOG_INFO(MSGID_SDKAGENT, 0, "%s is called", __FUNCTION__);
    std::string strBuffer = readTextFile(TELEGRAF_AVAILABLE_CONFIG);
    if (strBuffer.empty() || strBuffer == "") return;

    auto initConfig = json_tokener_parse(strBuffer.c_str());
    if (!initConfig)
    {
        SDK_LOG_INFO(MSGID_SDKAGENT, 0, "%s does not exist", TELEGRAF_AVAILABLE_CONFIG);
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

void TelegrafController::loadConfig()
{
    bool ret = true;
    std::tie(ret, _allConfig) = getTelegrafConfig();

    // load webOS Config
    if (ret) {

        pbnjson::JValue webOSConfigJson = readWebOSJsonConfig();

        if (
            webOSConfigJson.hasKey("webOS.webProcessSize") &&
            webOSConfigJson["webOS.webProcessSize"].hasKey("enabled")
        ) {
            _allConfig["webOS.webProcessSize"]["enabled"] = "false";
            if (webOSConfigJson["webOS.webProcessSize"]["enabled"].asBool()) {
                _allConfig["webOS.webProcessSize"]["enabled"] = "true";
            }
        }

        if (
            webOSConfigJson.hasKey("webOS.processMonitoring") && 
            webOSConfigJson["webOS.processMonitoring"].hasKey("enabled")
        ) {
            _allConfig["webOS.processMonitoring"]["enabled"] = "false";
            if (webOSConfigJson["webOS.processMonitoring"]["enabled"].asBool()) {
                _allConfig["webOS.processMonitoring"]["enabled"] = "true";
                std::string processList = "[";
                pbnjson::JValue process_name = (webOSConfigJson["webOS.processMonitoring"])["process_name"];
                if (process_name.isArray())
                {
                    for (int i = 0; i < process_name.arraySize(); i++) {
                        processList += '\"' + process_name[i].asString() + "\", ";
                    }
                    if ((!processList.empty()) && (processList.at(processList.length() - 2) == ',')) {
                        processList.pop_back();
                        processList.pop_back();
                    }
                }
                processList += ']';
                _allConfig["webOS.processMonitoring"]["process_name"] = std::move(processList);
            }
        }
    }
}

void TelegrafController::splitMainConfig()
{
    SDK_LOG_INFO(MSGID_SDKAGENT, 0, "%s is called", __FUNCTION__);
    auto mainConfig = readTomlFile(TELEGRAF_MAIN_CONFIG);

    for (auto &it : mainConfig)
    {
        std::string section = it.first;
        if (availableConfiguration.find(section) != availableConfiguration.end())
        {
            auto configPath = getSectionConfigPath(section);
            if (fileExists(configPath.c_str())) continue;

            tomlObject plugin;
            for (const auto & configParam : it.second) {
                plugin[section][configParam.first] = configParam.second;
            }
            writeTomlSection(configPath, section, plugin[section]);
        }
    }

    std::unordered_set<std::string> plugins;
    for (const auto & kv : availableConfiguration) {
        plugins.insert(kv.first);
    }
    disableTomlSection(TELEGRAF_MAIN_CONFIG, plugins);
}

extern "C" void terminationHandler(int sig)
{
    while (true) {
        pid_t pid = waitpid(-1, NULL, WNOHANG);
        if (pid <= 0) break;
        if (pid == TelegrafController::pid_) TelegrafController::pid_ = -1;
    }
}

pid_t TelegrafController::getRunningTelegrafPID()
{
    return pid_;
    return 0;
}

double TelegrafController::elapsedFromLastStartedTime()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - lastStartedTime).count() / 1000.0;
}

SDKError TelegrafController::start()
{
    if (!isDevMode()) {
        return SDKError::DEVMODE_DISABLE;
    }    

    if (!isRunning()) {

        posix_spawn_file_actions_t file_actions;

        int ret = posix_spawn_file_actions_init(&file_actions);
        if (ret != 0) {
            SDK_LOG_INFO(MSGID_SDKAGENT, 0, "Error %d when calling posix_spawn_file_actions_init()", ret);
        }

        ret = posix_spawn_file_actions_addopen(&file_actions, STDOUT_FILENO, TELEGRAF_CONSOLE_LOG, O_WRONLY | O_CREAT | O_TRUNC, 0640);
        if (ret != 0) {
            SDK_LOG_INFO(MSGID_SDKAGENT, 0, "Error %d when calling posix_spawn_file_actions_addopen()", ret);
        }

        ret = posix_spawn_file_actions_adddup2(&file_actions, 1, 2);
        if (ret != 0) {
            SDK_LOG_INFO(MSGID_SDKAGENT, 0, "Error %d when calling posix_spawn_file_actions_adddup2()", ret);
        }

        signal(SIGCHLD, terminationHandler);

        char *const argv[] = {
            strdup(TELEGRAF_BIN),
            strdup("-config"),
            strdup(TELEGRAF_MAIN_CONFIG),
            strdup("-config-directory"),
            strdup(TELEGRAF_CONFIG_DIR),
            NULL
        };

        auto errCode = posix_spawn(&pid_, TELEGRAF_BIN, NULL, NULL, argv, NULL);
        if ((errCode == 0) && (pid_ != 0)) {
            SDK_LOG_INFO(MSGID_SDKAGENT, 0, "Started telegraf with pid = %d", pid_);
            lastStartedTime = std::chrono::system_clock::now();

            SDK_LOG_INFO(MSGID_SDKAGENT, 0, "SDKAgent loading telegraf configurations");
            loadConfig();
        }
        else {
            SDK_LOG_ERROR(MSGID_SDKAGENT, 0, "Failed to start telegraf with errorCode: %d", errCode);
        }

        SDK_LOG_INFO(MSGID_SDKAGENT, 0, "Free posix_spawn char* input");
        for (int i = 0; i < 5; i++) {
            free(argv[i]);
        }
    }
    else {
        SDK_LOG_INFO(MSGID_SDKAGENT, 0, "Try to start telegraf but it it's still running with pid = %d", pid_);
    }

    return SDKError::SUCCESS;
}

SDKError TelegrafController::stop()
{
    if (!isDevMode()) {
        return SDKError::DEVMODE_DISABLE;
    }

    if (pid_ <= 0) return SDKError::SUCCESS;

    auto ret = kill(pid_, SIGTERM);
    if (0 == ret) {
        int status = 0;
        waitpid(pid_, &status, 0);
        if (WIFEXITED(status)) {
            SDK_LOG_INFO(MSGID_SDKAGENT, 0, "Telegraf stopped normally. PID = %d", pid_);
        }
        else if (WCOREDUMP(status)) {
            SDK_LOG_INFO(MSGID_SDKAGENT, 0, "Telegraf produced a core dump. PID = %d", pid_);
        }
        else if (WIFSIGNALED(status)) {
            SDK_LOG_INFO(MSGID_SDKAGENT, 0, "Telegraf stopped by signal %d. PID = %d", WTERMSIG(status), pid_);
        }
        pid_ = -1;
    }
    else {
        printf("Failed to send SIGTERM. ErrorCode = %d", ret);
    }

    return SDKError::SUCCESS;
}

SDKError TelegrafController::restart()
{
    if (!isDevMode()) {
        return SDKError::DEVMODE_DISABLE;
    }

    stop();
    start();

    return SDKError::SUCCESS;
}

bool TelegrafController::isRunning()
{
    if (pid_ > 0) {
        auto ret = kill(pid_, 0);
        switch (ret)
        {
            case 0:
                return true;

            case ESRCH:
                SDK_LOG_INFO(MSGID_SDKAGENT, 0, "The pid [%d] does not exist", pid_);
                return false;
            
            default:
                return false;
        }
    }
    
    return false;
}

tomlObject TelegrafController::getConfig()
{
    loadConfig();
    return _allConfig;
}

bool TelegrafController::updateSectionConfig(const std::string &section, tomlObject &inputConfig)
{
    if (section.compare(0, 6, "webOS.") == 0) return true;

    std::string configPath = getSectionConfigPath(section);
    if (configPath.empty()) return true;

    tomlObject currentConfig = readTomlFile(configPath.c_str());

    // remove all the current configuration, leave it blank
    if (inputConfig[section].empty()) {
        writeTomlSection(configPath, section, inputConfig[section]);
    }
    else
    {
        for (auto &cfg : inputConfig[section]) {
            currentConfig[section][cfg.first] = cfg.second;
        }
        writeTomlSection(configPath, section, currentConfig[section]);
    }
    return true;
}

bool checkProcessName(std::string processName)
{
    std::string pid = executeCommand("pgrep -f " + processName, true);
    return !pid.empty();
}

extern "C" void updateProcstatConfig(const pbnjson::JValue & webOSConfigJson)
{
    std::string procstatConfigFile = std::string(TELEGRAF_CONFIG_DIR) + "procstat.conf";

    if (webOSConfigJson.hasKey("webOS.processMonitoring") &&
        (webOSConfigJson["webOS.processMonitoring"]).hasKey("enabled") &&
        ((webOSConfigJson["webOS.processMonitoring"])["enabled"]).asBool())
    {
        std::string processList = "";
        pbnjson::JValue process_name = (webOSConfigJson["webOS.processMonitoring"])["process_name"];
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
                std::string procstatCmd = "echo -e '[[inputs.procstat]]\npid_tag=false\npid_finder=\"native\"\npattern=\"" + processList + "\"' > " + procstatConfigFile;
                executeCommand(procstatCmd);
                return;
            }
        }
    }
    executeCommand("rm -fr " + procstatConfigFile);
}

extern "C" void updateWebOSConfig(tomlObject &inputConfig)
{
    pbnjson::JValue webOSConfigJson = readWebOSJsonConfig();

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
    writeWebOSConfigJson(std::move(webOSConfigJson));
}

bool TelegrafController::setConfig(tomlObject &inputConfig)
{
    for (auto &it : inputConfig) {
        updateSectionConfig(it.first, inputConfig);
    }
    updateWebOSConfig(inputConfig);

    return true;
}

bool TelegrafController::checkInputConfig(tomlObject &inputConfig)
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