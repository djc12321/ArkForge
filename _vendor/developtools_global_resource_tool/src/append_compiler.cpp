/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include "append_compiler.h"
#include <iostream>
#include "restool_errors.h"

namespace OHOS {
namespace Global {
namespace Restool {
using namespace std;
AppendCompiler::AppendCompiler(ResType type, const string &output) : IResourceCompiler(type, output)
{
}

uint32_t AppendCompiler::CompileSingleFile(const FileInfo &fileInfo)
{
    ResourceItem resourceItem(fileInfo.filename, fileInfo.keyParams, type_);
    resourceItem.SetFilePath(fileInfo.filePath);
    resourceItem.SetLimitKey(fileInfo.limitKey);

    string data = fileInfo.filePath;
    if (!resourceItem.SetData(reinterpret_cast<const int8_t *>(data.c_str()), data.length())) {
        std::string msg = "item data is null, resource name: " + resourceItem.GetName();
        PrintError(GetError(ERR_CODE_UNDEFINED_ERROR).FormatCause(msg.c_str()).SetPosition(fileInfo.filePath));
        return RESTOOL_ERROR;
    }

    if (!MergeResourceItem(resourceItem)) {
        return RESTOOL_ERROR;
    }
    return RESTOOL_SUCCESS;
}
}
}
}