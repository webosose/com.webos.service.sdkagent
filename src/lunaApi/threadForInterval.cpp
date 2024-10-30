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
#include "common.h"
#include "telegrafController.h"

#include <unistd.h>
#include <unordered_map>
#include <iomanip>
#include <unordered_set>
#include <iterator>

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

bool ThreadForInterval::cb_getWebProcessSize(LSHandle *sh, LSMessage *msg, void *monitoringProcesses)
{
    pbnjson::JValue response = stringToJValue(LSMessageGetPayload(msg));

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
            SDK_LOG_INFO(MSGID_SDKAGENT, 0, "[webProcessSize] sendData : %s", sendData.c_str());
            LunaApiCollector::Instance()->sendToTelegraf(sendData);
        }
    }

    return true;
}

static unsigned long page_to_kb(int x)
{
    int npage_per_kb = getpagesize() / 1024;
    return (unsigned long)(x * npage_per_kb);
}

char** str_split(char* a_str, const char a_delim)
{
    char** result    = 0;
    size_t count     = 0;
    char* tmp        = a_str;
    char* last_comma = 0;
    char delim[2];
    delim[0] = a_delim;
    delim[1] = 0;

    // Count how many elements will be extracted.
    while (*tmp)
    {
        if (a_delim == *tmp)
        {
            count++;
            last_comma = tmp;
        }
        tmp++;
    }

    // Add space for trailing token.
    count += last_comma < (a_str + strlen(a_str) - 1);

    // Add space for terminating null string so caller knows where the list of returned strings ends.
    count++;

    result = (char**) malloc(sizeof(char*) * count);

    if (result)
    {
        size_t idx  = 0;
        char* token = strtok(a_str, delim);

        while (token)
        {
            assert(idx < count);
            *(result + idx++) = strdup(token);
            token = strtok(0, delim);
        }
        assert(idx == count - 1);
        *(result + idx) = 0;
    }

    return result;
}

char buffer[1024];

// get utime/stime in /proc/pid/stat
float getProcessTime(const std::string& sPID)
{
    std::string psPath = "/proc/" + sPID + "/stat";
    std::string line;
    float utime = 0.0f;
    float stime = 0.0f;

    // if(access(psPath.c_str(), F_OK) == 0)
    // {
    //     gchar * buffer = NULL;
    //     gsize bufferSize = 0;
    //     GError * err = NULL;

    //     if (!g_file_get_contents(psPath.c_str(), &buffer, &bufferSize, &err)) {
    //         SDK_LOG_ERROR(MSGID_SDKAGENT, 0, "Error reading /proc/%s/stat", sPID.c_str());
    //     }
    //     else {
    //         // we want to obtain 14-15th values (utime, stime). 
    //         // split to maximum 16 tokens (last token is for the rest of the string)
    //         gchar **tokens = g_strsplit(buffer, " \t\n\0", 16);
    //         utime = (float)g_strtod(tokens[13], NULL);
    //         stime = (float)g_strtod(tokens[14], NULL);
    //         g_strfreev(tokens);
    //     }
        
    //     g_free(buffer);
    //     g_error_free(err);
    // }

    if(access(psPath.c_str(), F_OK) == 0)
    {
        FILE * fp = fopen(psPath.c_str(), "r");
        if (NULL != fp) {
            if ( fgets(buffer, 1024, fp) != NULL ) {
                char ** tokens = str_split(buffer, ' ');
                if (tokens) {
                    SDK_LOG_INFO(MSGID_SDKAGENT, 0, "token for utime: %s", *(tokens + 13));
                    SDK_LOG_INFO(MSGID_SDKAGENT, 0, "token for stime: %s", *(tokens + 14));
                    utime = strtof(tokens[13], NULL);
                    stime = strtof(tokens[14], NULL);

                    int i;
                    for (i = 0; *(tokens + i); i++) {
                        free(*(tokens + i));
                    }
                    printf("\n");
                    free(tokens);
                }
            }
            fclose(fp);
        }
    }

    return utime + stime;
}

int configIntervalSecond = 0;

std::unordered_map<int, float> process_time_mapper;
std::string intervalCPUsage(int pid)
{
    std::string sPID = std::to_string(pid);
    float curr_process_time = getProcessTime(sPID);

    if (process_time_mapper.find(pid) == process_time_mapper.end()) {
        process_time_mapper[pid] = curr_process_time;
        return "0";
    }

    float prev_process_time = process_time_mapper[pid];
    float elapsed = curr_process_time - prev_process_time;
    process_time_mapper[pid] = curr_process_time;
    elapsed = elapsed / (float)configIntervalSecond;

    std::stringstream cpu_usage_stream;
    cpu_usage_stream << std::fixed << std::setprecision(2) << elapsed;
    return cpu_usage_stream.str();
}

