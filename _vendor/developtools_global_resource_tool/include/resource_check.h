/*
 * Copyright (c) 2023-2024 Huawei Device Co., Ltd.
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

#ifndef OHOS_RESTOOL_RESOURCE_CHECK_H
#define OHOS_RESTOOL_RESOURCE_CHECK_H

#include "config_parser.h"
#include "resource_append.h"
#include "resource_item.h"
#include <iostream>
#include <stdint.h>
#include <map>
#include <set>

namespace OHOS {
namespace Global {
namespace Restool {

class ResourceCheck {
public:
    ResourceCheck(const std::map<std::string, std::set<uint32_t>> &jsonCheckIds,
        const std::shared_ptr<ResourceAppend> &resourceAppend = nullptr);
    virtual ~ResourceCheck() {};
    void CheckConfigJson();
    void CheckConfigJsonForCombine();

private:
    const std::map<std::string, std::set<uint32_t>> jsonCheckIds_;
    const std::shared_ptr<ResourceAppend> resourceAppend_;
    void CheckNodeInResourceItem(const std::string &key, const ResourceItem &resourceItem);
    bool GetPngWidthAndHeight(const std::string &filePath, uint32_t *width, uint32_t *height);
    bool IsValidPngImage(FILE *&in) const;
    void CloseFile(FILE *fp);
};

}
}
}
#endif