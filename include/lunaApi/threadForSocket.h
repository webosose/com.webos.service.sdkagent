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

#ifndef __THREADFORSOCKET_H__
#define __THREADFORSOCKET_H__

#include <glib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <string>
#include <unistd.h>

typedef struct _SocketHandle SocketHandle;
struct _SocketHandle
{
    GThread *thread;
    GAsyncQueue *queue;
};

typedef struct _SocketMsg SocketMsg;
struct _SocketMsg
{
    gpointer send_string;
    gpointer data_int;
};

class ThreadForSocket
{
public:
    ThreadForSocket();
    ~ThreadForSocket();

    void socketHandle_destroy(SocketHandle *socketHandle);

    void sendToMSGQ(std::string &data);

private:
    SocketHandle *pSocketHandle;

    static gpointer socketHandle_process(gpointer data);
};

#endif