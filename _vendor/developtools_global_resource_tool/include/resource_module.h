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

#ifndef OHOS_RESTOOL_RESOURCE_MODULE_H
#define OHOS_RESTOOL_RESOURCE_MODULE_H

#include "resource_item.h"
#include "resource_directory.h"
#include "resource_util.h"

namespace OHOS {
namespace Global {
namespace Restool {
class ResourceModule {
public:
    ResourceModule(const std::string &modulePath, const std::string &moduleOutput, const std::string &moduleName);
    virtual ~ResourceModule() {};
    uint32_t ScanResource(bool isHap = false);
    const std::map<int64_t, std::vector<ResourceItem>> &GetOwner() const;
    const std::map<ResType, std::vector<DirectoryInfo>> &GetScanDirectorys() const;
    static uint32_t MergeResourceItem(std::map<int64_t, std::vector<ResourceItem>> &alls,
        const std::map<int64_t, std::vector<ResourceItem>> &other, bool tipError = false);

protected:
    const std::string &modulePath_;
    const std::string &moduleOutput_;
    const std::string &moduleName_;
    std::map<int64_t, std::vector<ResourceItem>> owner_;
    std::map<ResType, std::vector<DirectoryInfo>> scanDirs_;
private:
    void Push(const std::map<int64_t, std::vector<ResourceItem>> &other);
    static const std::vector<ResType> SCAN_SEQ;
    bool isHarResource_ = false;
};
}
}
}
#endif