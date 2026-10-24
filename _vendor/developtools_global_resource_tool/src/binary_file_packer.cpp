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

#include "binary_file_packer.h"

#include "compression_parser.h"
#include "restool_errors.h"

namespace OHOS {
namespace Global {
namespace Restool {
using namespace std;

BinaryFilePacker::BinaryFilePacker(const PackageParser &packageParser, const std::string &moduleName)
    : packageParser_(packageParser), moduleName_(moduleName)
{
}

BinaryFilePacker::~BinaryFilePacker()
{
}

void BinaryFilePacker::Terminate()
{
    terminate_.store(true);
    GetResult();
}

uint32_t BinaryFilePacker::GetResult()
{
    if (copyFuture_.valid()) {
        result_ = copyFuture_.get();
    }
    return result_;
}

void BinaryFilePacker::CopyBinaryFileAsync(const std::vector<std::string> &inputs)
{
    auto func = [this](const vector<string> &inputs) { return this->CopyBinaryFile(inputs); };
    copyFuture_ = ThreadPool::GetInstance().Enqueue(func, inputs);
}

uint32_t BinaryFilePacker::CopyBinaryFile(const vector<string> &inputs)
{
    for (const auto &input : inputs) {
        CopyBinaryFile(input);
    }
    if (CheckCopyResults() != RESTOOL_SUCCESS) {
        return RESTOOL_ERROR;
    }
    return RESTOOL_SUCCESS;
}

uint32_t BinaryFilePacker::CopyBinaryFile(const string &input)
{
    string rawfilePath = FileEntry::FilePath(input).Append(RAW_FILE_DIR).GetPath();
    if (CopyBinaryFile(rawfilePath, RAW_FILE_DIR) == RESTOOL_ERROR) {
        return RESTOOL_ERROR;
    }
    string resfilePath = FileEntry::FilePath(input).Append(RES_FILE_DIR).GetPath();
    if (CopyBinaryFile(resfilePath, RES_FILE_DIR) == RESTOOL_ERROR) {
        return RESTOOL_ERROR;
    }
    return RESTOOL_SUCCESS;
}

uint32_t BinaryFilePacker::CopyBinaryFile(const string &filePath, const string &fileType)
{
    if (!ResourceUtil::FileExist(filePath)) {
        return RESTOOL_SUCCESS;
    }

    if (!FileEntry::IsDirectory(filePath)) {
        PrintError(GetError(ERR_CODE_INVALID_RESOURCE_PATH).FormatCause(filePath.c_str(), "not a directory"));
        return RESTOOL_ERROR;
    }
    if (ResourceUtil::IsIgnoreFile(FileEntry(filePath))) {
        return RESTOOL_SUCCESS;
    }

    string dst = FileEntry::FilePath(packageParser_.GetOutput()).Append(RESOURCES_DIR).Append(fileType).GetPath();
    if (CopyBinaryFileImpl(filePath, dst) != RESTOOL_SUCCESS) {
        return RESTOOL_ERROR;
    }
    return RESTOOL_SUCCESS;
}

uint32_t BinaryFilePacker::CopyBinaryFileImpl(const string &src, const string &dst)
{
    if (!ResourceUtil::CreateDirs(dst)) {
        return RESTOOL_ERROR;
    }

    FileEntry f(src);
    if (!f.Init()) {
        return RESTOOL_ERROR;
    }
    for (const auto &entry : f.GetChilds()) {
        string filename = entry->GetFilePath().GetFilename();
        if (ResourceUtil::IsIgnoreFile(*entry)) {
            continue;
        }

        string subPath = FileEntry::FilePath(dst).Append(filename).GetPath();
        if (!entry->IsFile()) {
            if (CopyBinaryFileImpl(entry->GetFilePath().GetPath(), subPath) != RESTOOL_SUCCESS) {
                return RESTOOL_ERROR;
            }
            continue;
        }

        if (IsDuplicated(entry, subPath)) {
            continue;
        }

        if (terminate_.load()) {
            cout << "Info: CopyBinaryFileImpl: stop copy binary file." << endl;
            return RESTOOL_ERROR;
        }

        string path = entry->GetFilePath().GetPath();
        auto copyFunc = [this](const string path, string subPath) { return this->CopySingleFile(path, subPath); };
        std::future<uint32_t> res = ThreadPool::GetInstance().Enqueue(copyFunc, path, subPath);
        copyResults_.push_back(std::move(res));
    }
    return RESTOOL_SUCCESS;
}

bool BinaryFilePacker::IsDuplicated(const unique_ptr<FileEntry> &entry, string subPath)
{
    lock_guard<mutex> lock(mutex_);
    if (g_hapResourceSet.count(subPath)) {
        g_hapResourceSet.erase(subPath);
    } else if (!g_resourceSet.emplace(subPath).second) {
        cout << "Warning: '" << entry->GetFilePath().GetPath() << "' is defined repeatedly." << endl;
        return true;
    }
    return false;
}

uint32_t BinaryFilePacker::CopySingleFile(const std::string &path, std::string &subPath)
{
    if (terminate_.load()) {
        cout << "Info: CopySingleFile: stop copy binary file." << endl;
        return RESTOOL_ERROR;
    }
    if (moduleName_ == "har" || CompressionParser::GetCompressionParser()->GetDefaultCompress()) {
        if (!ResourceUtil::CopyFileInner(path, subPath)) {
            return RESTOOL_ERROR;
        }
        return RESTOOL_SUCCESS;
    }
    if (!CompressionParser::GetCompressionParser()->CopyAndTranscode(path, subPath, true)) {
        return RESTOOL_ERROR;
    }
    return RESTOOL_SUCCESS;
}

uint32_t BinaryFilePacker::CheckCopyResults()
{
    for (auto &res : copyResults_) {
        if (terminate_.load()) {
            cout << "Info: CopyBinaryFile: stop copy binary file." << endl;
            return RESTOOL_ERROR;
        }
        uint32_t ret = res.get();
        if (ret != RESTOOL_SUCCESS) {
            return RESTOOL_ERROR;
        }
    }
    return RESTOOL_SUCCESS;
}
} // namespace Restool
} // namespace Global
} // namespace OHOS
