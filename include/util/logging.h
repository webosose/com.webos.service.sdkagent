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

#ifndef __SERVICE_LOGGING_H__
#define __SERVICE_LOGGING_H__

#include <PmLogLib.h>

#define SDK_LOG_CRITICAL(msgid, kvcount, ...) \
        PmLogCritical(getServiceContext(), msgid, kvcount, ##__VA_ARGS__)

#define SDK_LOG_ERROR(msgid, kvcount, ...) \
        PmLogError(getServiceContext(), msgid, kvcount, ##__VA_ARGS__)

#define SDK_LOG_WARNING(msgid, kvcount, ...) \
        PmLogWarning(getServiceContext(), msgid, kvcount, ##__VA_ARGS__)

#define SDK_LOG_INFO(msgid, kvcount, ...) \
        PmLogInfo(getServiceContext(), msgid, kvcount, ##__VA_ARGS__)

#define SDK_LOG_DEBUG(...) \
        PmLogDebug(getServiceContext(), ##__VA_ARGS__)

/*
// extern "C" void logKmsg(const char *fmt, ...);
#define SDK_KMSG_DEBUG_MSG(b, fmt, arg...) \
        if ((b))                           \
        logKmsg(fmt, ##arg)
*/

/*
#define LOG_BUF_MAX 512
template<typename... Args>
void newLogKmsg(const char* format, Args ...args)
{
    char buf[LOG_BUF_MAX];
    sprintf(buf, format, args...);
    std::string strBuffer = std::string(buf);
    writeTextFile("dev/kmsg", strBuffer);
}
#define SDK_KMSG_DEBUG_MSG(b, fmt, arg...) if ((b)) newLogKmsg(fmt, ##arg)
*/

extern "C" void SDK_KMSG_DEBUG_MSG(const char* format, const char* functionName);

extern PmLogContext getServiceContext();

#define MSGID_SDKAGENT "SDK_AGENT"

#endif