std::string intervalGPUsage(const std::string & sPID)
{
    int gpu = 0;
    std::string procGPUPath = "/proc/gpu/" + sPID;
    if (access(procGPUPath.c_str(), F_OK) == 0)
    {
        gchar * buffer = NULL;
        gsize bufferSize = 0;
        GError * err = NULL;

        if (!g_file_get_contents(procGPUPath.c_str(), &buffer, &bufferSize, &err)) {
            SDK_LOG_ERROR(MSGID_SDKAGENT, 0, "Error reading /proc/gpu/%s", sPID.c_str());
        }
        else {
            gchar **tokens = g_strsplit(buffer, " \t\n\0", 2);
            gpu = (int)g_ascii_strtoll(tokens[0], (char**)NULL, 10);
            g_strfreev(tokens);
        }
        
        g_free(buffer);
        g_error_free(err);
    }
    else
    {
        SDK_LOG_INFO(MSGID_SDKAGENT, 0, "Cannot access /proc/gpu/%s", sPID.c_str());
        return "0";
    }

    return std::to_string(page_to_kb(gpu) / 1024);      // to KB
}

std::string exceptionProcesses[1] = {"telegraf"};
void calculateProcessMonitoring(const std::string& processName, const std::string& sPID)
{
    int pid = string_to_positive_int(sPID);
    if (pid == -1 || (configIntervalSecond == 0)) return;
        
    std::string sendData = std::string("processMonitoring");
    sendData += ",processName=" + processName + ",pid=" + sPID + " ";

    sendData += "interval_cpu_usage=" + intervalCPUsage(pid);
    sendData += ",interval_gpu_usage=" + intervalGPUsage(sPID);

    // calculate memory (kB)
    // VSZ (Virtual Memory Size) (kB)
    /*
    std::string cmd = "cat /proc/" + sPID + "/stat | cut -d\" \" -f23 | xargs -n 1 bash -c 'echo $(($1/1024))' args";
    std::string VSZ = executeCommand(cmd);
    sendData += ",VSZ=" + VSZ;
    // VmRSS = RssAnon + RssFile + RssSHmem (kB)
    cmd = "cat /proc/" + sPID + "/status | grep '^VmRSS:' | awk '{print $2}'";
    std::string vmRSS = executeCommand(cmd);
    sendData += ",vmRSS=" + vmRSS;
    // RSS (Resident Set Size) (kB)
    cmd = "grep -e '^Rss' /proc/" + sPID + "/smaps | awk '{sum += $2} END {print sum}'";
    std::string smaps_RSS = executeCommand(cmd);
    sendData += ",smaps_RSS=" + smaps_RSS;
    // PSS (Proportional Set Size) (kB)
    cmd = "grep -e '^Pss' /proc/" + sPID + "/smaps | awk '{sum += $2} END {print sum}'";
    std::string smaps_PSS = executeCommand(cmd);
    sendData += ",smaps_PSS=" + smaps_PSS;
    // USS (Unique Set Size) = Private_Clean + Private_Dirty (kB)
    cmd = "grep -e '^Private' /proc/" + sPID + "/smaps | awk '{sum += $2} END {print sum}'";
    std::string smaps_USS = executeCommand(cmd);
    sendData += ",smaps_USS=" + smaps_USS;
    */
    SDK_LOG_INFO(MSGID_SDKAGENT, 0, "[processMonitoring] sendData : %s", sendData.c_str());
    LunaApiCollector::Instance()->sendToTelegraf(sendData);
}

void monitoringAllProcesses(pbnjson::JValue runningWebProcesses)
{
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
                std::string targetmonitorProcessName(targetProcessName);
                if (targetmonitorProcessName.find("WebAppMgr") != std::string::npos)
                { // Not Need???
                    for (int i = 0; i < runningWebProcesses.arraySize(); i++)
                    {
                        pbnjson::JValue webProcess = runningWebProcesses[i];
                        std::string pid = webProcess["processid"].asString();
                        if (strcmp(pid.c_str(), pDirEntry->d_name) == 0)
                        {
                            std::string runningProcessName = webProcess["id"].asString();
                            targetmonitorProcessName = std::move(runningProcessName);
                            break;
                        }
                    }
                }
                calculateProcessMonitoring(targetmonitorProcessName, std::string(pDirEntry->d_name));
            }
        }
    }
    closedir(pDir);
}

bool ThreadForInterval::cb_getRunningProcess(LSHandle *sh, LSMessage *msg, void *monitoringProcesses)
{
    pbnjson::JValue response = stringToJValue(LSMessageGetPayload(msg));
    if (!response["returnValue"].asBool())
    {
        SDK_LOG_ERROR(MSGID_SDKAGENT, 0, "%s returnValue is false [%d:%s]\n", __FUNCTION__, errno, strerror(errno));
        return false;
    }

    pbnjson::JValue monitorProcessNameList = stringToJValue((char*)monitoringProcesses)["process_name"];
    if (!monitorProcessNameList.isArray()) return false;

    pbnjson::JValue allRunningProcesses = response["running"];

    if ((monitorProcessNameList.arraySize() == 1) &&
        (monitorProcessNameList[0].asString().compare(".") == 0)
    ) {
        monitoringAllProcesses(allRunningProcesses);
    }
    else
    {
        // create a set for fast look-up
        std::unordered_set<std::string> monitorProcSet;
        for (int i = 0; i < monitorProcessNameList.arraySize(); i++) {
            monitorProcSet.insert(trim_string(monitorProcessNameList[i].asString()));
        }
        
        // running process in monitoring processes -> collect data
        for (int i = 0; i < allRunningProcesses.arraySize(); i++) {
            pbnjson::JValue runningProcess = allRunningProcesses[i];
            std::string runningProcessName = trim_string(runningProcess["id"].asString());

            if (monitorProcSet.find(runningProcessName) != monitorProcSet.end()) {
                std::string sPID = runningProcess["processid"].asString();
                monitorProcSet.erase(runningProcessName);
                calculateProcessMonitoring(runningProcessName, sPID);
            }
        }
    }

    free((char *)monitoringProcesses);

    return true;
}

