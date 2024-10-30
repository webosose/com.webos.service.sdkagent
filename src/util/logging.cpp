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

//=================================================================================================
// Includes & Definitions
//=================================================================================================

#include <stdio.h>
#include <glib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "logging.h"

//=================================================================================================
// Variables
//=================================================================================================

//=================================================================================================
// Functions
//=================================================================================================
void _good_assert(const char *cond_str, bool cond)
{
    if (G_UNLIKELY(!(cond)))
    {
        SDK_LOG_CRITICAL("ASSERTION_FAIL", 1, PMLOGKS("CAUSE", cond_str), "");
        *(int *)0x00 = 0;
    }
}

PmLogContext getServiceContext()
{
    static PmLogContext logContext = 0;

    if (0 == logContext)
    {
        PmLogGetContext("com.webos.service.sdkagent", &logContext);
    }

    return logContext;
}

#define LOG_BUF_MAX 512
void SDK_KMSG_DEBUG_MSG(const char* format, const char* functionName) {
    char buf[LOG_BUF_MAX];
    sprintf(buf, format, functionName);

    int log_fd;
    log_fd = open("/dev/kmsg", O_WRONLY);
    if (log_fd < 0) {
        perror("Error opening /dev/kmsg to write SDK_AGENT log message");
        return;
    }

    ssize_t bytes_written = write(log_fd, buf, strlen(buf));
    if (bytes_written == -1) {
        perror("Error writing to file");
    }
    close(log_fd);
}