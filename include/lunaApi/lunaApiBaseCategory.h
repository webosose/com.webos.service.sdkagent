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

#ifndef __LUNAAPIBASECATEGORY_H__
#define __LUNAAPIBASECATEGORY_H__

#include <luna-service2/lunaservice.h>
#include <pbnjson.hpp>
#include <fstream>
#include <mutex>

class LunaApiBaseCategory
{
public:
    ~LunaApiBaseCategory();
    bool initLunaServiceCategory(LSHandle *);

    LSHandle *pLSHandle;

protected:
    LunaApiBaseCategory();

    const char *pCategory;
    LSMethod *pMethods;

    void LSMessageReplyErrorUnknown(LSHandle *sh, LSMessage *msg);
    void LSMessageReplyErrorInvalidParams(LSHandle *sh, LSMessage *msg);
    void LSMessageReplyErrorBadJSON(LSHandle *sh, LSMessage *msg);
    void LSMessageReplyErrorInvalidConfigurations(LSHandle *sh, LSMessage *msg);
    void LSMessageReplyErrorCollectorIsRunning(LSHandle *sh, LSMessage *msg);
    void LSMessageReplyErrorDevModeDisable(LSHandle *sh, LSMessage *msg);
    void LSMessageReplyPayload(LSHandle *sh, LSMessage *msg, const char *payload);

    static void postEvent(LSHandle *handle, void *subscribeKey, void *payload);
};

#endif
