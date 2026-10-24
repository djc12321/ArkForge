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

#include "resource_compiler_factory.h"
#include "append_compiler.h"
#include "generic_compiler.h"
#include "json_compiler.h"
#include "overlap_compiler.h"

namespace OHOS {
namespace Global {
namespace Restool {
using namespace std;
unique_ptr<IResourceCompiler> ResourceCompilerFactory::CreateCompiler(ResType type,
    const string &output, bool isOverlap, bool isHarResource)
{
    if (type == ResType::ELEMENT) {
        return make_unique<JsonCompiler>(type, output, isOverlap, isHarResource);
    } else if (isOverlap) {
        return make_unique<OverlapCompiler>(type, output, isOverlap, isHarResource);
    } else {
        return make_unique<GenericCompiler>(type, output, isOverlap, isHarResource);
    }
}

unique_ptr<IResourceCompiler> ResourceCompilerFactory::CreateCompilerForAppend(ResType type, const string &output)
{
    if (type == ResType::ELEMENT) {
        return make_unique<JsonCompiler>(type, output);
    } else if (type == ResType::MEDIA || type == ResType::PROF) {
        return make_unique<AppendCompiler>(type, output);
    } else {
        return nullptr;
    }
}
}
}
}