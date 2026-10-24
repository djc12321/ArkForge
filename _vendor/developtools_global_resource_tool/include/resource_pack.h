/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#ifndef OHOS_RESTOOL_RESOURCE_PACK_H
#define OHOS_RESTOOL_RESOURCE_PACK_H

#include "config_parser.h"
#include "resource_append.h"
#include "resource_data.h"
#include "resource_item.h"
#include "resource_merge.h"
#include "resource_util.h"

namespace OHOS {
namespace Global {
namespace Restool {
class ResourcePack {
public:
    explicit ResourcePack(const PackageParser &packageParser);
    virtual ~ResourcePack() = default;
    uint32_t Package();
    virtual uint32_t Pack();

protected:
    uint32_t InitResourcePack();
    uint32_t PackResources(const ResourceMerge &resourceMerge);
    virtual uint32_t ScanResources(const std::vector<std::string> &inputs, const std::string &output);
    PackageParser packageParser_;
    std::string moduleName_;
    ConfigParser configJson_;

private:
    uint32_t InitModule();
    void InitHeaderCreater();
    uint32_t InitOutput() const;
    uint32_t InitCompression();
    uint32_t GenerateHeader() const;
    uint32_t InitConfigJson();
    uint32_t GenerateTextHeader(const std::string &headerPath) const;
    uint32_t GenerateCplusHeader(const std::string &headerPath) const;
    uint32_t GenerateJsHeader(const std::string &headerPath) const;
    uint32_t GenerateTsHeader(const std::string &headerPath) const;
    uint32_t GenerateConfigJson();
    uint32_t PackPreview();
    uint32_t PackAppend();
    uint32_t PackCombine();
    uint32_t HandleFeature();
    uint32_t FindResourceItems(const std::map<int64_t, std::vector<ResourceItem>> &resInfoLocal,
                               std::vector<ResourceItem> &items, int64_t id) const;
    uint32_t HandleLabel(std::vector<ResourceItem> &items, ConfigParser &config) const;
    uint32_t HandleIcon(std::vector<ResourceItem> &items, ConfigParser &config) const;
    void SaveResourceItem(const ResourceItem &resourceItem, int64_t nextId) const;
    void CheckConfigJson();
    void CheckConfigJsonForCombine(ResourceAppend &resourceAppend);
    bool CopyIcon(std::string &dataPath, const std::string &idName, std::string &fileName) const;
    void ShowPackSuccess();
    
    using HeaderCreater = std::function<uint32_t(const std::string&)>;
    std::map<std::string, HeaderCreater> headerCreaters_;
    PackType packType_ = PackType::NORMAL;
};
}
}
}
#endif