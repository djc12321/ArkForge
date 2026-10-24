/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#ifndef OHOS_RESTOOL_RES_CONFIG_PARSER_H
#define OHOS_RESTOOL_RES_CONFIG_PARSER_H

#include <functional>
#include <string>
#include <cJSON.h>
#include "resource_util.h"

namespace OHOS {
namespace Global {
namespace Restool {
class ResConfigParser {
public:
    ResConfigParser();
    virtual ~ResConfigParser();
    using HandleBack = std::function<uint32_t(int c, const std::string &argValue)>;
    uint32_t Init(const std::string &filePath, HandleBack callback);
private:
    void InitFileListCommand(HandleBack callback);
    using HandleFileListValue = std::function<uint32_t(const cJSON *)>;
    std::map<std::string, HandleFileListValue> fileListHandles_;
    uint32_t GetString(const std::string &nodeName, const cJSON *node, int c, HandleBack callback);
    uint32_t GetArray(const std::string &nodeName, const cJSON *node, int c, HandleBack callback);
    uint32_t GetModuleNames(const cJSON *node, int c, HandleBack callback);
    uint32_t GetBool(const std::string &nodeName, const cJSON *node, int c, HandleBack callback);
    uint32_t GetNumber(const std::string &nodeName, const cJSON *node, int c, HandleBack callback);
    uint32_t GetIgnorePatterns(const std::string &nodeName, const cJSON *node, int c);
    uint32_t GetQualifiersConfig(const std::string &nodeName, const cJSON *node, int c);
    cJSON *root_;
    std::string filePath_;
};
}
}
}
#endif