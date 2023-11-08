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
#include "lunaApiCollector.h"
#include "threadForInterval.h"

#include <map>
#include <iomanip>

ThreadForInterval::ThreadForInterval()
{
    IntervalHandle *intervalHandle = g_new(IntervalHandle, 1);
    if (intervalHandle != NULL)
    {
        intervalHandle->queue = g_async_queue_new();
        intervalHandle->thread = g_thread_new("IntervalThread", ThreadForInterval::intervalHandle_process, intervalHandle);
    }
    pIntervalHandle = intervalHandle;
}

ThreadForInterval::~ThreadForInterval()
{
    intervalHandle_destroy(pIntervalHandle);
}

bool ThreadForInterval::cb_getWebProcessSize(LSHandle *sh, LSMessage *msg, void *user_data)
{
    pbnjson::JValue response = LunaApiCollector::Instance()->convertStringToJson(LSMessageGetPayload(msg));

    if (!response["returnValue"].asBool())
    {
        SDK_LOG_ERROR(MSGID_SDKAGENT, 0, "%s returnValue is false [%d:%s]\n", __FUNCTION__, errno, strerror(errno));
        return false;
    }
    pbnjson::JValue webProcesses = response["WebProcesses"];
    for (int i = 0; i < webProcesses.arraySize(); i++)
    {
        std::string pid = webProcesses[i]["pid"].asString();
        std::string webProcessSize = webProcesses[i]["webProcessSize"].asString();
        webProcessSize.erase(webProcessSize.size() - 2);
        pbnjson::JValue runningApps = webProcesses[i]["runningApps"];
        for (int j = 0; j < runningApps.arraySize(); j++)
        {
            pbnjson::JValue app = runningApps[j];
            std::string processId = app["id"].asString();
            std::string sendData = std::string("webProcessSize,webId=");
            sendData += processId + ",pid=" + pid + " webProcessSize=" + webProcessSize;
            LunaApiCollector::Instance()->sendToTelegraf(sendData);
        }
    }

    return true;
}

float stringToFloat(std::string str)
{
    if (str.empty())
        return 0.0f;
    try
    {
        return std::stof(str);
    }
    catch (const std::exception &expn)
    {
        return 0.0f;
    }
    catch (...)
    {
        return 0.0f;
    }
}

int stringToPositiveInt(std::string str)
{
    if (str.empty())
        return -1;
    try
    {
        return std::stoi(str);
    }
    catch (const std::exception &expn)
    {
        return -1;
    }
    catch (...)
    {
        return -1;
    }
}

int configIntervalSecond = 0;

std::map<int, float> old_process_time_map;
std::string interval_cpu_usage(std::string pid)
{
    // calculate cpu usage from interval seconds
    std::string cmd = "sed -E 's/\\([^)]+\\)/X/' \"/proc/" + pid + "/stat\" | awk '{print $14}'";
    std::string process_utime_str = LunaApiCollector::Instance()->executeCommand(cmd);
    cmd = "sed -E 's/\\([^)]+\\)/X/' \"/proc/" + pid + "/stat\" | awk '{print $15}'";
    std::string process_stime_str = LunaApiCollector::Instance()->executeCommand(std::move(cmd));
    float process_time = stringToFloat(std::move(process_utime_str)) + stringToFloat(std::move(process_stime_str));

    int int_pid = stringToPositiveInt(std::move(pid));
    std::map<int, float>::iterator iter;
    iter = old_process_time_map.find(int_pid);
    if (iter == old_process_time_map.end())
    {
        old_process_time_map[int_pid] = process_time;
        return "0";
    }

    float tmp_old_process_time = old_process_time_map[int_pid];
    float elapsed = process_time - tmp_old_process_time;
    old_process_time_map[int_pid] = process_time;
    elapsed = elapsed / (float)configIntervalSecond;

    std::stringstream cpu_usage_stream;
    cpu_usage_stream << std::fixed << std::setprecision(2) << elapsed;
    return cpu_usage_stream.str();
}

