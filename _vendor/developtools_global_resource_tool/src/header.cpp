/*
 * Copyright (c) 2021-2024 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "header.h"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>

namespace OHOS {
namespace Global {
namespace Restool {
using namespace std;
const std::string Header::LICENSE_HEADER = "/*\n\
 * Copyright (c) 2025 Huawei Device Co., Ltd.\n\
 * Licensed under the Apache License, Version 2.0 (the \"License\");\n\
 * you may not use this file except in compliance with the License.\n\
 * You may obtain a copy of the License at\n\
 *\n\
 *     http://www.apache.org/licenses/LICENSE-2.0\n\
 *\n\
 * Unless required by applicable law or agreed to in writing, software\n\
 * distributed under the License is distributed on an \"AS IS\" BASIS,\n\
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.\n\
 * See the License for the specific language governing permissions and\n\
 * limitations under the License.\n\
 */\n";

Header::Header(const string &outputPath) : outputPath_(outputPath)
{
}

Header::~Header()
{
}

uint32_t Header::Create(HandleHeaderTail headerHandler, HandleBody bodyHandler, HandleHeaderTail tailHandler) const
{
    IdWorker &idWorker = IdWorker::GetInstance();
    vector<ResourceId> resourceIds = idWorker.GetHeaderId();

    ofstream out(outputPath_, ofstream::out | ofstream::binary);
    if (!out.is_open()) {
        PrintError(GetError(ERR_CODE_OPEN_FILE_ERROR).FormatCause(outputPath_.c_str(), strerror(errno)));
        return RESTOOL_ERROR;
    }

    stringstream buffer;
    if (headerHandler) {
        headerHandler(buffer);
    }
    if (bodyHandler) {
        for (const auto &resourceId : resourceIds) {
            bodyHandler(buffer, resourceId);
        }
    }
    if (tailHandler) {
        tailHandler(buffer);
    }
    out << buffer.rdbuf();
    out.close();
    return RESTOOL_SUCCESS;
}
}
}
}