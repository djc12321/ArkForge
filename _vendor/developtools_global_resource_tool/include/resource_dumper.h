/*
 * Copyright (c) 2024-2024 Huawei Device Co., Ltd.
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

#ifndef OHOS_RESTOOL_RESOURCE_DUMP_H
#define OHOS_RESTOOL_RESOURCE_DUMP_H

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <vector>
#include "config_parser.h"
#include "unzip.h"
#include "cJSON.h"
#include "cmd/dump_parser.h"
#include "resource_data.h"
#include "resource_item.h"


namespace OHOS {
namespace Global {
namespace Restool {
class ResourceDumper {
public:
    virtual ~ResourceDumper() = default;
    virtual uint32_t Dump(const DumpParserBase &parser);
protected:
    virtual uint32_t DumpRes(std::string &out) const = 0;
    void ReadHapInfo(const std::unique_ptr<char[]> &buffer, size_t len);
    uint32_t LoadHap();
    uint32_t ReadFileFromZip(unzFile &zip, const char *fileName, std::unique_ptr<char[]> &buffer, size_t &len);

    std::string inputPath_;
    std::string bundleName_;
    std::string moduleName_;
    std::map<int64_t, std::vector<ResourceItem>> resInfos_;
};

class ConfigDumper : public ResourceDumper {
public:
    virtual ~ConfigDumper() = default;
    uint32_t DumpRes(std::string &out) const override;
};


class CommonDumper : public ResourceDumper {
public:
    virtual ~CommonDumper() = default;
    uint32_t DumpRes(std::string &out) const override;

private:
    uint32_t AddValueToJson(const ResourceItem &item, cJSON *json) const;
    uint32_t AddPairVauleToJson(const ResourceItem &item, cJSON *json) const;
    uint32_t AddKeyParamsToJson(const std::vector<KeyParam> &keyParams, cJSON *json) const;
    uint32_t AddResourceToJson(int64_t id, const std::vector<ResourceItem> &items, cJSON *json) const;
    uint32_t AddItemCommonPropToJson(int64_t resId, const ResourceItem &item, cJSON* json) const;
};

} // namespace Restool
} // namespace Global
} // namespace OHOS

#endif