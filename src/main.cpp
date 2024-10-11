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
#include <csignal>

GMainLoop *mainLoop;

extern "C" void signalHandler(int signal)
{
    switch (signal)
    {
    case SIGSTOP:
        // SDK_KMSG_DEBUG_MSG(1, "[%s] %s( ... ) , case SIGSTOP\n", MSGID_SDKAGENT, __FUNCTION__);
        SDK_KMSG_DEBUG_MSG("%s() , case SIGSTOP\n", __FUNCTION__);
        break;

    case SIGCONT:
        // SDK_KMSG_DEBUG_MSG(1, "[%s] %s( ... ) , case SIGCONT\n", MSGID_SDKAGENT, __FUNCTION__);
        SDK_KMSG_DEBUG_MSG("%s() , case SIGCONT\n", __FUNCTION__);
        break;

    case SIGTERM:
        // SDK_KMSG_DEBUG_MSG(1, "[%s] %s( ... ) , case SIGTERM\n", MSGID_SDKAGENT, __FUNCTION__);
        SDK_KMSG_DEBUG_MSG("%s() , case SIGTERM\n", __FUNCTION__);
        if (g_main_loop_is_running(mainLoop))
            g_main_loop_quit(mainLoop);
        g_main_loop_unref(mainLoop);
        break;

    case SIGINT:
        // SDK_KMSG_DEBUG_MSG(1, "[%s] %s( ... ) , case SIGINT\n", MSGID_SDKAGENT, __FUNCTION__);
        SDK_KMSG_DEBUG_MSG("%s() , case SIGINT\n", __FUNCTION__);
        if (g_main_loop_is_running(mainLoop))
            g_main_loop_quit(mainLoop);
        g_main_loop_unref(mainLoop);
        break;
    }
}

void registSignalHandler()
{
    /*
     * Register a function to be able to gracefully handle termination signals
     * from the OS or other processes.
     */
    std::signal(SIGSTOP, signalHandler);
    std::signal(SIGCONT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    std::signal(SIGINT, signalHandler);
}

int main(int argc, char **argv)
{
    registSignalHandler();

    mainLoop = g_main_loop_new(NULL, FALSE);

    LSError lserror;
    LSErrorInit(&lserror);
    const char *serviceId = "com.webos.service.sdkagent";
    LSHandle *lsHandle = NULL;

    bool retVal = false;
    if (LSRegister(serviceId, &lsHandle, &lserror))
    {
        if (LSGmainAttach(lsHandle, mainLoop, &lserror))
        {
            retVal = LunaApiCollector::Instance()->initLunaServiceCategory(lsHandle);
        }
    }
    if (!retVal)
    {
        SDK_LOG_CRITICAL(MSGID_SDKAGENT, 1, PMLOGKS("ERRTEXT", lserror.message), "Could not initialize %s", serviceId);
        LSErrorFree(&lserror);
        exit(-1);
    }

    SDK_LOG_INFO(MSGID_SDKAGENT, 0, "%s( ... ) , com.webos.service.sdkagent daemon started", __FUNCTION__);
    LunaApiCollector::Instance()->initialize();
    g_main_loop_run(mainLoop);

    return 0;
}
