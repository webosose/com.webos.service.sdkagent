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

#ifndef __LUNAAPICOLLECTOR_H__
#define __LUNAAPICOLLECTOR_H__

// #include <json-c/json.h>
#include <lunaApiBaseCategory.h>
#include "threadForInterval.h"
#include "threadForSocket.h"
#include "telegrafController.h"

// Singleton class
// https://henriquesd.medium.com/singleton-vs-static-class-e6b2b32ec331
class LunaApiCollector : public LunaApiBaseCategory
{
public:
    static LunaApiCollector *Instance()
    {
        if (_instance == nullptr)
        {
            _instance = new LunaApiCollector();
            atexit(release_instance);
        }
        return _instance;
    }
    static void release_instance()
    {
        if (_instance)
        {
            delete _instance;
            _instance = nullptr;
        }
    }

    void initialize();
    void sendToTelegraf(std::string &msg);

    void loadInitConfig();

private:
    static LunaApiCollector *_instance;

    LunaApiCollector();
    ~LunaApiCollector();

    static const LSMethod collectorMethods[];

    ThreadForInterval *pThreadForInterval = nullptr;
    ThreadForSocket *pThreadForSocket = nullptr;

    static bool start(LSHandle *sh, LSMessage *msg, void *data);
    static bool stop(LSHandle *sh, LSMessage *msg, void *data);
    static bool restart(LSHandle *sh, LSMessage *msg, void *data);
    static bool startOnBoot(LSHandle *sh, LSMessage *msg, void *data);

    static bool getStatus(LSHandle *sh, LSMessage *msg, void *data);

    static bool setConfig(LSHandle *sh, LSMessage *msg, void *data);
    static bool getConfig(LSHandle *sh, LSMessage *msg, void *data);

    static bool getData(LSHandle *sh, LSMessage *msg, void *data);

    static void postEvent(void *subscribeKey, void *payload);
};

#endif
