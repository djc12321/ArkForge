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

#ifndef OHOS_TRANSLATABLE_H
#define OHOS_TRANSLATABLE_H

#include <cJSON.h>
#include "resource_data.h"

namespace OHOS {
namespace Global {
namespace Restool {
class TranslatableParse {
public:
    static bool ParseTranslatable(cJSON *objectNode, const FileInfo &fileInfo, const std::string &name);
private:
    static bool ParseTranslatable(cJSON *objectNode, const std::string &filePath);
    static bool CheckBaseStringAttr(const cJSON *objectNode, const std::string &filePath);
    static bool CheckBaseStringTranslatable(const cJSON *attrNode, const std::string &filePath);
    static bool CheckBaseStringPriority(const cJSON *attrNode, const std::string &filePath);
    static bool ReplaceTranslateTags(cJSON *node, const char *key, const std::string &filePath);
    static bool GetReplaceStringTranslate(std::string &str);
    static bool FindTranslatePairs(const std::string &str, std::vector<size_t> &posData);
};
}
}
}
#endif