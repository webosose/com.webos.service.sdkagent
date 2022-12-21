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
#include "lunaApiBaseCategory.h"

lunaApiBaseCategory::lunaApiBaseCategory():
pLSHandle(NULL) {
}

lunaApiBaseCategory::~lunaApiBaseCategory() {
}

bool lunaApiBaseCategory::initLunaServiceCategory(LSHandle *lsHandle) {
    LSError lserror;
    LSErrorInit(&lserror);

    SDK_LOG_INFO(MSGID_SDKAGENT, 0, "[ %s : %d ] %s( ... )", __FILE__, __LINE__, __FUNCTION__);

    if(!LSRegisterCategory(lsHandle, pCategory, pMethods, NULL, NULL, &lserror)) {
        LSErrorPrint(&lserror, stderr);
        LSErrorFree(&lserror);
        return false;
    }
    pLSHandle = lsHandle;

    return true;
}

void lunaApiBaseCategory::LSMessageReplyErrorInvalidParams(LSHandle *sh, LSMessage *msg) {
    LSError lserror;
    LSErrorInit(&lserror);

    bool retVal = LSMessageReply(sh, msg, "{\"returnValue\":false,\"errorCode\":2,\"errorText\":\"Invalid parameters.\"}", NULL);
    if (!retVal) {
        LSErrorPrint(&lserror, stderr);
        LSErrorFree(&lserror);
    }

    return;
}

void lunaApiBaseCategory::LSMessageReplyErrorBadJSON(LSHandle *sh, LSMessage *msg) {
    LSError lserror;
    LSErrorInit(&lserror);

    bool retVal = LSMessageReply(sh, msg, "{\"returnValue\":false,\"errorCode\":3,\"errorText\":\"Malformed json.\"}", NULL);
    if (!retVal) {
        LSErrorPrint(&lserror, stderr);
        LSErrorFree(&lserror);
    }

    return;
}

void lunaApiBaseCategory::LSMessageReplyPayload(LSHandle *sh, LSMessage *msg, char *payload) {
    LSError lserror;
    LSErrorInit(&lserror);

    bool retVal;

    if (payload == NULL) {
        retVal = LSMessageReply(sh, msg, "{\"returnValue\":true}", NULL);
    } else {
        retVal = LSMessageReply(sh, msg, payload, NULL);
    }

    if (!retVal) {
        LSErrorPrint(&lserror, stderr);
        LSErrorFree(&lserror);
    }

    return;
}

void lunaApiBaseCategory::postEvent(LSHandle *handle, void *subscribeKey, void *payload) {
    LSError lserror;
    LSErrorInit(&lserror);

    // Post event message to the clients
    bool retVal = LSSubscriptionReply(
        handle,
        (char *)subscribeKey,
        (char *)payload,
        &lserror
    );

    if (!retVal) {
        LSErrorPrint(&lserror, stderr);
        LSErrorFree(&lserror);
    }

    return;
}

std::string lunaApiBaseCategory::executeCommand(std::string pszCommand) {
    FILE *fp = popen(pszCommand.c_str(), "r");
    SDK_LOG_INFO(MSGID_SDKAGENT, 0, "%s : %s", __FUNCTION__, pszCommand.c_str());
    if (!fp) {
        SDK_LOG_ERROR(MSGID_SDKAGENT, 0, "Error (!fp) [%d:%s]\n", errno, strerror(errno));
        return NULL;
    }

    std::string retStr = "";
    char *ln = NULL;
    size_t len = 0;
    while (getline(&ln, &len, fp) != -1) {
        retStr = retStr.append(ln);
        if (retStr.at(retStr.length()-1) == '\n') {
            retStr = retStr.erase(retStr.length()-1, 1);
        }
    }
    free(ln);
    pclose(fp);
    return retStr;
}

pbnjson::JValue lunaApiBaseCategory::convertStringToJson(const char *rawData)
{
    pbnjson::JInput input(rawData);
    pbnjson::JSchema schema = pbnjson::JSchemaFragment("{}");
    pbnjson::JDomParser parser;
    if (!parser.parse(input, schema))
    {
        return pbnjson::JValue();
    }
    return parser.getDom();
}