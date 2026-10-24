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

#include "reference_parser.h"
#include <iostream>
#include <regex>
#include "file_entry.h"
#include "restool_errors.h"

namespace OHOS {
namespace Global {
namespace Restool {
using namespace std;
const map<string, ResType> ReferenceParser::ID_REFS = {
    { "^\\$id:", ResType::ID },
    { "^\\$boolean:", ResType::BOOLEAN },
    { "^\\$color:", ResType::COLOR },
    { "^\\$float:", ResType::FLOAT },
    { "^\\$media:", ResType::MEDIA },
    { "^\\$profile:", ResType::PROF },
    { "^\\$integer:", ResType::INTEGER },
    { "^\\$string:", ResType::STRING },
    { "^\\$pattern:", ResType::PATTERN },
    { "^\\$plural:", ResType::PLURAL },
    { "^\\$theme:", ResType::THEME },
    { "^\\$symbol:", ResType::SYMBOL }
};

const map<string, ResType> ReferenceParser::ID_OHOS_REFS = {
    { "^\\$ohos:id:", ResType::ID },
    { "^\\$ohos:boolean:", ResType::BOOLEAN },
    { "^\\$ohos:color:", ResType::COLOR },
    { "^\\$ohos:float:", ResType::FLOAT },
    { "^\\$ohos:media:", ResType::MEDIA },
    { "^\\$ohos:profile:", ResType::PROF },
    { "^\\$ohos:integer:", ResType::INTEGER },
    { "^\\$ohos:string:", ResType::STRING },
    { "^\\$ohos:pattern:", ResType::PATTERN },
    { "^\\$ohos:plural:", ResType::PLURAL },
    { "^\\$ohos:theme:", ResType::THEME },
    { "^\\$ohos:symbol:", ResType::SYMBOL }
};

std::map<int64_t, std::set<int64_t>> ReferenceParser::layerIconIds_;

ReferenceParser::ReferenceParser() : idWorker_(IdWorker::GetInstance()), root_(nullptr), isParsingMediaJson_(false)
{
}

ReferenceParser::~ReferenceParser()
{
    if (root_) {
        cJSON_Delete(root_);
    }
}

uint32_t ReferenceParser::ParseRefInResources(map<int64_t, vector<ResourceItem>> &items, const string &output)
{
    for (auto &iter : items) {
        for (auto &resourceItem : iter.second) {
            if (resourceItem.IsCoverable()) {
                continue;
            }
            if (IsElementRef(resourceItem) && ParseRefInResourceItem(resourceItem) != RESTOOL_SUCCESS) {
                return RESTOOL_ERROR;
            }
            if ((IsMediaRef(resourceItem) || IsProfileRef(resourceItem)) &&
                ParseRefInJsonFile(resourceItem, output) != RESTOOL_SUCCESS) {
                return RESTOOL_ERROR;
            }
        }
    }
    return RESTOOL_SUCCESS;
}

uint32_t ReferenceParser::ParseRefInResourceItem(ResourceItem &resourceItem) const
{
    ResType resType = resourceItem.GetResType();
    string data;
    bool update = false;
    if (IsStringOfResourceItem(resType)) {
        if (resourceItem.GetData() == nullptr) {
            std::string msg = "item data is null, resource name: " + resourceItem.GetName();
            PrintError(GetError(ERR_CODE_UNDEFINED_ERROR).FormatCause(msg.c_str()));
            return RESTOOL_ERROR;
        }
        data = string(reinterpret_cast<const char *>(resourceItem.GetData()), resourceItem.GetDataLength());
        if (!ParseRefString(data, update, resourceItem.GetFilePath())) {
            return RESTOOL_ERROR;
        }
        if (!update) {
            return RESTOOL_SUCCESS;
        }
    } else if (IsArrayOfResourceItem(resType)) {
        if (!ParseRefResourceItemData(resourceItem, data, update)) {
            return RESTOOL_ERROR;
        }
        if (!update) {
            return RESTOOL_SUCCESS;
        }
    }
    if (update && !resourceItem.SetData(reinterpret_cast<const int8_t *>(data.c_str()), data.length())) {
        std::string msg = "item data is null, resource name: " + resourceItem.GetName();
        PrintError(GetError(ERR_CODE_UNDEFINED_ERROR).FormatCause(msg.c_str()).SetPosition(resourceItem.GetFilePath()));
        return RESTOOL_ERROR;
    }
    return RESTOOL_SUCCESS;
}

uint32_t ReferenceParser::ParseRefInJsonFile(ResourceItem &resourceItem, const string &output, const bool isIncrement)
{
    string jsonPath;
    ResType resType = resourceItem.GetResType();
    string resName = resourceItem.GetName();
    if (resType == ResType::MEDIA) {
        jsonPath = FileEntry::FilePath(output).Append(RESOURCES_DIR).Append(resourceItem.GetLimitKey()).Append("media")
            .Append(resName).GetPath();
        isParsingMediaJson_ = true;
        mediaJsonId_ = idWorker_.GetId(resType, ResourceUtil::GetIdName(resName, resType));
        if (mediaJsonId_ != INVALID_ID) {
            set<int64_t> set;
            layerIconIds_[mediaJsonId_] = set;
        }
    } else {
        jsonPath = FileEntry::FilePath(output).Append(RESOURCES_DIR).Append("base").Append("profile").Append(resName)
            .GetPath();
    }
    bool parseJsonRet = ParseRefJson(resourceItem.GetFilePath(), jsonPath);
    isParsingMediaJson_ = false;
    mediaJsonId_ = INVALID_ID;
    if (!parseJsonRet) {
        return RESTOOL_ERROR;
    }

    if (isIncrement && ResourceUtil::FileExist(jsonPath)) {
        resourceItem.SetData(reinterpret_cast<const int8_t *>(jsonPath.c_str()), jsonPath.length());
    }
    return RESTOOL_SUCCESS;
}

uint32_t ReferenceParser::ParseRefInString(string &value, bool &update, const std::string &filePath) const
{
    if (ParseRefString(value, update, filePath)) {
        return RESTOOL_SUCCESS;
    }
    return RESTOOL_ERROR;
}

bool ReferenceParser::ParseRefJson(const string &from, const string &to)
{
    if (!ResourceUtil::OpenJsonFile(from, &root_)) {
        return false;
    }
    if (!root_ || !cJSON_IsObject(root_)) {
        PrintError(GetError(ERR_CODE_JSON_FORMAT_ERROR).SetPosition(from));
        return RESTOOL_ERROR;
    }
    bool needSave = false;
    if (!ParseRefJsonImpl(root_, needSave, from)) {
        return false;
    }

    if (!needSave) {
        return true;
    }

    if (!ResourceUtil::CreateDirs(FileEntry::FilePath(to).GetParent().GetPath())) {
        return false;
    }

    if (!ResourceUtil::SaveToJsonFile(to, root_)) {
        return false;
    }
    return true;
}

bool ReferenceParser::ParseRefResourceItemData(const ResourceItem &resourceItem, string &data, bool &update) const
{
    if (resourceItem.GetData() == nullptr) {
        std::string msg = "item data is null, resource name: " + resourceItem.GetName();
        PrintError(GetError(ERR_CODE_UNDEFINED_ERROR).FormatCause(msg.c_str()));
        return false;
    }
    data = string(reinterpret_cast<const char *>(resourceItem.GetData()), resourceItem.GetDataLength());
    vector<string> contents = ResourceUtil::DecomposeStrings(data);
    if (contents.empty()) {
        PrintError(GetError(ERR_CODE_ARRAY_TOO_LARGE).FormatCause(resourceItem.GetName().c_str())
            .SetPosition(resourceItem.GetFilePath()));
        return false;
    }

    for (auto &content : contents) {
        bool flag = false;
        if (!ParseRefString(content, flag, resourceItem.GetFilePath())) {
            return false;
        }
        update = (update || flag);
    }

    if (!update) {
        return true;
    }

    data = ResourceUtil::ComposeStrings(contents);
    if (data.empty()) {
        PrintError(GetError(ERR_CODE_ARRAY_TOO_LARGE).FormatCause(resourceItem.GetName().c_str())
            .SetPosition(resourceItem.GetFilePath()));
        return false;
    }
    return true;
}

bool ReferenceParser::IsStringOfResourceItem(ResType resType) const
{
    if (resType == ResType::STRING ||
        resType == ResType::INTEGER ||
        resType == ResType::BOOLEAN ||
        resType == ResType::COLOR ||
        resType == ResType::FLOAT ||
        resType == ResType::SYMBOL) {
        return true;
    }
    return false;
}

bool ReferenceParser::IsArrayOfResourceItem(ResType resType) const
{
    if (resType == ResType::STRARRAY ||
        resType == ResType::INTARRAY ||
        resType == ResType::PLURAL ||
        resType == ResType::THEME ||
        resType == ResType::PATTERN) {
        return true;
    }
    return false;
}

bool ReferenceParser::IsElementRef(const ResourceItem &resourceItem) const
{
    ResType resType = resourceItem.GetResType();
    auto result = find_if(g_contentClusterMap.begin(), g_contentClusterMap.end(), [resType](const auto &iter) {
        return resType == iter.second;
    });
    if (result == g_contentClusterMap.end()) {
        return false;
    }
    return true;
}

bool ReferenceParser::IsMediaRef(const ResourceItem &resourceItem) const
{
    return resourceItem.GetResType() == ResType::MEDIA &&
                FileEntry::FilePath(resourceItem.GetFilePath()).GetExtension() == JSON_EXTENSION;
}

bool ReferenceParser::IsProfileRef(const ResourceItem &resourceItem) const
{
    return resourceItem.GetResType() == ResType::PROF && resourceItem.GetLimitKey() == "base" &&
                FileEntry::FilePath(resourceItem.GetFilePath()).GetExtension() == JSON_EXTENSION;
}

bool ReferenceParser::ParseRefString(string &key) const
{
    bool update = false;
    return ParseRefString(key, update);
}

bool ReferenceParser::ParseRefString(std::string &key, bool &update, const std::string &filePath) const
{
    update = false;
    if (regex_match(key, regex("^\\$ohos:[a-z]+:.+"))) {
        update = true;
        return ParseRefImpl(key, ID_OHOS_REFS, true, filePath);
    } else if (regex_match(key, regex("^\\$[a-z]+:.+"))) {
        update = true;
        return ParseRefImpl(key, ID_REFS, false, filePath);
    }
    return true;
}

bool ReferenceParser::ParseRefImpl(string &key, const map<string, ResType> &refs, bool isSystem,
    const std::string &filePath) const
{
    for (const auto &ref : refs) {
        smatch result;
        if (regex_search(key, result, regex(ref.first))) {
            string name = key.substr(result[0].str().length());
            int64_t id = idWorker_.GetId(ref.second, name);
            if (!isSystem && ref.second == ResType::MEDIA && mediaJsonId_ != 0
                && layerIconIds_.find(mediaJsonId_) != layerIconIds_.end()) {
                layerIconIds_[mediaJsonId_].insert(id);
            }
            if (isSystem) {
                id = idWorker_.GetSystemId(ref.second, name);
            }
            if (id < 0) {
                PrintError(GetError(ERR_CODE_REF_NOT_DEFINED).FormatCause(key.c_str()).SetPosition(filePath));
                return false;
            }

            key = to_string(id);
            if (ref.second != ResType::ID) {
                key = "$" + ResourceUtil::ResTypeToString(ref.second) + ":" + to_string(id);
            }
            return true;
        }
    }
    string refer;
    for (const auto &ref:refs) {
        refer.append(ref.first).append(" ");
    }
    PrintError(GetError(ERR_CODE_INVALID_RESOURCE_REF).FormatCause(key.c_str(), refer.c_str()).SetPosition(filePath));
    return false;
}

bool ReferenceParser::ParseRefJsonImpl(cJSON *node, bool &needSave, const std::string &filePath) const
{
    if (cJSON_IsObject(node) || cJSON_IsArray(node)) {
        for (cJSON *item = node->child; item; item = item->next) {
            if (!ParseRefJsonImpl(item, needSave, filePath)) {
                return false;
            }
        }
    }  else if (cJSON_IsString(node)) {
        string value = node->valuestring;
        bool update = false;
        if (!ParseRefString(value, update, filePath)) {
            return false;
        }
        if (update) {
            needSave = update;
        }
        cJSON_SetValuestring(node, value.c_str());
    }
    return true;
}

std::map<int64_t, std::set<int64_t>> &ReferenceParser::GetLayerIconIds()
{
    return layerIconIds_;
}
}
}
}
