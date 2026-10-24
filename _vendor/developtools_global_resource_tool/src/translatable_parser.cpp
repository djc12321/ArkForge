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

#include "translatable_parser.h"
#include <algorithm>
#include <iostream>
#include "restool_errors.h"
#include "resource_util.h"

namespace OHOS {
namespace Global {
namespace Restool {
using namespace std;
const string TAG_VALUE = "value";
const string TAG_ATTR = "attr";
const string TAG_TRANSLATABLE = "translatable";
const string TAG_PRIORITY = "priority";
const string NO_TRANSLATE_START = "{noTranslateStart}";
const string NO_TRANSLATE_END = "{noTranslateEnd}";
const vector<string> PRIORITY_ATTRS = { "code", "translate", "LT", "customer" };

bool TranslatableParse::ParseTranslatable(cJSON *objectNode, const FileInfo &fileInfo, const string &name)
{
    if (fileInfo.fileType == ResType::STRING) {
        return ParseTranslatable(objectNode, fileInfo.filePath);
    }
    cJSON *arrayNode = cJSON_GetObjectItem(objectNode, TAG_VALUE.c_str());
    if (!arrayNode || !cJSON_IsArray(arrayNode)) {
        PrintError(GetError(ERR_CODE_JSON_NODE_MISMATCH).FormatCause(name.c_str(), "array")
            .SetPosition(fileInfo.filePath));
        return false;
    }

    if (cJSON_GetArraySize(arrayNode) == 0) {
        PrintError(GetError(ERR_CODE_JSON_NODE_EMPTY).FormatCause(name.c_str()).SetPosition(fileInfo.filePath));
        return false;
    }
    int32_t index = -1;
    for (cJSON *item = arrayNode->child; item; item = item->next) {
        index++;
        if (!item || !cJSON_IsObject(item)) {
            PrintError(GetError(ERR_CODE_JSON_NODE_MISMATCH)
                           .FormatCause("value's child", "object")
                           .SetPosition(fileInfo.filePath));
            return false;
        }
        
        if (!ParseTranslatable(item, fileInfo.filePath)) {
            return false;
        }
    }
    return true;
}

bool TranslatableParse::ParseTranslatable(cJSON *objectNode, const string &filePath)
{
    if (!CheckBaseStringAttr(objectNode, filePath)) {
        return false;
    }
    if (!ReplaceTranslateTags(objectNode, TAG_VALUE.c_str(), filePath)) {
        return false;
    }
    return true;
}

bool TranslatableParse::CheckBaseStringAttr(const cJSON *objectNode, const std::string &filePath)
{
    cJSON *attrNode = cJSON_GetObjectItem(objectNode, TAG_ATTR.c_str());
    if (!attrNode) {
        return true;
    }

    if (!cJSON_IsObject(attrNode)) {
        PrintError(
            GetError(ERR_CODE_JSON_NODE_MISMATCH).FormatCause(TAG_ATTR.c_str(), "object").SetPosition(filePath));
        return false;
    }
    return CheckBaseStringTranslatable(attrNode, filePath);
}

bool TranslatableParse::CheckBaseStringTranslatable(const cJSON *attrNode, const std::string &filePath)
{
    cJSON *translatableNode = cJSON_GetObjectItem(attrNode, TAG_TRANSLATABLE.c_str());
    if (!translatableNode) {
        return CheckBaseStringPriority(attrNode, filePath);
    }
    if (!cJSON_IsBool(translatableNode)) {
        PrintError(GetError(ERR_CODE_JSON_NODE_MISMATCH)
                       .FormatCause(TAG_TRANSLATABLE.c_str(), "bool")
                       .SetPosition(filePath));
        return false;
    }
    return CheckBaseStringPriority(attrNode, filePath);
}

bool TranslatableParse::CheckBaseStringPriority(const cJSON *attrNode, const std::string &filePath)
{
    cJSON *priorityNode = cJSON_GetObjectItem(attrNode, TAG_PRIORITY.c_str());
    if (!priorityNode) {
        return true;
    }
    if (!cJSON_IsString(priorityNode)) {
        PrintError(GetError(ERR_CODE_JSON_NODE_MISMATCH)
                       .FormatCause(TAG_PRIORITY.c_str(), "string")
                       .SetPosition(filePath));
        return false;
    }
    string priorityValue = priorityNode->valuestring;
    if (find(PRIORITY_ATTRS.begin(), PRIORITY_ATTRS.end(), priorityValue) == PRIORITY_ATTRS.end()) {
        string message("[");
        for (const auto &value : PRIORITY_ATTRS) {
            message.append("\"" + value + "\",");
        }
        message.pop_back();
        message.append("]");
        PrintError(GetError(ERR_CODE_INVALID_TRANSLATE_PRIORITY)
                       .FormatCause(priorityValue.c_str(), message.c_str())
                       .SetPosition(filePath));
        return false;
    }
    return true;
}

bool TranslatableParse::ReplaceTranslateTags(cJSON *node, const char *key, const std::string &filePath)
{
    cJSON *valueNode = cJSON_GetObjectItem(node, TAG_VALUE.c_str());
    if (!valueNode || !cJSON_IsString(valueNode)) {
        PrintError(GetError(ERR_CODE_JSON_NODE_MISMATCH).FormatCause(TAG_VALUE.c_str(), "string")
            .SetPosition(filePath));
        return false;
    }

    string value = valueNode->valuestring;
    if (GetReplaceStringTranslate(value)) {
        return cJSON_ReplaceItemInObject(node, key, cJSON_CreateString(value.c_str()));
    }
    return true;
}

bool TranslatableParse::GetReplaceStringTranslate(string &str)
{
    vector<size_t> posData;
    if (!FindTranslatePairs(str, posData) || posData.empty()) {
        return false;
    }
    size_t startIndex = 0;
    string tmp;
    for (size_t index = 0; index < posData.size(); index++) {
        tmp.append(str, startIndex, posData[index] - startIndex);
        startIndex = posData[++index];
    }
    str = tmp.append(str, posData.back());
    return true;
}

bool TranslatableParse::FindTranslatePairs(const string &str, vector<size_t> &posData)
{
    auto startPos = str.find(NO_TRANSLATE_START, 0);
    auto endPos = str.find(NO_TRANSLATE_END, 0);
    auto startLength = NO_TRANSLATE_START.length();
    auto endLength = NO_TRANSLATE_END.length();
    while (!(startPos == string::npos && endPos == string::npos)) {
        if (startPos == string::npos || endPos == string::npos) {
            return false;
        }
        if (startPos >= endPos || (!posData.empty() && posData.back() >= startPos)) {
            return false;
        }
        posData.emplace_back(startPos);
        posData.emplace_back(startPos + startLength);
        posData.emplace_back(endPos);
        posData.emplace_back(endPos + endLength);
        startPos = str.find(NO_TRANSLATE_START, startPos + startLength);
        endPos = str.find(NO_TRANSLATE_END, endPos + endLength);
    }
    return true;
}
}
}
}
