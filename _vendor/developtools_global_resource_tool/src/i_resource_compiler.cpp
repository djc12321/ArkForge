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

#include "i_resource_compiler.h"
#include <algorithm>
#include <iostream>
#include "file_entry.h"
#include "id_worker.h"
#include "resource_util.h"
#include "restool_errors.h"

namespace OHOS {
namespace Global {
namespace Restool {
using namespace std;
IResourceCompiler::IResourceCompiler(ResType type, const string &output, bool isOverlap, bool isHarResource)
    :type_(type), output_(output), isOverlap_(isOverlap), isHarResource_(isHarResource)
{
}

IResourceCompiler::~IResourceCompiler()
{
    nameInfos_.clear();
    resourceInfos_.clear();
}

uint32_t IResourceCompiler::Compile(const vector<DirectoryInfo> &directoryInfos)
{
    vector<FileInfo> fileInfos;
    map<string, vector<FileInfo>> setsByDirectory;
    for (const auto &directoryInfo : directoryInfos) {
        string outputFolder = GetOutputFolder(directoryInfo);
        FileEntry f(directoryInfo.dirPath);
        if (!f.Init()) {
            return RESTOOL_ERROR;
        }
        for (const auto &it : f.GetChilds()) {
            if (ResourceUtil::IsIgnoreFile(*it)) {
                continue;
            }

            if (!it->IsFile()) {
                PrintError(GetError(ERR_CODE_INVALID_RESOURCE_PATH)
                    .FormatCause(it->GetFilePath().GetPath().c_str(), "not a file"));
                return RESTOOL_ERROR;
            }

            FileInfo fileInfo = { directoryInfo, it->GetFilePath().GetPath(), it->GetFilePath().GetFilename() };
            fileInfos.push_back(fileInfo);
            setsByDirectory[outputFolder].push_back(fileInfo);
        }
    }

    sort(fileInfos.begin(), fileInfos.end(), [](const auto &a, const auto &b) {
        return a.filePath < b.filePath;
    });
    if (CompileFiles(fileInfos) != RESTOOL_SUCCESS) {
        return RESTOOL_ERROR;
    }
    return PostCommit();
}

uint32_t IResourceCompiler::CompileFiles(const std::vector<FileInfo> &fileInfos)
{
    for (const auto &fileInfo : fileInfos) {
        if (CompileSingleFile(fileInfo) != RESTOOL_SUCCESS) {
            return RESTOOL_ERROR;
        }
    }
    return RESTOOL_SUCCESS;
}

const map<int64_t, vector<ResourceItem>> &IResourceCompiler::GetResult() const
{
    return resourceInfos_;
}

uint32_t IResourceCompiler::Compile(const FileInfo &fileInfo)
{
    if (CompileSingleFile(fileInfo) != RESTOOL_SUCCESS) {
        return RESTOOL_ERROR;
    }
    return PostCommit();
}

uint32_t IResourceCompiler::CompileForAppend(const FileInfo &fileInfo)
{
    return CompileSingleFile(fileInfo);
}

const map<pair<ResType, string>, vector<ResourceItem>> &IResourceCompiler::GetResourceItems() const
{
    return nameInfos_;
}

void IResourceCompiler::SetModuleName(const string &moduleName)
{
    moduleName_ = moduleName;
}

uint32_t IResourceCompiler::CompileSingleFile(const FileInfo &fileInfo)
{
    return RESTOOL_SUCCESS;
}

uint32_t IResourceCompiler::PostCommit()
{
    IdWorker &idWorker = IdWorker::GetInstance();
    for (const auto &nameInfo : nameInfos_) {
        int64_t id = idWorker.GenerateId(nameInfo.first.first, nameInfo.first.second);
        if (id < 0) {
            PrintError(GetError(ERR_CODE_RESOURCE_ID_NOT_DEFINED).FormatCause(nameInfo.first.second.c_str(),
                ResourceUtil::ResTypeToString(nameInfo.first.first).c_str()));
            return RESTOOL_ERROR;
        }
        if (isHarResource_) {
            ResourceUtil::AddHarResourceId(id);
        }
        resourceInfos_.emplace(id, nameInfo.second);
    }
    return RESTOOL_SUCCESS;
}

bool IResourceCompiler::MergeResourceItem(const ResourceItem &resourceItem)
{
    string idName = ResourceUtil::GetIdName(resourceItem.GetName(), resourceItem.GetResType());
    if (!ResourceUtil::IsValidName(idName)) {
        PrintError(GetError(ERR_CODE_INVALID_RESOURCE_NAME).FormatCause(idName.c_str())
            .SetPosition(resourceItem.GetFilePath()));
        return false;
    }
    auto item = nameInfos_.find(make_pair(resourceItem.GetResType(), idName));
    if (item == nameInfos_.end()) {
        nameInfos_[make_pair(resourceItem.GetResType(), idName)].push_back(resourceItem);
        return true;
    }

    auto ret = find_if(item->second.begin(), item->second.end(), [resourceItem](auto &iter) {
        return resourceItem.GetLimitKey() == iter.GetLimitKey();
    });
    if (ret != item->second.end()) {
        PrintError(GetError(ERR_CODE_RESOURCE_DUPLICATE)
                       .FormatCause(idName.c_str(), ret->GetFilePath().c_str(), resourceItem.GetFilePath().c_str()));
        return false;
    }
    nameInfos_[make_pair(resourceItem.GetResType(), idName)].push_back(resourceItem);
    return true;
}

string IResourceCompiler::GetOutputFolder(const DirectoryInfo &directoryInfo) const
{
    string outputFolder = FileEntry::FilePath(output_).Append(RESOURCES_DIR)
        .Append(directoryInfo.limitKey).Append(directoryInfo.fileCluster).GetPath();
    return outputFolder;
}
}
}
}
