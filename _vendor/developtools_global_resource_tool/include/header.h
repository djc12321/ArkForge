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

#ifndef OHOS_RESTOOL_HEADER_H
#define OHOS_RESTOOL_HEADER_H

#include <functional>
#include <cstring>
#include <string>
#include <sstream>
#include "id_worker.h"
#include "restool_errors.h"

namespace OHOS {
namespace Global {
namespace Restool {
class Header {
public:
    static const std::string LICENSE_HEADER;
    Header(const std::string &outputPath);
    virtual ~Header();
    using HandleHeaderTail = std::function<void(std::stringstream&)>;
    using HandleBody = std::function<void(std::stringstream&, const ResourceId&)>;
    uint32_t Create(HandleHeaderTail headerHandler, HandleBody bodyHander, HandleHeaderTail tailHander) const;
private:
    const std::string &outputPath_;
};
}
}
}
#endif