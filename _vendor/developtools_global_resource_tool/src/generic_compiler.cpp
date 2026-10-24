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

#include "generic_compiler.h"

#include <iostream>

#include "compression_parser.h"
#include "file_entry.h"
#include "id_worker.h"
#include "resource_util.h"
#include "restool_errors.h"
#include "thread_pool.h"

namespace OHOS {
namespace Global {
namespace Restool {
using namespace std;
GenericCompiler::GenericCompiler(ResType type, const string &output, bool isOverlap, bool isHarResource)
    : IResourceCompiler(type, output, isOverlap, isHarResource)
{
}

GenericCompiler::~GenericCompiler()
{
}

uint32_t GenericCompiler::CompileFiles(const std::vector<FileInfo> &fileInfos)
{
    cout << "Info: GenericCompiler::CompileFiles" << endl;
    std::vector<std::future<uint32_t>> results;
    for (const auto &fileInfo : fileInfos) {
        auto taskFunc = [this](const FileInfo &fileInfo) { return this->CompileSingleFile(fileInfo); };
        results.push_back(ThreadPool::GetInstance().Enqueue(taskFunc, fileInfo));
    }
    for (auto &ret : results) {
        if (ret.get() != RESTOOL_SUCCESS) {
            return RESTOOL_ERROR;
        }
    }
    return RESTOOL_SUCCESS;
}

uint32_t GenericCompiler::CompileSingleFile(const FileInfo &fileInfo)
{
    if (IsIgnore(fileInfo)) {
        return RESTOOL_SUCCESS;
    }

    string output = "";
    if (!CopyMediaFile(fileInfo, output)) {
        return RESTOOL_ERROR;
    }

    if (!PostMediaFile(fileInfo, output)) {
        return RESTOOL_ERROR;
    }
    return RESTOOL_SUCCESS;
}

bool GenericCompiler::PostMediaFile(const FileInfo &fileInfo, const std::string &output)
{
    std::lock_guard<std::mutex> lock(mutex_);
    ResourceItem resourceItem(fileInfo.filename, fileInfo.keyParams, type_);
    resourceItem.SetFilePath(fileInfo.filePath);
    resourceItem.SetLimitKey(fileInfo.limitKey);

    auto index = output.find_last_of(SEPARATOR_FILE);
    if (index == string::npos) {
        PrintError(GetError(ERR_CODE_INVALID_RESOURCE_PATH).FormatCause(output.c_str(), "missing separator"));
        return false;
    }
    string data = output.substr(index + 1);
    data = moduleName_ + SEPARATOR + RESOURCES_DIR + SEPARATOR + \
        fileInfo.limitKey + SEPARATOR + fileInfo.fileCluster + SEPARATOR + data;
    if (!resourceItem.SetData(reinterpret_cast<const int8_t *>(data.c_str()), data.length())) {
        std::string msg = "item data is null, resource name: " + resourceItem.GetName();
        PrintError(GetError(ERR_CODE_UNDEFINED_ERROR).FormatCause(msg.c_str()).SetPosition(fileInfo.filePath));
        return false;
    }

    if (isOverlap_) {
        resourceItem.MarkCoverable();
    }

    return MergeResourceItem(resourceItem);
}

string GenericCompiler::GetOutputFilePath(const FileInfo &fileInfo) const
{
    string outputFolder = GetOutputFolder(fileInfo);
    string outputFilePath = FileEntry::FilePath(outputFolder).Append(fileInfo.filename).GetPath();
    return outputFilePath;
}

bool GenericCompiler::IsIgnore(const FileInfo &fileInfo)
{
    lock_guard<mutex> lock(mutex_);
    string output = GetOutputFilePath(fileInfo);
    if (g_hapResourceSet.count(output)) {
        g_hapResourceSet.erase(output);
    } else if (!g_resourceSet.emplace(output).second) {
        if (isHarResource_) {
            string idName = ResourceUtil::GetIdName(fileInfo.filename, fileInfo.dirType);
            int64_t id = IdWorker::GetInstance().GetId(fileInfo.dirType, idName);
            ResourceUtil::AddHarResourceId(id);
        }
        cout << "Warning: '" << fileInfo.filePath << "' is defined repeatedly." << endl;
        return true;
    }
    return false;
}

bool GenericCompiler::CopyMediaFile(const FileInfo &fileInfo, std::string &output)
{
    string outputFolder = GetOutputFolder(fileInfo);
    if (!ResourceUtil::CreateDirs(outputFolder)) {
        return false;
    }
    output = GetOutputFilePath(fileInfo);
    if (moduleName_ == "har" || type_ != ResType::MEDIA) {
        return ResourceUtil::CopyFileInner(fileInfo.filePath, output);
    } else {
        return CompressionParser::GetCompressionParser()->CopyAndTranscode(fileInfo.filePath, output);
    }
}
}
}
}