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

#include "resource_merge.h"
#include "file_entry.h"

namespace OHOS {
namespace Global {
namespace Restool {
using namespace std;
const vector<ConfigParser::ModuleType> ResourceMerge::ORDERS = {
    ConfigParser::ModuleType::NONE,
    ConfigParser::ModuleType::ENTRY,
    ConfigParser::ModuleType::FEATURE,
    ConfigParser::ModuleType::SHARED,
    ConfigParser::ModuleType::HAR
};

ResourceMerge::ResourceMerge()
{
}

ResourceMerge::~ResourceMerge()
{
}

uint32_t ResourceMerge::Init(const PackageParser &packageParser)
{
    const vector<string> &inputs = packageParser.GetInputs();
    if (packageParser.IsFileList()) {
        inputsOrder_ = inputs;
        return RESTOOL_SUCCESS;
    }

    map<ConfigParser::ModuleType, vector<string>> inputTypes;
    string hapDir = "";
    for (auto it = inputs.crbegin(); it != inputs.crend(); it++) {
        string configPath = ResourceUtil::GetMainPath(*it).Append(ConfigParser::GetConfigName()).GetPath();
        string resourceDir = FileEntry::FilePath(*it).GetPath();
        string indexPath = ResourceUtil::GetMainPath(*it).Append(RESOURCE_INDEX_FILE).GetPath();
        if (ResourceUtil::FileExist(indexPath)) {
            hapDir = resourceDir;
            continue;
        }
        ConfigParser::ModuleType moduleType = ConfigParser::ModuleType::NONE;
        if (!ResourceUtil::FileExist(configPath)) {
            inputTypes[moduleType].push_back(resourceDir);
            continue;
        }
        ConfigParser configParser(configPath);
        if (configParser.Init() != RESTOOL_SUCCESS) {
            return RESTOOL_ERROR;
        }
        moduleType = configParser.GetModuleType();
        inputTypes[moduleType].push_back(resourceDir);
    }

    for (const auto &type : ORDERS) {
        if (inputTypes.find(type) == inputTypes.end()) {
            continue;
        }
        inputsOrder_.insert(inputsOrder_.end(), inputTypes[type].begin(), inputTypes[type].end());
    }
    if (hapDir != "") {
        inputsOrder_.insert(inputsOrder_.begin(), hapDir);
    }
    return RESTOOL_SUCCESS;
}

const vector<string> &ResourceMerge::GetInputs() const
{
    return inputsOrder_;
}
}
}
}