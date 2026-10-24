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

#ifndef OHOS_RESTOOL_RESOURCE_ITEM_H
#define OHOS_RESTOOL_RESOURCE_ITEM_H

#include <vector>
#include "resource_data.h"

namespace OHOS {
namespace Global {
namespace Restool {
class ResourceItem {
public:
    ResourceItem();
    ResourceItem(const ResourceItem &other);
    ResourceItem(const std::string &name, const std::vector<KeyParam> &keyparams, ResType type);
    virtual ~ResourceItem();

    bool SetData(const std::string &data);
    bool SetData(const int8_t *data, uint32_t length);
    void SetFilePath(const std::string &filePath);
    void SetLimitKey(const std::string &limitKey);
    void SetName(const std::string &name);
    void MarkCoverable();

    const int8_t *GetData() const;
    uint32_t GetDataLength() const;
    const std::string &GetName() const;
    const ResType &GetResType() const;
    const std::vector<KeyParam> &GetKeyParam() const;
    const std::string &GetFilePath() const;
    const std::string &GetLimitKey() const;
    bool IsCoverable() const;
    const std::vector<std::string> SplitValue() const;
    bool IsArray() const;
    bool IsPair() const;
    void CheckData();

    ResourceItem &operator=(const ResourceItem &other);
private:
    void ReleaseData();
    void CopyFrom(const ResourceItem &other);
    int8_t *data_ = nullptr;
    uint32_t dataLen_ = 0;
    std::string name_;
    std::vector<KeyParam> keyparams_;
    ResType type_;
    std::string filePath_;
    std::string limitKey_;
    bool coverable_ = false;
};
}
}
}
#endif