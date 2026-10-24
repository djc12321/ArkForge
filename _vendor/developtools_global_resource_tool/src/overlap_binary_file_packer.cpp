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

#include "overlap_binary_file_packer.h"

namespace OHOS {
namespace Global {
namespace Restool {
using namespace std;

OverlapBinaryFilePacker::OverlapBinaryFilePacker(const PackageParser &packageParser, const std::string &moduleName)
    : BinaryFilePacker(packageParser, moduleName)
{
}

uint32_t OverlapBinaryFilePacker::CopyBinaryFile(const vector<string> &inputs)
{
    string hapPath = inputs[0];
    BinaryFilePacker::CopyBinaryFile(hapPath);
    if (CheckCopyResults() != RESTOOL_SUCCESS) {
        return RESTOOL_ERROR;
    }

    vector<string> resource(inputs.begin() + 1, inputs.end());
    BinaryFilePacker rawFilePacker(packageParser_, moduleName_);
    rawFilePacker.CopyBinaryFileAsync(resource);
    if (rawFilePacker.GetResult() != RESTOOL_SUCCESS) {
        return RESTOOL_ERROR;
    }
    return RESTOOL_SUCCESS;
}

bool OverlapBinaryFilePacker::IsDuplicated(const unique_ptr<FileEntry> &entry, string subPath)
{
    lock_guard<mutex> lock(mutex_);
    if (!g_hapResourceSet.emplace(subPath).second || !g_resourceSet.emplace(subPath).second) {
        cout << "Warning: '" << entry->GetFilePath().GetPath() << "' is defined repeatedly in hap." << endl;
        return true;
    }
    return false;
}
}
}
}