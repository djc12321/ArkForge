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

#include "file_manager.h"
#include <algorithm>
#include "compression_parser.h"
#include <iostream>
#include "resource_compiler_factory.h"
#include "file_entry.h"
#include "key_parser.h"
#include "reference_parser.h"
#include "resource_directory.h"
#include "resource_util.h"
#include "restool_errors.h"
#include "resource_module.h"

namespace OHOS {
namespace Global {
namespace Restool {
using namespace std;
uint32_t FileManager::ScanModules(const vector<string> &inputs, const string &output, const bool isHar)
{
    vector<pair<ResType, string>> noBaseResource;
    for (auto input : inputs) {
        if (ScanModule(input, output) != RESTOOL_SUCCESS) {
            return RESTOOL_ERROR;
        }
        CheckAllItems(noBaseResource);
    }
    if (!noBaseResource.empty()) {
        ResourceUtil::PrintWarningMsg(noBaseResource);
    }
    return isHar ? RESTOOL_SUCCESS : ParseReference(output);
}

uint32_t FileManager::MergeResourceItem(const map<int64_t, vector<ResourceItem>> &resourceInfos)
{
    return ResourceModule::MergeResourceItem(items_, resourceInfos);
}

// below private founction
uint32_t FileManager::ScanModule(const string &input, const string &output)
{
    ResourceModule resourceModule(input, output, moduleName_);
    if (resourceModule.ScanResource(scanHap_) != RESTOOL_SUCCESS) {
        return RESTOOL_ERROR;
    }
    MergeResourceItem(resourceModule.GetOwner());
    return RESTOOL_SUCCESS;
}

uint32_t FileManager::ParseReference(const string &output)
{
    ReferenceParser referenceParser;
    if (referenceParser.ParseRefInResources(items_, output) != RESTOOL_SUCCESS) {
        return RESTOOL_ERROR;
    }
    return RESTOOL_SUCCESS;
}

void FileManager::CheckAllItems(vector<pair<ResType, string>> &noBaseResource)
{
    for (const auto &item : items_) {
        bool found = any_of(item.second.begin(), item.second.end(), [](const auto &iter) {
            return iter.GetLimitKey() == "base";
        });
        if (!found) {
            auto firstItem = item.second.front();
            bool ret = any_of(noBaseResource.begin(), noBaseResource.end(), [firstItem](const auto &iterItem) {
                return (firstItem.GetResType() == iterItem.first)  &&
                    (firstItem.GetName() == iterItem.second);
            });
            if (!ret) {
                noBaseResource.push_back(make_pair(firstItem.GetResType(), firstItem.GetName()));
            }
        }
    }
}

bool FileManager::ScaleIcons(const string &output, const std::map<std::string, std::set<uint32_t>> &iconMap)
{
    if (!CompressionParser::GetCompressionParser()->ScaleIconEnable()) {
        cout << "Info: scale icon is not enable." << endl;
        return true;
    }
    std::set<int64_t> allIconIds;
    for (auto &it : iconMap) {
        if (it.first != "icon") {
            continue;
        }
        allIconIds.insert(it.second.begin(), it.second.end());
    }
    if (allIconIds.size() == 0) {
        cout << "Info: no icons need to scale, icon ids size is 0." << endl;
        return true;
    }
    for (auto &id : allIconIds) {
        std::map<int64_t, std::vector<ResourceItem>>::iterator iter = items_.find(id);
        if (iter == items_.end()) {
            continue;
        }
        for (auto &item : iter->second) {
            if (!ScaleIcon(output, item)) {
                return false;
            }
        }
    }
    return true;
}

void FileManager::SetScanHap(bool state)
{
    scanHap_ = state;
}

bool FileManager::ScaleIcon(const string &output, ResourceItem &item)
{
    std::string media = "media";
    // item's data is short path for icon file, such as "entry/resources/base/media/app_icon.png"
    const string currentData(reinterpret_cast<const char *>(item.GetData()), item.GetDataLength());
    auto outIndex = currentData.find_last_of(SEPARATOR);
    if (outIndex == string::npos) {
        PrintError(GetError(ERR_CODE_INVALID_OUTPUT).FormatCause(currentData.c_str()));
        return false;
    }
    // get current output file name and full path
    string fileName = currentData.substr(outIndex + 1);
    FileEntry::FilePath fullFilePath = FileEntry::FilePath(output).Append(RESOURCES_DIR).Append(item.GetLimitKey())
        .Append(media).Append(fileName);
    if (fullFilePath.GetExtension() == JSON_EXTENSION) {
        cout << "Info: can't scale media json file." << endl;
        return true;
    }
    const string fullOutPath = fullFilePath.GetPath();
    // delete current output file
    if (!ResourceUtil::RmoveFile(fullOutPath)) {
        return false;
    }
    // get origin icon output full path with the origin icon file name in src
    std::string dst = FileEntry::FilePath(output).Append(RESOURCES_DIR).Append(item.GetLimitKey()).Append(media)
        .Append(item.GetName()).GetPath();
    // the origin full file in src
    std::string scaleDst = item.GetFilePath();
    // scale icon
    if (!CompressionParser::GetCompressionParser()->CheckAndScaleIcon(item.GetFilePath(), dst, scaleDst)) {
        return false;
    }
    string scaleFileName = FileEntry::FilePath(scaleDst).GetFilename();
    dst = FileEntry::FilePath(output).Append(RESOURCES_DIR).Append(item.GetLimitKey()).Append(media)
        .Append(scaleFileName).GetPath();
    // compress scaled icon
    if (!CompressionParser::GetCompressionParser()->CopyAndTranscode(scaleDst, dst)) {
        return false;
    }
    string newFileName = FileEntry::FilePath(dst).GetFilename();
    std::string newData = moduleName_ + SEPARATOR + RESOURCES_DIR + SEPARATOR + item.GetLimitKey() + SEPARATOR + media
        + SEPARATOR + newFileName;
    if (!item.SetData(reinterpret_cast<const int8_t *>(newData.c_str()), newData.length())) {
        std::string msg = "item data is null, resource name: " + item.GetName();
        PrintError(GetError(ERR_CODE_UNDEFINED_ERROR).FormatCause(msg.c_str()).SetPosition(item.GetFilePath()));
        return false;
    }
    return true;
}
}
}
}
