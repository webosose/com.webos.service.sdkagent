// Copyright (c) 2018-2019 LG Electronics, Inc.
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

#include <json-c/json.h>
#include <lunaApiBaseCategory.h>

class lunaApiCollector : public lunaApiBaseCategory {
public:
    ~lunaApiCollector();

    static lunaApiCollector* Instance() {
        if (!pInstance) pInstance = new lunaApiCollector;
        return pInstance;
    }

private:
    lunaApiCollector();

    static const LSMethod collectorMethods[];
    json_object *availableConfigurationJson;

private:
    static bool start(LSHandle *sh, LSMessage *msg, void *data);
    static bool stop(LSHandle *sh, LSMessage *msg, void *data);
    static bool restart(LSHandle *sh, LSMessage *msg, void *data);
    static bool startOnBoot(LSHandle *sh, LSMessage *msg, void *data);

    static bool getStatus(LSHandle *sh, LSMessage *msg, void *data);

    static bool setConfig(LSHandle *sh, LSMessage *msg, void *data);
    static bool getConfig(LSHandle *sh, LSMessage *msg, void *data);

    static bool getData(LSHandle *sh, LSMessage *msg, void *data);

    static void postEvent(void *subscribeKey, void *payload);

private:
    static lunaApiCollector *pInstance;
};

#endif
