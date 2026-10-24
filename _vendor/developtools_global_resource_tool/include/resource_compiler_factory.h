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

#ifndef OHOS_RESTOOL_FACTORY_RESOURCE_COMPILER_H
#define OHOS_RESTOOL_FACTORY_RESOURCE_COMPILER_H

#include <memory>
#include "i_resource_compiler.h"

namespace OHOS {
namespace Global {
namespace Restool {
class ResourceCompilerFactory {
public:
    static std::unique_ptr<IResourceCompiler> CreateCompiler(ResType type, const std::string &output, bool isHap,
        bool isHarResource = false);
    static std::unique_ptr<IResourceCompiler> CreateCompilerForAppend(ResType type, const std::string &output);
};
}
}
}
#endif