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

#include "resource_module.h"

#include <algorithm>
#include <iostream>

#include "config_parser.h"
#include "resource_compiler_factory.h"
#include "restool_errors.h"

namespace OHOS {
namespace Global {
namespace Restool {
using namespace std;
const vector<ResType> ResourceModule::SCAN_SEQ = {
    ResType::ELEMENT,
    ResType::MEDIA,
    ResType::PROF,
};
ResourceModule::ResourceModule(const string &modulePath, const string &moduleOutput, const string &moduleName)
    : modulePath_(modulePath), moduleOutput_(moduleOutput), moduleName_(moduleName)
{
    string config = ResourceUtil::GetMainPath(modulePath).Append(CONFIG_JSON).GetPath();
    if (!ResourceUtil::FileExist(config)) {
        config = ResourceUtil::GetMainPath(modulePath).Append(MODULE_JSON).GetPath();
    }
    if (ResourceUtil::FileExist(config)) {
        ConfigParser parser = ConfigParser(config);
        parser.Init();
        isHarResource_ = parser.GetModuleType() == ConfigParser::ModuleType::HAR;
    }
}

uint32_t ResourceModule::ScanResource(bool isHap)
{
    if (!ResourceUtil::FileExist(modulePath_)) {
        return RESTOOL_SUCCESS;
    }

    ResourceDirectory directory;
    if (!directory.ScanResources(modulePath_, [this](const DirectoryInfo &info) -> bool {
            scanDirs_[info.dirType].push_back(info);
            return true;
        })) {
        return RESTOOL_ERROR;
    }

    for (const auto &type : SCAN_SEQ) {
        const auto &item = scanDirs_.find(type);
        if (item == scanDirs_.end() || item->second.empty()) {
            continue;
        }

        unique_ptr<IResourceCompiler> resourceCompiler =
            ResourceCompilerFactory::CreateCompiler(type, moduleOutput_, isHap, isHarResource_);
        resourceCompiler->SetModuleName(moduleName_);
        if (resourceCompiler->Compile(item->second) != RESTOOL_SUCCESS) {
            return RESTOOL_ERROR;
        }
        Push(resourceCompiler->GetResult());
    }
    return RESTOOL_SUCCESS;
}

const map<int64_t, vector<ResourceItem>> &ResourceModule::GetOwner() const
{
    return owner_;
}

const map<ResType, vector<DirectoryInfo>> &ResourceModule::GetScanDirectorys() const
{
    return scanDirs_;
}

uint32_t ResourceModule::MergeResourceItem(map<int64_t, vector<ResourceItem>> &alls,
    const map<int64_t, vector<ResourceItem>> &other, bool tipError)
{
    for (const auto &iter : other) {
        auto result = alls.emplace(iter.first, iter.second);
        if (result.second) {
            continue;
        }

        for (const auto &resourceItem : iter.second) {
            auto ret = find_if(result.first->second.begin(), result.first->second.end(), [&resourceItem](auto &iter) {
                return resourceItem.GetLimitKey() == iter.GetLimitKey();
            });
            if (ret == result.first->second.end()) {
                result.first->second.push_back(resourceItem);
                continue;
            }
            if (ret->IsCoverable()) { // overlap the hap resource by new resource
                *ret = resourceItem;
                continue;
            }
            if (tipError) {
                PrintError(GetError(ERR_CODE_RESOURCE_DUPLICATE)
                               .FormatCause(resourceItem.GetName().c_str(), ret->GetFilePath().c_str(),
                                            resourceItem.GetFilePath().c_str()));
                return RESTOOL_ERROR;
            }
            cerr << "Warning: '"<< resourceItem.GetName() <<"' conflict, first declared.";
            cerr << NEW_LINE_PATH << ret->GetFilePath() << endl;
            cerr << "but declared again." << NEW_LINE_PATH << resourceItem.GetFilePath() << endl;
        }
    }
    return RESTOOL_SUCCESS;
}
// below private
void ResourceModule::Push(const map<int64_t, std::vector<ResourceItem>> &other)
{
    for (const auto &iter : other) {
        owner_.emplace(iter.first, iter.second);
    }
}
}
}
}
