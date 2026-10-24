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

#include "id_worker.h"
#include <iostream>
#include <regex>
#include "cmd/cmd_parser.h"

namespace OHOS {
namespace Global {
namespace Restool {
using namespace std;
uint32_t IdWorker::Init(ResourceIdCluster &type, int64_t startId)
{
    type_ = type;
    CmdParser &parser = CmdParser::GetInstance();
    PackageParser &packageParser = parser.GetPackageParser();
    IdDefinedParser idDefinedParser = IdDefinedParser(packageParser, type_);
    if (idDefinedParser.Init() != RESTOOL_SUCCESS) {
        return RESTOOL_ERROR;
    }
    sysDefinedIds_ = idDefinedParser.GetSysDefinedIds();
    appDefinedIds_ = idDefinedParser.GetAppDefinedIds();
    if (type == ResourceIdCluster::RES_ID_APP) {
        appId_ = static_cast<uint64_t>(startId);
        maxId_ = GetMaxId(startId);
    }
    return RESTOOL_SUCCESS;
}

int64_t IdWorker::GenerateId(ResType resType, const string &name)
{
    if (type_ == ResourceIdCluster::RES_ID_APP) {
        return GenerateAppId(resType, name);
    }
    return GenerateSysId(resType, name);
}

int64_t IdWorker::GetId(ResType resType, const string &name) const
{
    auto result = ids_.find(make_pair(resType, name));
    if (result == ids_.end()) {
        return -1;
    }
    return result->second;
}

vector<ResourceId> IdWorker::GetHeaderId() const
{
    map<ResType, vector<ResourceId>> idClassify;
    for (const auto &it : ids_) {
        ResourceId resourceId;
        resourceId.id = it.second;
        resourceId.type = ResourceUtil::ResTypeToString(it.first.first);
        resourceId.name = it.first.second;
        idClassify[it.first.first].push_back(resourceId);
    }

    vector<ResourceId> ids;
    for (const auto &item : idClassify) {
        ids.insert(ids.end(), item.second.begin(), item.second.end());
    }
    return ids;
}

int64_t IdWorker::GetSystemId(ResType resType, const string &name) const
{
    auto result = sysDefinedIds_.find(make_pair(resType, name));
    if (result == sysDefinedIds_.end()) {
        return -1;
    }
    return result->second.id;
}

int64_t IdWorker::LoadIdFromHap(map<int64_t, vector<ResourceItem>> &items)
{
    int64_t minId = 0xffffffff;
    int64_t maxId = 0x01000000;
    for (auto &item : items) {
        for (auto &resourceItem : item.second) {
            resourceItem.CheckData();
            ids_.emplace(make_pair(resourceItem.GetResType(), resourceItem.GetName()), item.first);
        }
        minId = min(minId, item.first);
        maxId = max(maxId, item.first);
    }
    maxId_ = GetMaxId(minId);
    if (static_cast<uint64_t>(maxId) > maxId_) {
        PrintError(GetError(ERR_CODE_RESOURCE_ID_EXCEED).FormatCause(static_cast<uint64_t>(maxId), maxId_));
        return RESTOOL_ERROR;
    }
    appId_ = static_cast<uint64_t>(maxId) + 1;
    return RESTOOL_SUCCESS;
}

int64_t IdWorker::GenerateAppId(ResType resType, const string &name)
{
    auto result = ids_.find(make_pair(resType, name));
    if (result != ids_.end()) {
        return result->second;
    }

    auto defined = appDefinedIds_.find(make_pair(resType, name));
    if (defined != appDefinedIds_.end()) {
        ids_.emplace(make_pair(resType, name), defined->second.id);
        return defined->second.id;
    }

    result = cacheIds_.find(make_pair(resType, name));
    if (result != cacheIds_.end()) {
        ids_.emplace(make_pair(resType, name), result->second);
        return result->second;
    }

    if (appId_ > maxId_) {
        PrintError(GetError(ERR_CODE_RESOURCE_ID_EXCEED).FormatCause(appId_, maxId_));
        return -1;
    }
    int64_t id = -1;
    if (!delIds_.empty()) {
        id = delIds_.front();
        delIds_.erase(delIds_.begin());
    } else {
        id = GetCurId();
        if (id < 0) {
            return -1;
        }
    }
    ids_.emplace(make_pair(resType, name), id);
    return id;
}

int64_t IdWorker::GetCurId()
{
    if (appDefinedIds_.size() == 0) {
        return static_cast<int64_t>(appId_++);
    }
    while (appId_ <= maxId_) {
        int64_t id = static_cast<int64_t>(appId_);
        auto ret = find_if(appDefinedIds_.begin(), appDefinedIds_.end(), [id](const auto &iter) {
            return id == iter.second.id;
        });
        if (ret == appDefinedIds_.end()) {
            return static_cast<int64_t>(appId_++);
        }
        appId_++;
    }
    PrintError(GetError(ERR_CODE_RESOURCE_ID_EXCEED).FormatCause(appId_, maxId_).SetPosition("id_defined.json"));
    return -1;
}

int64_t IdWorker::GenerateSysId(ResType resType, const string &name)
{
    auto result = ids_.find(make_pair(resType, name));
    if (result != ids_.end()) {
        return result->second;
    }

    auto defined = sysDefinedIds_.find(make_pair(resType, name));
    if (defined != sysDefinedIds_.end()) {
        ids_.emplace(make_pair(resType, name), defined->second.id);
        return defined->second.id;
    }
    return -1;
}

uint64_t IdWorker::GetMaxId(uint64_t startId) const
{
    uint64_t flag = 1;
    while ((flag & startId) == 0) {
        flag = flag << 1;
    }
    if (startId + flag < 1) {
        return 0;
    }
    return startId + flag - 1;
}
}
}
}