std::string exceptionProcesses[1] = {"telegraf"};
void calculateProcessMonitoring(std::string processName, std::string pid)
{
    if (pid.empty() || (stringToPositiveInt(pid) < 0))
        return;

    // for (int i = 0; i < 1; i++)
    // {
    //     if (processName.find(exceptionProcesses[i]) != std::string::npos)
    //     {
    //         return;
    //     }
    // }
    std::string sendData = std::string("processMonitoring");
    sendData += ",processName=" + processName + ",pid=" + pid + " ";

    sendData += "interval_cpu_usage=" + interval_cpu_usage(pid);

    // calculate memory (kB)
    // VSZ (Virtual Memory Size) (kB)
    std::string cmd = "cat /proc/" + pid + "/stat | cut -d\" \" -f23 | xargs -n 1 bash -c 'echo $(($1/1024))' args";
    std::string VSZ = LunaApiCollector::Instance()->executeCommand(cmd);
    sendData += ",VSZ=" + VSZ;
    // VmRSS = RssAnon + RssFile + RssSHmem (kB)
    cmd = "cat /proc/" + pid + "/status | grep '^VmRSS:' | awk '{print $2}'";
    std::string vmRSS = LunaApiCollector::Instance()->executeCommand(cmd);
    sendData += ",vmRSS=" + vmRSS;
    // RSS (Resident Set Size) (kB)
    cmd = "grep -e '^Rss' /proc/" + pid + "/smaps | awk '{sum += $2} END {print sum}'";
    std::string smaps_RSS = LunaApiCollector::Instance()->executeCommand(cmd);
    sendData += ",smaps_RSS=" + smaps_RSS;
    // PSS (Proportional Set Size) (kB)
    cmd = "grep -e '^Pss' /proc/" + pid + "/smaps | awk '{sum += $2} END {print sum}'";
    std::string smaps_PSS = LunaApiCollector::Instance()->executeCommand(cmd);
    sendData += ",smaps_PSS=" + smaps_PSS;
    // USS (Unique Set Size) = Private_Clean + Private_Dirty (kB)
    cmd = "grep -e '^Private' /proc/" + pid + "/smaps | awk '{sum += $2} END {print sum}'";
    std::string smaps_USS = LunaApiCollector::Instance()->executeCommand(std::move(cmd));
    sendData += ",smaps_USS=" + smaps_USS;
    SDK_LOG_INFO(MSGID_SDKAGENT, 0, "sendData : %s", sendData.c_str());
    LunaApiCollector::Instance()->sendToTelegraf(sendData);
}

void monitoringAllProcesses(pbnjson::JValue runningWebProcesses)
{
    // SDK_LOG_INFO(MSGID_SDKAGENT, 0, "monitoringAllProcesses : %s", runningWebProcesses.stringify().c_str());
    char targetProcessName[256];
    DIR *pDir = opendir("/proc/"); // Open /proc/ directory
    struct dirent *pDirEntry;
    while (NULL != (pDirEntry = readdir(pDir)))
    { // Reading /proc/ entries
        if (strspn(pDirEntry->d_name, "0123456789") == strlen(pDirEntry->d_name))
        { // Checking for numbered directories
            std::string exeLink = "/proc/" + std::string(pDirEntry->d_name) + "/exe";
            int targetResult = readlink(exeLink.c_str(), targetProcessName, sizeof(targetProcessName) - 1);
            if (targetResult > 0)
            {
                targetProcessName[targetResult] = 0;
                std::string targetProcessNameStr(targetProcessName);
                if (targetProcessNameStr.find("WebAppMgr") != std::string::npos)
                { // Not Need???
                    for (int i = 0; i < runningWebProcesses.arraySize(); i++)
                    {
                        pbnjson::JValue webProcess = runningWebProcesses[i];
                        std::string pid = webProcess["webprocessid"].asString();
                        if (strcmp(pid.c_str(), pDirEntry->d_name) == 0)
                        {
                            std::string webProcessName = webProcess["id"].asString();
                            targetProcessNameStr = std::move(webProcessName);
                            break;
                        }
                    }
                }
                calculateProcessMonitoring(std::move(targetProcessNameStr), std::string(pDirEntry->d_name));
            }
        }
    }
    closedir(pDir);
}

