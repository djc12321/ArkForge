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

#include "resource_directory.h"

#include <iostream>

#include "file_entry.h"
#include "resource_util.h"
#include "restool_errors.h"
#include "select_compile_parse.h"

namespace OHOS {
namespace Global {
namespace Restool {
using namespace std;
bool ResourceDirectory::ScanResources(const string &resourcesDir, function<bool(const DirectoryInfo&)> callback) const
{
    FileEntry f(resourcesDir);
    if (!f.Init()) {
        return false;
    }

    for (const auto &it : f.GetChilds()) {
        string limitKey = it->GetFilePath().GetFilename();
        if (ResourceUtil::IsIgnoreFile(*it)) {
            continue;
        }

        if (it->IsFile()) {
            PrintError(GetError(ERR_CODE_INVALID_RESOURCE_PATH)
                .FormatCause(it->GetFilePath().GetPath().c_str(), "not a directory"));
            return false;
        }

        if (limitKey == RAW_FILE_DIR || limitKey == RES_FILE_DIR) {
            continue;
        }

        if (!ScanResourceLimitKeyDir(it->GetFilePath().GetPath(), limitKey, callback)) {
            return false;
        }
    }
    return true;
}

// below private
bool ResourceDirectory::ScanResourceLimitKeyDir(const string &resourceTypeDir, const string &limitKey,
    function<bool(const DirectoryInfo&)> callback) const
{
    vector<KeyParam> keyParams;
    if (!KeyParser::Parse(limitKey, keyParams)) {
        PrintError(GetError(ERR_CODE_INVALID_LIMIT_KEY).FormatCause(limitKey.c_str()).SetPosition(resourceTypeDir));
        return false;
    }
    if (!SelectCompileParse::IsSelectCompile(keyParams)) {
        return true;
    }
    FileEntry f(resourceTypeDir);
    if (!f.Init()) {
        return false;
    }
    for (const auto &it : f.GetChilds()) {
        string dirPath = it->GetFilePath().GetPath();
        string fileCluster = it->GetFilePath().GetFilename();
        if (ResourceUtil::IsIgnoreFile(*it)) {
            continue;
        }

        if (it->IsFile()) {
            PrintError(GetError(ERR_CODE_INVALID_RESOURCE_PATH).FormatCause(dirPath.c_str(), "not a directory"));
            return false;
        }

        ResType type = ResourceUtil::GetResTypeByDir(fileCluster);
        if (type == ResType::INVALID_RES_TYPE) {
            PrintError(GetError(ERR_CODE_INVALID_RESOURCE_DIR)
                           .FormatCause(fileCluster.c_str(), ResourceUtil::GetAllResTypeDirs().c_str())
                           .SetPosition(dirPath));
            return false;
        }
        DirectoryInfo info = { limitKey, fileCluster, dirPath, keyParams, type };
        if (callback && !callback(info)) {
            return false;
        }
    }
    return true;
}
}
}
}
