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

#ifndef OHOS_RESTOOL_GENERIC_COMPILER_H
#define OHOS_RESTOOL_GENERIC_COMPILER_H

#include "i_resource_compiler.h"
#include <mutex>

namespace OHOS {
namespace Global {
namespace Restool {
class GenericCompiler : public IResourceCompiler {
public:
    GenericCompiler(ResType type, const std::string &output, bool isOverlap = false, bool isHarResource = false);
    virtual ~GenericCompiler();

protected:
    std::string GetOutputFilePath(const FileInfo &fileInfo) const;
    virtual bool IsIgnore(const FileInfo &fileInfo);
    uint32_t CompileSingleFile(const FileInfo &fileInfo) override;
    uint32_t CompileFiles(const std::vector<FileInfo> &fileInfos) override;
    bool PostMediaFile(const FileInfo &fileInfo, const std::string &output);
    std::mutex mutex_;

private:
    bool CopyMediaFile(const FileInfo &fileInfo, std::string &output);
};
}
}
}
#endif