bool ThreadForInterval::cb_getRunningProcess(LSHandle *sh, LSMessage *msg, void *user_data)
{
    pbnjson::JValue response = LunaApiCollector::Instance()->convertStringToJson(LSMessageGetPayload(msg));
    if (!response["returnValue"].asBool())
    {
        SDK_LOG_ERROR(MSGID_SDKAGENT, 0, "%s returnValue is false [%d:%s]\n", __FUNCTION__, errno, strerror(errno));
        return false;
    }
    pbnjson::JValue userDataJValue = LunaApiCollector::Instance()->convertStringToJson((char *)user_data);
    pbnjson::JValue process_name = userDataJValue["process_name"];
    if (!process_name.isArray())
    {
        return false;
    }
    if ((process_name.arraySize() == 1) && (process_name[0].asString().compare(".") == 0))
    {
        monitoringAllProcesses(response["running"]);
    }
    else
    {
        pbnjson::JValue runningWebProcessArray = response["running"];
        for (int i = 0; i < runningWebProcessArray.arraySize(); i++)
        {
            pbnjson::JValue webProcess = runningWebProcessArray[i];
            std::string webProcessName = webProcess["id"].asString();
            for (int j = 0; j < process_name.arraySize(); j++)
            {
                std::string processNameStr = process_name[j].asString();
                if (processNameStr.compare(webProcessName) == 0)
                {
                    std::string pid = webProcess["webprocessid"].asString();
                    process_name.remove(j);
                    calculateProcessMonitoring(webProcessName, std::move(pid));
                    break;
                }
            }
        }
        for (int i = 0; i < process_name.arraySize(); i++)
        {
            std::string processNameStr = process_name[i].asString();
            std::string cmd = "ps -fC " + processNameStr + " | grep " + processNameStr + " | awk '{print $2}'";
            std::string pid = LunaApiCollector::Instance()->executeCommand(std::move(cmd));
            calculateProcessMonitoring(std::move(processNameStr), std::move(pid));
        }
    }

    free((char *)user_data);

    return true;
}

