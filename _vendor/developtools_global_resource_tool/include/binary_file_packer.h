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

#ifndef OHOS_RESTOOL_BINARY_FILE_PACKER_H
#define OHOS_RESTOOL_BINARY_FILE_PACKER_H

#include "cmd/package_parser.h"
#include "resource_util.h"
#include "thread_pool.h"

namespace OHOS {
namespace Global {
namespace Restool {
class BinaryFilePacker {
public:
    explicit BinaryFilePacker(const PackageParser &packageParser, const std::string &moduleName);
    virtual ~BinaryFilePacker();
    void CopyBinaryFileAsync(const std::vector<std::string> &inputs);
    void Terminate();
    uint32_t GetResult();

protected:
    virtual uint32_t CopyBinaryFile(const std::vector<std::string> &inputs);
    uint32_t CheckCopyResults();
    uint32_t CopyBinaryFile(const std::string &input);
    virtual bool IsDuplicated(const std::unique_ptr<FileEntry> &entry, std::string subPath);
    PackageParser packageParser_;
    std::string moduleName_;
    std::mutex mutex_;

private:
    uint32_t CopyBinaryFile(const std::string &filePath, const std::string &fileType);
    uint32_t CopyBinaryFileImpl(const std::string &src, const std::string &dst);
    uint32_t CopySingleFile(const std::string &path, std::string &subPath);
    std::future<uint32_t> copyFuture_;
    std::vector<std::future<uint32_t>> copyResults_;
    std::atomic<bool> terminate_{false};
    uint32_t result_ = RESTOOL_SUCCESS;
};
} // namespace Restool
} // namespace Global
} // namespace OHOS
#endif
