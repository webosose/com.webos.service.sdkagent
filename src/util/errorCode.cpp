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

#include "errorCode.h"

const char* getErrorMessage(SDKError ec) {

    switch (ec)
    {
    case SDKError::UNKNOWN_ERROR:
        return "{\"returnValue\":false,\"errorCode\":1,\"errorText\":\"Unknown error.\"}";
        break;
    
    case SDKError::INVALID_PARAMETERS:
        return "{\"returnValue\":false,\"errorCode\":2,\"errorText\":\"Invalid parameters.\"}";
        break;

    case SDKError::MALFORMED_JSON:
        return "{\"returnValue\":false,\"errorCode\":3,\"errorText\":\"Malformed json.\"}";
        break;

    case SDKError::INVALID_CONFIGURATIONS:
        return "{\"returnValue\":false,\"errorCode\":4,\"errorText\":\"Invalid configurations.\"}";
        break;

    case SDKError::COLLECTOR_IS_RUNNING:
        return "{\"returnValue\":false,\"errorCode\":5,\"errorText\":\"Collector is active (running).\"}";
        break;

    case SDKError::DEVMODE_DISABLE:
        return "{\"returnValue\":false,\"errorCode\":6,\"errorText\":\"The developer mode must be activated in order to monitor performance.\"}";
        break;
    
    default:
        return "{\"returnValue\":true,\"errorCode\":0,\"errorText\":\"Success.\"}";
        break;
    }
}