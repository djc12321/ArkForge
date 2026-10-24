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

#ifndef OHOS_RESTOOL_ID_WORKER_H
#define OHOS_RESTOOL_ID_WORKER_H

#include <vector>
#include "id_defined_parser.h"
#include "resource_data.h"
#include "resource_item.h"
#include "resource_util.h"
#include "singleton.h"

namespace OHOS {
namespace Global {
namespace Restool {
class IdWorker : public Singleton<IdWorker> {
public:
    uint32_t Init(ResourceIdCluster &type, int64_t start = 0x01000000);
    int64_t GenerateId(ResType resType, const std::string &name);
    std::vector<ResourceId> GetHeaderId() const;
    int64_t GetId(ResType resType, const std::string &name) const;
    int64_t GetSystemId(ResType resType, const std::string &name) const;
    int64_t LoadIdFromHap(std::map<int64_t, std::vector<ResourceItem>> &items);

private:
    int64_t GenerateAppId(ResType resType, const std::string &name);
    int64_t GenerateSysId(ResType resType, const std::string &name);
    uint64_t GetMaxId(uint64_t startId) const;
    int64_t GetCurId();
    uint64_t appId_;
    uint64_t maxId_;
    ResourceIdCluster type_;
    std::map<std::pair<ResType, std::string>, int64_t> ids_;
    std::map<std::pair<ResType, std::string>, ResourceId> sysDefinedIds_;
    std::map<std::pair<ResType, std::string>, ResourceId> appDefinedIds_;
    std::vector<int64_t> delIds_;
    std::map<std::pair<ResType, std::string>, int64_t> cacheIds_;
};
}
}
}
#endif
