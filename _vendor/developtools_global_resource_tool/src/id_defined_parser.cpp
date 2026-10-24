/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#include "id_defined_parser.h"
#include "file_entry.h"
#include "file_manager.h"
#include "resource_util.h"

namespace OHOS {
namespace Global {
namespace Restool {
using namespace std;
const int64_t IdDefinedParser::START_SYS_ID = 0x07800000;
IdDefinedParser::IdDefinedParser(const PackageParser &packageParser, const ResourceIdCluster &type)
    : packageParser_(packageParser), type_(type), root_(nullptr)
{
}

IdDefinedParser::~IdDefinedParser()
{
    if (root_) {
        cJSON_Delete(root_);
    }
}

uint32_t IdDefinedParser::Init()
{
    InitParser();
    string idDefinedInput = packageParser_.GetIdDefinedInputPath();
    int64_t startId = static_cast<int64_t>(packageParser_.GetStartId());
    bool combine = packageParser_.GetCombine();
    bool isSys = type_ == ResourceIdCluster::RES_ID_SYS;
    for (const auto &inputPath : packageParser_.GetInputs()) {
        string idDefinedPath;
        if (combine) {
            idDefinedPath = FileEntry::FilePath(inputPath).Append(ID_DEFINED_FILE).GetPath();
        } else {
            idDefinedPath = ResourceUtil::GetBaseElementPath(inputPath).Append(ID_DEFINED_FILE).GetPath();
        }
        if (ResourceUtil::FileExist(idDefinedPath) && startId > 0) {
            PrintError(GetError(ERR_CODE_EXCLUSIVE_START_ID).FormatCause(startId).SetPosition(idDefinedPath));
            return RESTOOL_ERROR;
        }
        if (Init(idDefinedPath, isSys) != RESTOOL_SUCCESS) {
            return RESTOOL_ERROR;
        }
    }
    //SystemResource.hap only defined by base/element/id_defined.json
    string moduleName = FileManager::GetInstance().GetModuleName();
    if (isSys && moduleName == "entry") {
        return RESTOOL_SUCCESS;
    }
    if (!idDefinedInput.empty() && ResourceUtil::FileExist(idDefinedInput)) {
        appDefinedIds_.clear();
        idDefineds_.clear();
        string idDefinedPath = FileEntry::FilePath(idDefinedInput).GetPath();
        if (Init(idDefinedPath, false) != RESTOOL_SUCCESS) {
            return RESTOOL_ERROR;
        }
    }
    for (const auto &sysIdDefinedPath : packageParser_.GetSysIdDefinedPaths()) {
        if (Init(sysIdDefinedPath, true) != RESTOOL_SUCCESS) {
            return RESTOOL_ERROR;
        }
    }
    string sysIdDefinedPath = FileEntry::FilePath(packageParser_.GetRestoolPath())
        .GetParent().Append(ID_DEFINED_FILE).GetPath();
    if (Init(sysIdDefinedPath, true) != RESTOOL_SUCCESS) {
        return RESTOOL_ERROR;
    }
    return RESTOOL_SUCCESS;
}

uint32_t IdDefinedParser::Init(const string &filePath, bool isSystem)
{
    if (!ResourceUtil::FileExist(filePath)) {
        return RESTOOL_SUCCESS;
    }

    if (!ResourceUtil::OpenJsonFile(filePath, &root_)) {
        return RESTOOL_ERROR;
    }

    if (!root_ || !cJSON_IsObject(root_)) {
        PrintError(GetError(ERR_CODE_JSON_FORMAT_ERROR).SetPosition(filePath));
        return RESTOOL_ERROR;
    }

    cJSON *recordNode = cJSON_GetObjectItem(root_, "record");
    if (!recordNode || !cJSON_IsArray(recordNode)) {
        PrintError(GetError(ERR_CODE_JSON_NODE_MISMATCH).FormatCause("record", "array").SetPosition(filePath));
        return RESTOOL_ERROR;
    }
    if (cJSON_GetArraySize(recordNode) == 0) {
        cerr << "Warning: 'record' node is empty, please check the JSON file.";
        cerr << NEW_LINE_PATH << filePath << endl;
        return RESTOOL_SUCCESS;
    }
    int64_t startSysId = 0;
    if (isSystem) {
        startSysId = GetStartId(filePath);
        if (startSysId < 0) {
            return RESTOOL_ERROR;
        }
    }

    if (IdDefinedToResourceIds(filePath, recordNode, isSystem, startSysId) != RESTOOL_SUCCESS) {
        return RESTOOL_ERROR;
    }
    return RESTOOL_SUCCESS;
}

void IdDefinedParser::InitParser()
{
    using namespace placeholders;
    handles_.emplace("type", bind(&IdDefinedParser::ParseType, this, _1, _2, _3));
    handles_.emplace("name", bind(&IdDefinedParser::ParseName, this, _1, _2, _3));
    handles_.emplace("id", bind(&IdDefinedParser::ParseId, this, _1, _2, _3));
    handles_.emplace("order", bind(&IdDefinedParser::ParseOrder, this, _1, _2, _3));
}

uint32_t IdDefinedParser::IdDefinedToResourceIds(const std::string &filePath, const cJSON *record,
    bool isSystem, const int64_t startSysId)
{
    int64_t index = -1;
    for (cJSON *item = record->child; item; item = item->next) {
        index++;
        if (!cJSON_IsObject(item)) {
            PrintError(GetError(ERR_CODE_JSON_NODE_MISMATCH).FormatCause("record's child", "object")
                .SetPosition(filePath));
            return RESTOOL_ERROR;
        }
        ResourceId resourceId;
        resourceId.seq = index;
        resourceId.id = startSysId;
        for (const auto &handle : handles_) {
            if ((handle.first == "id" && isSystem) || (handle.first == "order" && !isSystem)) {
                continue;
            }
            if (!handle.second(filePath, cJSON_GetObjectItem(item, handle.first.c_str()), resourceId)) {
                return RESTOOL_ERROR;
            }
        }
        if (!PushResourceId(filePath, resourceId, isSystem)) {
            return RESTOOL_ERROR;
        }
    }
    return RESTOOL_SUCCESS;
}

bool IdDefinedParser::PushResourceId(const std::string &filePath, const ResourceId &resourceId, bool isSystem)
{
    ResType resType = ResourceUtil::GetResTypeFromString(resourceId.type);
    auto ret = idDefineds_.emplace(resourceId.id, resourceId);
    if (!ret.second) {
        PrintError(GetError(ERR_CODE_ID_DEFINED_SAME_ID).FormatCause(ret.first->second.name.c_str(),
            resourceId.name.c_str()).SetPosition(filePath));
        return false;
    }
    auto checkRet = checkDefinedIds_.find(filePath);
    if (checkRet != checkDefinedIds_.end()) {
        bool found = any_of(checkRet->second.begin(), checkRet->second.end(),
            [resType, resourceId](const auto &iterItem) {
            return (resType == iterItem.first)  && (resourceId.name == iterItem.second);
        });
        if (found) {
            PrintError(GetError(ERR_CODE_RESOURCE_DUPLICATE).FormatCause(resourceId.name.c_str(),
                filePath.c_str(), filePath.c_str()));
            return false;
        }
        checkRet->second.push_back(make_pair(resType, resourceId.name));
    } else {
        std::vector<std::pair<ResType, std::string>> vects;
        vects.push_back(make_pair(resType, resourceId.name));
        checkDefinedIds_.emplace(filePath, vects);
    }
    if (isSystem) {
        sysDefinedIds_.emplace(make_pair(resType, resourceId.name), resourceId);
    } else {
        appDefinedIds_.emplace(make_pair(resType, resourceId.name), resourceId);
    }
    return true;
}

bool IdDefinedParser::ParseId(const std::string &filePath, const cJSON *origId, ResourceId &resourceId)
{
    if (!origId) {
        PrintError(GetError(ERR_CODE_JSON_NODE_MISSING).FormatCause("id").SetPosition(filePath));
        return false;
    }
    if (!cJSON_IsString(origId)) {
        PrintError(GetError(ERR_CODE_JSON_NODE_MISMATCH).FormatCause("id", "string").SetPosition(filePath));
        return false;
    }
    string idStr = origId->valuestring;
    if (!ResourceUtil::CheckHexStr(idStr)) {
        PrintError(GetError(ERR_CODE_ID_DEFINED_INVALID_ID).FormatCause(idStr.c_str()).SetPosition(filePath));
        return false;
    }
    int64_t id = strtoll(idStr.c_str(), nullptr, 16);
    if (id < 0x01000000 || (id > 0x06FFFFFF && id < 0x08000000) || id > 0xFFFFFFFF) {
        PrintError(GetError(ERR_CODE_ID_DEFINED_INVALID_ID).FormatCause(idStr.c_str()).SetPosition(filePath));
        return false;
    }
    resourceId.id = id;
    return true;
}

bool IdDefinedParser::ParseType(const std::string &filePath, const cJSON *type, ResourceId &resourceId)
{
    if (!type) {
        PrintError(GetError(ERR_CODE_JSON_NODE_MISSING).FormatCause("type").SetPosition(filePath));
        return false;
    }
    if (!cJSON_IsString(type)) {
        PrintError(GetError(ERR_CODE_JSON_NODE_MISMATCH).FormatCause("type", "string").SetPosition(filePath));
        return false;
    }
    if (ResourceUtil::GetResTypeFromString(type->valuestring) == ResType::INVALID_RES_TYPE) {
        PrintError(GetError(ERR_CODE_ID_DEFINED_INVALID_TYPE)
                       .FormatCause(type->valuestring, ResourceUtil::GetAllRestypeString().c_str())
                       .SetPosition(filePath));
        return false;
    }
    resourceId.type = type->valuestring;
    return true;
}

bool IdDefinedParser::ParseName(const std::string &filePath, const cJSON *name, ResourceId &resourceId)
{
    if (!name) {
        PrintError(GetError(ERR_CODE_JSON_NODE_MISSING).FormatCause("name").SetPosition(filePath));
        return false;
    }
    if (!cJSON_IsString(name)) {
        PrintError(GetError(ERR_CODE_JSON_NODE_MISMATCH).FormatCause("name", "string").SetPosition(filePath));
        return false;
    }
    resourceId.name = name->valuestring;
    if (type_ == ResourceIdCluster::RES_ID_SYS &&
        (static_cast<uint64_t>(resourceId.id) & static_cast<uint64_t>(START_SYS_ID)) == START_SYS_ID &&
        !ResourceUtil::IsValidName(resourceId.name)) {
        PrintError(GetError(ERR_CODE_INVALID_RESOURCE_NAME).FormatCause(resourceId.name.c_str())
            .SetPosition(filePath));
        return false;
    }
    return true;
}

bool IdDefinedParser::ParseOrder(const std::string &filePath, const cJSON *order, ResourceId &resourceId)
{
    if (!order) {
        PrintError(GetError(ERR_CODE_JSON_NODE_MISSING).FormatCause("order").SetPosition(filePath));
        return false;
    }
    if (!ResourceUtil::IsIntValue(order)) {
        PrintError(GetError(ERR_CODE_JSON_NODE_MISMATCH).FormatCause("order", "integer").SetPosition(filePath));
        return false;
    }
    int64_t orderId = order->valueint;
    if (orderId != resourceId.seq) {
        PrintError(GetError(ERR_CODE_ID_DEFINED_ORDER_MISMATCH).FormatCause(orderId, resourceId.seq, resourceId.seq)
            .SetPosition(filePath));
        return false;
    }
    resourceId.id = resourceId.id + orderId;
    return true;
}

int64_t IdDefinedParser::GetStartId(const std::string &filePath) const
{
    cJSON *startIdNode = cJSON_GetObjectItem(root_, "startId");
    if (!startIdNode) {
        PrintError(GetError(ERR_CODE_JSON_NODE_MISSING).FormatCause("startId").SetPosition(filePath));
        return -1;
    }

    if (!cJSON_IsString(startIdNode)) {
        PrintError(GetError(ERR_CODE_JSON_NODE_MISMATCH).FormatCause("startId", "string").SetPosition(filePath));
        return -1;
    }

    int64_t id = strtoll(startIdNode->valuestring, nullptr, 16);
    if (id == 0) {
        PrintError(GetError(ERR_CODE_ID_DEFINED_INVALID_ID).FormatCause(startIdNode->valuestring)
            .SetPosition(filePath));
        return -1;
    }
    return id;
}

}
}
}