gpointer ThreadForInterval::intervalHandle_process(gpointer data)
{
    IntervalHandle *intervalHandle = (IntervalHandle *)data;
    int intervalSecCnt = 1;
    while (intervalHandle != NULL)
    {
        gpointer msg = g_async_queue_try_pop(intervalHandle->queue);
        if ((msg) && (msg == GINT_TO_POINTER(INTERVAL_THREAD_MSG_STOP)))
            break;

        std::string stateResult = LunaApiCollector::Instance()->executeCommand("systemctl status telegraf -l | grep \"Active: active (running)\"");
        if (!stateResult.empty()) // if telegraf is active
        {
            bool isNeedUpdateInterval = false;
            stateResult = stateResult.substr(stateResult.find(";") + 2);
            stateResult = stateResult.substr(0, stateResult.find(" "));
            if (configIntervalSecond <= 0) // initial state
            {
                isNeedUpdateInterval = true;
            }
            else if (stateResult.find("s") != std::string::npos) // telegraf was run in 60 seconds
            {
                isNeedUpdateInterval = true;
            }
            if (isNeedUpdateInterval) // get Interval from telegraf status log
            {
                std::string cmdResult = LunaApiCollector::Instance()->executeCommand("systemctl status telegraf -l | grep \"Interval:\"");
                if (!cmdResult.empty())
                {
                    int tmpIntervalSecond = 0;
                    cmdResult = cmdResult.substr(cmdResult.find("Interval:"));
                    cmdResult = cmdResult.substr(cmdResult.find(":") + 1);
                    cmdResult = cmdResult.substr(0, cmdResult.find(","));
                    if (cmdResult.find("ms") != std::string::npos)
                    {
                        tmpIntervalSecond += 1;
                        cmdResult = cmdResult.substr(cmdResult.find("ms") + 1);
                    }
                    if (cmdResult.find("h") != std::string::npos)
                    {
                        std::string h = cmdResult.substr(0, cmdResult.find("h"));
                        int hour = atoi(h.c_str());
                        tmpIntervalSecond += hour * 60 * 60;
                        cmdResult = cmdResult.substr(cmdResult.find("h") + 1);
                    }
                    if (cmdResult.find("m") != std::string::npos)
                    {
                        std::string m = cmdResult.substr(0, cmdResult.find("m"));
                        int minute = atoi(m.c_str());
                        tmpIntervalSecond += minute * 60;
                        cmdResult = cmdResult.substr(cmdResult.find("m") + 1);
                    }
                    if (cmdResult.find("s") != std::string::npos)
                    {
                        std::string s = cmdResult.substr(0, cmdResult.find("s"));
                        int second = atoi(s.c_str());
                        tmpIntervalSecond += second;
                    }
                    if (configIntervalSecond != tmpIntervalSecond)
                    {
                        configIntervalSecond = tmpIntervalSecond;
                        intervalSecCnt = 1;
                    }
                }
            }
            if (intervalSecCnt <= 1)
            {
                intervalSecCnt = configIntervalSecond;

                pbnjson::JValue tmpValue = LunaApiCollector::Instance()->readwebOSConfigJson();
                if (tmpValue.hasKey("webOS.webProcessSize") && (tmpValue["webOS.webProcessSize"]).hasKey("enabled") && ((tmpValue["webOS.webProcessSize"])["enabled"]).asBool())
                {
                    LSError lserror;
                    LSErrorInit(&lserror);
                    if (!LSCall(LunaApiCollector::Instance()->pLSHandle,
                                "luna://com.webos.service.webappmanager/getWebProcessSize",
                                "{}",
                                ThreadForInterval::cb_getWebProcessSize,
                                NULL,
                                NULL,
                                &lserror))
                    {
                        LSErrorPrint(&lserror, stderr);
                        LSErrorFree(&lserror);
                    }
                }

                if (tmpValue.hasKey("webOS.processMonitoring") && (tmpValue["webOS.processMonitoring"]).hasKey("enabled") && ((tmpValue["webOS.processMonitoring"])["enabled"]).asBool())
                {
                    pbnjson::JValue processMonitoringJValue = tmpValue["webOS.processMonitoring"];
                    LSError lserror;
                    LSErrorInit(&lserror);
                    const char *ctx = strdup(processMonitoringJValue.stringify().c_str());
                    if (!LSCall(LunaApiCollector::Instance()->pLSHandle,
                                "luna://com.webos.applicationManager/running",
                                "{}",
                                ThreadForInterval::cb_getRunningProcess,
                                (void *)ctx,
                                NULL,
                                &lserror))
                    {
                        LSErrorPrint(&lserror, stderr);
                        LSErrorFree(&lserror);
                    }
                }
            }
            else
            {
                intervalSecCnt--;
            }
        }

        g_usleep(G_USEC_PER_SEC);
    }

    return NULL;
}

void ThreadForInterval::intervalHandle_destroy(IntervalHandle *intervalHandle)
{
    g_return_if_fail(intervalHandle != NULL);
    g_async_queue_push(intervalHandle->queue, GINT_TO_POINTER(INTERVAL_THREAD_MSG_STOP));
    g_thread_join(intervalHandle->thread);
    g_async_queue_unref(intervalHandle->queue);
    g_free(intervalHandle);
}
