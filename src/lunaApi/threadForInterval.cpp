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
        std::string webProcessSize = webProcesses[i]["webProcessSize"].asString();
        webProcessSize.erase(webProcessSize.size() - 2);
        pbnjson::JValue runningApps = webProcesses[i]["runningApps"];
        for (int j = 0; j < runningApps.arraySize(); j++)
        {
            pbnjson::JValue app = runningApps[j];
            std::string processId = app["id"].asString();
            std::string sendData = std::string("webProcessSize,webId=");
            sendData += processId + " webSize=" + webProcessSize;
            LunaApiCollector::Instance()->sendToTelegraf(sendData);
        }
    }

    return true;
}

bool ThreadForInterval::cb_getAllAppProperties(LSHandle *sh, LSMessage *msg, void *user_data)
{
    pbnjson::JValue response = LunaApiCollector::Instance()->convertStringToJson(LSMessageGetPayload(msg));
    for (int i = 0; i < response.arraySize(); i++)
    {
        pbnjson::JValue tmpValue = response[i];
        if (tmpValue.hasKey("webProcessSize") && (tmpValue["webProcessSize"]).hasKey("enabled") && ((tmpValue["webProcessSize"])["enabled"]).asBool())
        {
            // Test configuration
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
    return true;
}

gpointer ThreadForInterval::intervalHandle_process(gpointer data)
{
    IntervalHandle *intervalHandle = (IntervalHandle *)data;
    int intervalSecCnt = 1;
    int configIntervalSecond = 0;
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

                LSError lserror;
                LSErrorInit(&lserror);
                if (!LSCall(LunaApiCollector::Instance()->pLSHandle,
                            "luna://com.webos.service.preferences/appProperties/getAllAppProperties",
                            "{\"appId\":\"com.webos.service.sdkagent\"}",
                            ThreadForInterval::cb_getAllAppProperties,
                            NULL,
                            NULL,
                            &lserror))
                {
                    LSErrorPrint(&lserror, stderr);
                    LSErrorFree(&lserror);
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