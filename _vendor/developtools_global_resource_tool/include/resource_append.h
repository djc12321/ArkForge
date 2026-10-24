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

#ifndef OHOS_RESTOOL_RESOURCE_APPEND_H
#define OHOS_RESTOOL_RESOURCE_APPEND_H

#include <fstream>
#include "cmd/package_parser.h"
#include "resource_compiler_factory.h"
#include "file_entry.h"

namespace OHOS {
namespace Global {
namespace Restool {
class ResourceAppend {
public:
    ResourceAppend(const PackageParser &packageParser);
    virtual ~ResourceAppend() {};
    uint32_t Append();
    uint32_t Combine();
    const std::map<int64_t, std::vector<std::shared_ptr<ResourceItem>>> GetItems() const
    {
        return items_;
    }

private:
    bool Combine(const std::string &folderPath);
    bool ScanResources(const std::string &resourcePath, const std::string &outputPath);
    bool ScanSubResources(const FileEntry entry, const std::string &resourcePath,
                            const std::string &outputPath);
    bool ScanSubLimitkeyResources(const FileEntry entry, const std::string &resourcePath,
                            const std::string &outputPath);
    bool ScanIegalResources(const std::string &resourcePath, const std::string &outputPath);
    bool ScanLimitKey(const std::unique_ptr<FileEntry> &entry, const std::string &limitKey,
                    const std::string outputPath);
    bool ScanFiles(const std::unique_ptr<FileEntry> &entry, const DirectoryInfo &directoryInfo,
                    const std::string &outputPath);
    bool ScanFile(const FileInfo &fileInfo, const std::string &outputPath);
    bool ScanSingleFile(const std::string &filePath, const std::string &outputPath);
    bool WriteFileInner(std::ostringstream &outStream, const std::string &outputPath) const;
    bool WriteResourceItem(const ResourceItem &resourceItem, std::ostringstream &out);
    bool LoadResourceItem(const std::string &filePath);
    bool ScanRawFilesOrResFiles(const std::string &path, const std::string &outputPath, const std::string &limit);
    bool WriteRawFilesOrResFiles(const std::string &filePath, const std::string &outputPath, const std::string &limit);
    bool Push(const std::shared_ptr<ResourceItem> &resourceItem);
    void AddRef(const std::shared_ptr<ResourceItem> &resourceItem);
    bool LoadResourceItemFromMem(const char buffer[], int32_t length);
    std::string ParseString(const char buffer[], int32_t length, int32_t &offset) const;
    int32_t ParseInt32(const char buffer[], int32_t length, int32_t &offset) const;
    bool ParseRef();
    bool CheckModuleResourceItem(const std::shared_ptr<ResourceItem> &resourceItem, int64_t id);
    bool IsBaseIdDefined(const FileInfo &fileInfo);
#ifdef __WIN32
    bool LoadResourceItemWin(const std::string &filePath);
#endif
    void CheckAllItems(std::vector<std::pair<ResType, std::string>> &noBaseResource);
    const PackageParser &packageParser_;
    std::map<int64_t, std::vector<std::shared_ptr<ResourceItem>>> items_;
    std::map<int64_t, std::vector<std::shared_ptr<ResourceItem>>> itemsForModule_;
    std::vector<std::shared_ptr<ResourceItem>> refs_;
};
}
}
}
#endif