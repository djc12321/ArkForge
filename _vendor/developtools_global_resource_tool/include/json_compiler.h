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

#ifndef OHOS_RESTOOL_JSON_COMPILER_H
#define OHOS_RESTOOL_JSON_COMPILER_H

#include <functional>
#include <cJSON.h>
#include "i_resource_compiler.h"
#include "resource_util.h"

namespace OHOS {
namespace Global {
namespace Restool {
class JsonCompiler : public IResourceCompiler {
public:
    JsonCompiler(ResType type, const std::string &output, bool isOverlap = false, bool isHarResource = false);
    virtual ~JsonCompiler();
protected:
    uint32_t CompileSingleFile(const FileInfo &fileInfo) override;
private:
    void InitParser();
    bool ParseJsonArrayLevel(const cJSON *arrayNode, const FileInfo &fileInfo);
    bool ParseJsonObjectLevel(cJSON *objectNode, const FileInfo &fileInfo);

    using HandleResource = std::function<bool(const cJSON *, ResourceItem&)>;
    bool HandleString(const cJSON *objectNode, ResourceItem &resourceItem) const;
    bool HandleInteger(const cJSON *objectNode, ResourceItem &resourceItem) const;
    bool HandleBoolean(const cJSON *objectNode, ResourceItem &resourceItem) const;
    bool HandleColor(const cJSON *objectNode, ResourceItem &resourceItem) const;
    bool HandleFloat(const cJSON *objectNode, ResourceItem &resourceItem) const;
    bool HandleStringArray(const cJSON *objectNode, ResourceItem &resourceItem) const;
    bool HandleIntegerArray(const cJSON *objectNode, ResourceItem &resourceItem) const;
    bool HandleTheme(const cJSON *objectNode, ResourceItem &resourceItem) const;
    bool HandlePattern(const cJSON *objectNode, ResourceItem &resourceItem) const;
    bool HandlePlural(const cJSON *objectNode, ResourceItem &resourceItem) const;
    bool HandleSymbol(const cJSON *objectNode, ResourceItem &resourceItem) const;

    bool PushString(const std::string &value, ResourceItem &resourceItem) const;
    bool CheckJsonStringValue(const cJSON *valueNode, const ResourceItem &resourceItem) const;
    bool CheckJsonIntegerValue(const cJSON *valueNode, const ResourceItem &resourceItem) const;
    bool CheckJsonSymbolValue(const cJSON *valueNode, const ResourceItem &resourceItem) const;
    using HandleValue = std::function<bool(const cJSON *, const ResourceItem&, std::vector<std::string>&)>;
    bool ParseValueArray(const cJSON *objectNode, ResourceItem &resourceItem,
                         const std::vector<std::string> &extra, HandleValue callback) const;
    bool ParseParent(const cJSON *objectNode, const ResourceItem &resourceItem,
                     std::vector<std::string> &extra) const;
    bool ParseAttribute(const cJSON *arrayItem, const ResourceItem &resourceItem,
                        std::vector<std::string> &values) const;
    bool CheckPluralValue(const cJSON *arrayItem, const ResourceItem &resourceItem) const;
    bool CheckColorValue(const char *s) const;
    std::map<ResType, HandleResource> handles_;
    bool isBaseString_;
    cJSON *root_;
};
}
}
}
#endif