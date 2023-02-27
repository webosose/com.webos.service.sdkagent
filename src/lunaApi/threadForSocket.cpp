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

#include "threadForSocket.h"
#include "logging.h"

ThreadForSocket::ThreadForSocket()
{
    SocketHandle *socketHandle = g_new(SocketHandle, 1);
    if (socketHandle != NULL)
    {
        socketHandle->queue = g_async_queue_new();
        socketHandle->thread = g_thread_new("SocketThread", ThreadForSocket::socketHandle_process, socketHandle);
    }
    pSocketHandle = socketHandle;
}

ThreadForSocket::~ThreadForSocket()
{
    socketHandle_destroy(pSocketHandle);
}

void ThreadForSocket::sendToMSGQ(std::string &data)
{
    gint test_data = 0;
    SocketMsg *msg = g_slice_new(SocketMsg);
    if (msg != NULL)
    {
        msg->send_string = g_strdup(data.c_str());
        msg->data_int = GINT_TO_POINTER(test_data);
    }
    g_async_queue_push(pSocketHandle->queue, msg);
}

gpointer ThreadForSocket::socketHandle_process(gpointer data)
{
    SocketHandle *socketHandle = (SocketHandle *)data;
    while (socketHandle != NULL)
    {
        SocketMsg *msg = (SocketMsg *)g_async_queue_pop(socketHandle->queue);
        if (msg)
        {
            if (msg == GINT_TO_POINTER(-1))
                break;

            std::string sendData = std::string((gchar *)(((SocketMsg *)msg)->send_string));

            /**
             * https://docs.influxdata.com/influxdb/v1.8/write_protocols/line_protocol_reference/
             * https://docs.influxdata.com/influxdb/v1.8/write_protocols/line_protocol_tutorial/
             * Need to check tag key, field key
             */
            int sock = socket(AF_UNIX, SOCK_DGRAM, 0);
            struct sockaddr_un sock_name;
            if (sock < 0)
            {
                SDK_LOG_ERROR(MSGID_SDKAGENT, 0, "Error opening datagram socket [%d:%s]\n", errno, strerror(errno));
                continue;
            }

            sock_name.sun_family = AF_UNIX;
            std::string sockPath = "/tmp/telegraf.sock";
            strncpy(sock_name.sun_path, sockPath.c_str(), (sockPath.length() + 1));

            if (sendto(sock, sendData.c_str(), sendData.length(), 0, (struct sockaddr *)&sock_name, sizeof(struct sockaddr_un)) < 0)
            {
                SDK_LOG_ERROR(MSGID_SDKAGENT, 0, "Error sending datagram message [%d:%s]\n", errno, strerror(errno));
            }
            close(sock);

            g_slice_free(SocketMsg, msg);
        }
    }

    return NULL;
}

void ThreadForSocket::socketHandle_destroy(SocketHandle *socketHandle)
{
    g_return_if_fail(socketHandle != NULL);
    g_async_queue_push(socketHandle->queue, GINT_TO_POINTER(-1));
    g_thread_join(socketHandle->thread);
    g_async_queue_unref(socketHandle->queue);
    g_free(socketHandle);
}