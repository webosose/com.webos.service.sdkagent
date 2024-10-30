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

#ifndef __TELEGRAFCONTROLLER_H__
#define __TELEGRAFCONTROLLER_H__

#include "tomlParser.h"
#include <mutex>
#include <chrono>
#include <string>
#include <pbnjson.hpp>
#include "tomlParser.h"
#include "errorCode.h"

class TelegrafController {

public:
    static inline TelegrafController * m_pTelegrafInstancePtr = nullptr;
    static inline std::mutex m_pMutex;
    // static inline std::mutex webOSConfigMutex;
    static inline pid_t pid_ {-1};
    static inline std::chrono::time_point<std::chrono::system_clock> lastStartedTime;  
    static inline tomlObject _allConfig {};

protected:

    TelegrafController();
    ~TelegrafController();

private:
    void initAvailableConfigurations();
    void splitMainConfig();
    bool updateSectionConfig(const std::string &section, tomlObject &inputConfig);
    static void loadConfig();

public:
    bool checkInputConfig(tomlObject &inputConfig);

public:

    TelegrafController(TelegrafController &other) = delete;
    void operator=(const TelegrafController&) = delete;

    // getInstance()
    static TelegrafController *getInstance()
    {
        std::lock_guard<std::mutex> lock(m_pMutex);
        if (m_pTelegrafInstancePtr == nullptr)
        {
            m_pTelegrafInstancePtr = new TelegrafController();
            atexit(releaseTelegrafInstance);
        }
        return m_pTelegrafInstancePtr;
    }

    // release
    static void releaseTelegrafInstance()
    {
        if (m_pTelegrafInstancePtr)
        {
            delete m_pTelegrafInstancePtr;
            m_pTelegrafInstancePtr = nullptr;
        }
    }

public:
    int getRunningTelegrafPID();
    double elapsedFromLastStartedTime();

    static SDKError start();
    static SDKError stop();
    static SDKError restart();
    static bool isRunning();
    tomlObject getConfig();

    bool setConfig(tomlObject &inputConfig);
};

#endif