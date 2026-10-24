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

#include "resource_packer_factory.h"
#include "resource_overlap.h"

namespace OHOS {
namespace Global {
namespace Restool {
using namespace std;

unique_ptr<ResourcePack> ResourcePackerFactory::CreatePacker(PackType type, const PackageParser &packageParser)
{
    if (type == PackType::NORMAL) {
        return make_unique<ResourcePack>(packageParser);
    } else if (type == PackType::OVERLAP) {
        return make_unique<ResourceOverlap>(packageParser);
    } else {
        cout << "Warning: ResourcePackerFactory: Unknown input PackType." << endl;
        return nullptr;
    }
}
}
}
}