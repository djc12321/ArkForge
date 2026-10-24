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

#ifndef OHOS_RESTOOL_REFERENCE_PARSER_H
#define OHOS_RESTOOL_REFERENCE_PARSER_H

#include <cJSON.h>
#include "id_worker.h"
#include "resource_data.h"
#include "resource_item.h"

namespace OHOS {
namespace Global {
namespace Restool {
class ReferenceParser {
public:
    ReferenceParser();
    virtual ~ReferenceParser();
    uint32_t ParseRefInResources(std::map<int64_t, std::vector<ResourceItem>> &items, const std::string &output);
    uint32_t ParseRefInResourceItem(ResourceItem &resourceItem) const;
    uint32_t ParseRefInJsonFile(ResourceItem &resourceItem, const std::string &output, const bool isIncrement = false);
    uint32_t ParseRefInString(std::string &value, bool &update, const std::string &filePath = "") const;
    static std::map<int64_t, std::set<int64_t>> &GetLayerIconIds();
private:
    bool ParseRefJson(const std::string &from, const std::string &to);
    bool ParseRefResourceItemData(const ResourceItem &resourceItem, std::string &data, bool &update) const;
    bool IsStringOfResourceItem(ResType resType) const;
    bool IsArrayOfResourceItem(ResType resType) const;
    bool IsElementRef(const ResourceItem &resourceItem) const;
    bool IsMediaRef(const ResourceItem &resourceItem) const;
    bool IsProfileRef(const ResourceItem &resourceItem) const;
    bool ParseRefString(std::string &key) const;
    bool ParseRefString(std::string &key, bool &update, const std::string &filePath = "") const;
    bool ParseRefImpl(std::string &key, const std::map<std::string, ResType> &refs, bool isSystem,
        const std::string &filePath = "") const;
    bool ParseRefJsonImpl(cJSON *root, bool &needSave, const std::string &filePath) const;
    const IdWorker &idWorker_;
    static const std::map<std::string, ResType> ID_REFS;
    static const std::map<std::string, ResType> ID_OHOS_REFS;
    static std::map<int64_t, std::set<int64_t>> layerIconIds_;
    cJSON *root_;
    bool isParsingMediaJson_;
    int64_t mediaJsonId_{ INVALID_ID };
};
}
}
}
#endif