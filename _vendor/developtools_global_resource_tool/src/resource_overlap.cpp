/*
 * Copyright (c) 2024-2024 Huawei Device Co., Ltd.
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

#include <future>
#include "resource_overlap.h"
#include "overlap_binary_file_packer.h"
#include "file_manager.h"
#include "resource_table.h"
#include "id_worker.h"

namespace OHOS {
namespace Global {
namespace Restool {
using namespace std;

ResourceOverlap::ResourceOverlap(const PackageParser &packageParser) : ResourcePack(packageParser)
{
}

uint32_t ResourceOverlap::Pack()
{
    cout << "Info: Pack: overlap pack mode" << endl;

    if (InitResourcePack() != RESTOOL_SUCCESS) {
        return RESTOOL_ERROR;
    }

    ResourceMerge resourceMerge;
    if (resourceMerge.Init(packageParser_) != RESTOOL_SUCCESS) {
        return RESTOOL_ERROR;
    }

    OverlapBinaryFilePacker rawFilePacker(packageParser_, moduleName_);
    rawFilePacker.CopyBinaryFileAsync(resourceMerge.GetInputs());

    if (LoadHapResources() != RESTOOL_SUCCESS) {
        rawFilePacker.Terminate();
        return RESTOOL_ERROR;
    }

    if (PackResources(resourceMerge) != RESTOOL_SUCCESS) {
        rawFilePacker.Terminate();
        return RESTOOL_ERROR;
    }

    if (rawFilePacker.GetResult() != RESTOOL_SUCCESS) {
        return RESTOOL_ERROR;
    }
    return RESTOOL_SUCCESS;
}

uint32_t ResourceOverlap::ScanResources(const std::vector<std::string> &inputs, const std::string &output)
{
    auto &fileManager = FileManager::GetInstance();
    fileManager.SetModuleName(moduleName_);
    vector<string> hapResInput{inputs[0]};
    if (fileManager.ScanModules(hapResInput, output, configJson_.IsHar()) != RESTOOL_SUCCESS) {
        return RESTOOL_ERROR;
    }
    fileManager.SetScanHap(false);

    vector<string> resInputs(inputs.begin() + 1, inputs.end());
    if (fileManager.ScanModules(resInputs, output, configJson_.IsHar()) != RESTOOL_SUCCESS) {
        return RESTOOL_ERROR;
    }
    return RESTOOL_SUCCESS;
}

uint32_t ResourceOverlap::LoadHapResources()
{
    ResourceTable resourceTabel;
    map<int64_t, vector<ResourceItem>> items;
    string resourceIndexPath =
        FileEntry::FilePath(packageParser_.GetInputs()[0]).GetParent().Append(RESOURCE_INDEX_FILE).GetPath();
    if (resourceTabel.LoadResTable(resourceIndexPath, items) != RESTOOL_SUCCESS) {
        return RESTOOL_ERROR;
    }
    
    if (IdWorker::GetInstance().LoadIdFromHap(items) != RESTOOL_SUCCESS) {
        return RESTOOL_ERROR;
    }

    FileManager &fileManager = FileManager::GetInstance();
    fileManager.SetModuleName(moduleName_);
    if (fileManager.MergeResourceItem(items) != RESTOOL_SUCCESS) {
        return RESTOOL_ERROR;
    }
    fileManager.SetScanHap(true);
    return RESTOOL_SUCCESS;
}
}
}
}