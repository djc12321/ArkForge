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

#ifndef OHOS_RESTOOL_RESOURCE_MERGE_H
#define OHOS_RESTOOL_RESOURCE_MERGE_H

#include <string>
#include <vector>
#include "config_parser.h"
#include "restool_errors.h"
#include "cmd/cmd_parser.h"

namespace OHOS {
namespace Global {
namespace Restool {
class ResourceMerge {
public:
    ResourceMerge();
    virtual ~ResourceMerge();
    uint32_t Init(const PackageParser &packageParser = CmdParser::GetInstance().GetPackageParser());
    const std::vector<std::string> &GetInputs() const;
private:
    std::vector<std::string> inputsOrder_;
    static const std::vector<ConfigParser::ModuleType> ORDERS;
};
}
}
}
#endif