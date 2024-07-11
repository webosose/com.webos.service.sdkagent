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

#ifndef __THREADFORINTERVAL_H__
#define __THREADFORINTERVAL_H__

#include <glib.h>
#include <luna-service2/lunaservice.h>
#include <pbnjson.hpp>
#include <iomanip>

enum
{
    INTERVAL_THREAD_MSG_STOP = -1,
};

typedef struct _IntervalHandle IntervalHandle;
struct _IntervalHandle
{
    GThread *thread;
    GAsyncQueue *queue;
};

class ThreadForInterval
{
public:
    ThreadForInterval();
    ~ThreadForInterval();

    void intervalHandle_destroy(IntervalHandle *intervalHandle);

private:
    IntervalHandle *pIntervalHandle;

    static gpointer intervalHandle_process(gpointer data);

    static bool cb_getWebProcessSize(LSHandle *sh, LSMessage *msg, void *user_data);
    static bool cb_getRunningProcess(LSHandle *sh, LSMessage *msg, void *user_data);

    static int getTelegrafAgentInterval();
};

#endif