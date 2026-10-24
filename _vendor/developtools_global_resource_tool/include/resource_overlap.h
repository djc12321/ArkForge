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

#ifndef OHOS_RESTOOL_RESOURCE_OVERLAP_H
#define OHOS_RESTOOL_RESOURCE_OVERLAP_H

#include "cmd/package_parser.h"
#include "resource_pack.h"

namespace OHOS {
namespace Global {
namespace Restool {
class ResourceOverlap : public ResourcePack {
public:
    ResourceOverlap(const PackageParser &packageParser);
    uint32_t Pack();

protected:
    uint32_t ScanResources(const std::vector<std::string> &inputs, const std::string &output);

private:
    uint32_t LoadHapResources();
};
}
}
}
#endif