bool needUpdateTelegrafAgentInterval()
{
    // update for the first time
    if (configIntervalSecond <= 0) return true;

    // check if telegraf is recently started. If yes -> update interval
    if (TelegrafController::getInstance()->elapsedFromLastStartedTime() <= 60) {
        return true;
    }

    return false;
}

int ThreadForInterval::getTelegrafAgentInterval()
{
    std::string strInterval = trim_string(TelegrafController::getInstance()->getConfig()["agent"]["interval"]);
    strInterval.erase(0, 1);
    strInterval.pop_back();

    if (!strInterval.empty())
    {
        int newIntervalInSecond = 0;

        if (strInterval.find("ms") != std::string::npos)
        {
            newIntervalInSecond += 1;
            strInterval = strInterval.substr(strInterval.find("ms") + 1);
        }
        if (strInterval.find("h") != std::string::npos)
        {
            std::string h = strInterval.substr(0, strInterval.find("h"));
            int hour = string_to_positive_int(h);
            newIntervalInSecond += hour * 60 * 60;
            strInterval = strInterval.substr(strInterval.find("h") + 1);
        }
        if (strInterval.find("m") != std::string::npos)
        {
            std::string m = strInterval.substr(0, strInterval.find("m"));
            int minute = string_to_positive_int(m);
            newIntervalInSecond += minute * 60;
            strInterval = strInterval.substr(strInterval.find("m") + 1);
        }
        if (strInterval.find("s") != std::string::npos)
        {
            std::string s = strInterval.substr(0, strInterval.find("s"));
            int second = string_to_positive_int(s);
            newIntervalInSecond += second;
        }

        return newIntervalInSecond;
    }

    return -1;
}

void ThreadForInterval::collectWebProcessSize(pbnjson::JValue & webOSConfig)
{
    if (
        webOSConfig.hasKey("webOS.webProcessSize") &&
        webOSConfig["webOS.webProcessSize"].hasKey("enabled") &&
        webOSConfig["webOS.webProcessSize"]["enabled"].asBool()
    ) {
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
}

void ThreadForInterval::collectProcessesData(pbnjson::JValue & webOSConfig)
{
    if (
        webOSConfig.hasKey("webOS.processMonitoring") &&
        webOSConfig["webOS.processMonitoring"].hasKey("enabled") &&
        webOSConfig["webOS.processMonitoring"]["enabled"].asBool()
    ) {
        pbnjson::JValue processMonitoringJValue = webOSConfig["webOS.processMonitoring"];
        LSError lserror;
        LSErrorInit(&lserror);
        char *ctx = strdup(processMonitoringJValue.stringify().c_str());
        if (!LSCall(LunaApiCollector::Instance()->pLSHandle,
                    "luna://com.webos.applicationManager/running",
                    "{}",
                    ThreadForInterval::cb_getRunningProcess,
                    (void*)ctx,
                    NULL,
                    &lserror))
        {
            LSErrorPrint(&lserror, stderr);
            LSErrorFree(&lserror);
        }
    }    
}

gpointer ThreadForInterval::intervalHandle_process(gpointer data)
{
    IntervalHandle *intervalHandle = (IntervalHandle *)data;
    int intervalCountDown = 1;
    while (intervalHandle != NULL)
    {
        gpointer msg = g_async_queue_try_pop(intervalHandle->queue);
        if ((msg) && (msg == GINT_TO_POINTER(INTERVAL_THREAD_MSG_STOP))) break;
            
        if (TelegrafController::getInstance()->isRunning())
        {
            if (needUpdateTelegrafAgentInterval())
            {
                int newIntervalInSecond = getTelegrafAgentInterval();
                if ((newIntervalInSecond != -1) && (configIntervalSecond != newIntervalInSecond))
                {
                    configIntervalSecond = newIntervalInSecond;
                    intervalCountDown = 1;     // reassign to start new interval
                }
            }

            if (intervalCountDown <= 1) {
                intervalCountDown = configIntervalSecond;
                pbnjson::JValue webOSConfigJson = readWebOSJsonConfig();
                collectWebProcessSize(webOSConfigJson);
                collectProcessesData(webOSConfigJson);
            }
            else {
                intervalCountDown--;
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
