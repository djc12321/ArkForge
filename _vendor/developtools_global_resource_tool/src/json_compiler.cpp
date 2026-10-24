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

#include "json_compiler.h"
#include <iostream>
#include <limits>
#include <regex>
#include "restool_errors.h"
#include "translatable_parser.h"

namespace OHOS {
namespace Global {
namespace Restool {
using namespace std;
const string TAG_NAME = "name";
const string TAG_VALUE = "value";
const string TAG_PARENT = "parent";
const string TAG_QUANTITY = "quantity";
const vector<string> QUANTITY_ATTRS = { "zero", "one", "two", "few", "many", "other" };
const vector<string> TRANSLATION_TYPE = { "string", "strarray", "plural" };

JsonCompiler::JsonCompiler(ResType type, const string &output, bool isOverlap, bool isHarResource)
    : IResourceCompiler(type, output, isOverlap, isHarResource), isBaseString_(false), root_(nullptr)
{
    InitParser();
}

JsonCompiler::~JsonCompiler()
{
    if (root_) {
        cJSON_Delete(root_);
    }
}

uint32_t JsonCompiler::CompileSingleFile(const FileInfo &fileInfo)
{
    if (fileInfo.limitKey == "base" &&
        fileInfo.fileCluster == "element" &&
        fileInfo.filename == ID_DEFINED_FILE) {
        return RESTOOL_SUCCESS;
    }

    if (!ResourceUtil::OpenJsonFile(fileInfo.filePath, &root_)) {
        return RESTOOL_ERROR;
    }
    if (!root_ || !cJSON_IsObject(root_)) {
        PrintError(GetError(ERR_CODE_JSON_FORMAT_ERROR).SetPosition(fileInfo.filePath));
        return RESTOOL_ERROR;
    }
    cJSON *item = root_->child;
    if (cJSON_GetArraySize(root_) != 1) {
        PrintError(GetError(ERR_CODE_JSON_NOT_ONE_MEMBER).FormatCause("root").SetPosition(fileInfo.filePath));
        return RESTOOL_ERROR;
    }

    string tag = item->string;
    auto ret = g_contentClusterMap.find(tag);
    if (ret == g_contentClusterMap.end()) {
        PrintError(GetError(ERR_CODE_JSON_INVALID_NODE_NAME)
                       .FormatCause(tag.c_str(), ResourceUtil::GetAllRestypeString().c_str())
                       .SetPosition(fileInfo.filePath));
        return RESTOOL_ERROR;
    }
    isBaseString_ = (fileInfo.limitKey == "base" &&
        find(TRANSLATION_TYPE.begin(), TRANSLATION_TYPE.end(), tag) != TRANSLATION_TYPE.end());
    FileInfo copy = fileInfo;
    copy.fileType = ret->second;
    if (!ParseJsonArrayLevel(item, copy)) {
        return RESTOOL_ERROR;
    }
    return RESTOOL_SUCCESS;
}

// below private
void JsonCompiler::InitParser()
{
    using namespace placeholders;
    handles_.emplace(ResType::STRING, bind(&JsonCompiler::HandleString, this, _1, _2));
    handles_.emplace(ResType::INTEGER, bind(&JsonCompiler::HandleInteger, this, _1, _2));
    handles_.emplace(ResType::BOOLEAN, bind(&JsonCompiler::HandleBoolean, this, _1, _2));
    handles_.emplace(ResType::COLOR, bind(&JsonCompiler::HandleColor, this, _1, _2));
    handles_.emplace(ResType::FLOAT, bind(&JsonCompiler::HandleFloat, this, _1, _2));
    handles_.emplace(ResType::STRARRAY, bind(&JsonCompiler::HandleStringArray, this, _1, _2));
    handles_.emplace(ResType::INTARRAY, bind(&JsonCompiler::HandleIntegerArray, this, _1, _2));
    handles_.emplace(ResType::THEME, bind(&JsonCompiler::HandleTheme, this, _1, _2));
    handles_.emplace(ResType::PATTERN, bind(&JsonCompiler::HandlePattern, this, _1, _2));
    handles_.emplace(ResType::PLURAL, bind(&JsonCompiler::HandlePlural, this, _1, _2));
    handles_.emplace(ResType::SYMBOL, bind(&JsonCompiler::HandleSymbol, this, _1, _2));
}

bool JsonCompiler::ParseJsonArrayLevel(const cJSON *arrayNode, const FileInfo &fileInfo)
{
    if (!arrayNode || !cJSON_IsArray(arrayNode)) {
        PrintError(GetError(ERR_CODE_JSON_NODE_MISMATCH)
                       .FormatCause(ResourceUtil::ResTypeToString(fileInfo.fileType).c_str(), "array")
                       .SetPosition(fileInfo.filePath));
        return false;
    }

    if (cJSON_GetArraySize(arrayNode) == 0) {
        PrintError(GetError(ERR_CODE_JSON_NODE_EMPTY)
                       .FormatCause(ResourceUtil::ResTypeToString(fileInfo.fileType).c_str())
                       .SetPosition(fileInfo.filePath));
        return false;
    }
    int32_t index = -1;
    for (cJSON *item = arrayNode->child; item; item = item->next) {
        index++;
        if (!item || !cJSON_IsObject(item)) {
            PrintError(GetError(ERR_CODE_JSON_NODE_MISMATCH).FormatCause("item", "object")
                .SetPosition(fileInfo.filePath));
            return false;
        }
        if (!ParseJsonObjectLevel(item, fileInfo)) {
            return false;
        }
    }
    return true;
}

bool JsonCompiler::ParseJsonObjectLevel(cJSON *objectNode, const FileInfo &fileInfo)
{
    cJSON *nameNode = cJSON_GetObjectItem(objectNode, TAG_NAME.c_str());
    if (!nameNode) {
        PrintError(GetError(ERR_CODE_JSON_NODE_MISSING).FormatCause("name").SetPosition(fileInfo.filePath));
        return false;
    }

    if (!cJSON_IsString(nameNode)) {
        PrintError(GetError(ERR_CODE_JSON_NODE_MISMATCH).FormatCause(TAG_NAME.c_str(), "string")
            .SetPosition(fileInfo.filePath));
        return false;
    }

    if (isBaseString_ && !TranslatableParse::ParseTranslatable(objectNode, fileInfo, nameNode->valuestring)) {
        return false;
    }
    ResourceItem resourceItem(nameNode->valuestring, fileInfo.keyParams, fileInfo.fileType);
    resourceItem.SetFilePath(fileInfo.filePath);
    resourceItem.SetLimitKey(fileInfo.limitKey);
    auto ret = handles_.find(fileInfo.fileType);
    if (ret == handles_.end()) {
        std::string elementTypes = "[";
        for (const auto &handle : handles_) {
            elementTypes.append("\"").append(ResourceUtil::ResTypeToString(handle.first)).append("\",");
        }
        elementTypes.pop_back();
        elementTypes.append("]");
        PrintError(GetError(ERR_CODE_INVALID_ELEMENT_TYPE)
                       .FormatCause(ResourceUtil::ResTypeToString(fileInfo.fileType).c_str(), elementTypes.c_str())
                       .SetPosition(fileInfo.filePath));
        return false;
    }

    if (!ret->second(objectNode, resourceItem)) {
        return false;
    }

    if (isOverlap_) {
        resourceItem.MarkCoverable();
    }

    return MergeResourceItem(resourceItem);
}

bool JsonCompiler::HandleString(const cJSON *objectNode, ResourceItem &resourceItem) const
{
    cJSON *valueNode = cJSON_GetObjectItem(objectNode, TAG_VALUE.c_str());
    if (!CheckJsonStringValue(valueNode, resourceItem)) {
        return false;
    }
    return PushString(valueNode->valuestring, resourceItem);
}

bool JsonCompiler::HandleInteger(const cJSON *objectNode, ResourceItem &resourceItem) const
{
    cJSON *valueNode = cJSON_GetObjectItem(objectNode, TAG_VALUE.c_str());
    if (!CheckJsonIntegerValue(valueNode, resourceItem)) {
        return false;
    }
    if (cJSON_IsString(valueNode)) {
        return PushString(valueNode->valuestring, resourceItem);
    } else if (cJSON_IsNumber(valueNode)) {
        return PushString(to_string(valueNode->valueint), resourceItem);
    } else {
        return false;
    }
}

bool JsonCompiler::HandleBoolean(const cJSON *objectNode, ResourceItem &resourceItem) const
{
    cJSON *valueNode = cJSON_GetObjectItem(objectNode, TAG_VALUE.c_str());
    if (valueNode == nullptr) {
        PrintError(GetError(ERR_CODE_JSON_NODE_MISSING).FormatCause(string(resourceItem.GetName() + " value").c_str())
            .SetPosition(resourceItem.GetFilePath()));
        return false;
    }
    if (cJSON_IsString(valueNode)) {
        regex ref("^\\$(ohos:)?boolean:.*");
        if (!regex_match(valueNode->valuestring, ref)) {
            PrintError(GetError(ERR_CODE_INVALID_RESOURCE_REF).FormatCause(valueNode->valuestring, "$(ohos:)?boolean:")
                .SetPosition(resourceItem.GetFilePath()));
            return false;
        }
        return PushString(valueNode->valuestring, resourceItem);
    }
    if (!cJSON_IsBool(valueNode)) {
        PrintError(GetError(ERR_CODE_JSON_NODE_MISMATCH)
                       .FormatCause(string(resourceItem.GetName() + " value").c_str(), "bool")
                       .SetPosition(resourceItem.GetFilePath()));
        return false;
    }
    return PushString(cJSON_IsTrue(valueNode) == 1 ? "true" : "false", resourceItem);
}

bool JsonCompiler::HandleColor(const cJSON *objectNode, ResourceItem &resourceItem) const
{
    return HandleString(objectNode, resourceItem);
}

bool JsonCompiler::HandleFloat(const cJSON *objectNode, ResourceItem &resourceItem) const
{
    return HandleString(objectNode, resourceItem);
}

bool JsonCompiler::HandleStringArray(const cJSON *objectNode, ResourceItem &resourceItem) const
{
    vector<string> extra;
    return ParseValueArray(objectNode, resourceItem, extra,
        [this](const cJSON *arrayItem, const ResourceItem &resourceItem, vector<string> &values) -> bool {
            if (!cJSON_IsObject(arrayItem)) {
                PrintError(GetError(ERR_CODE_JSON_NODE_MISMATCH)
                               .FormatCause(string(resourceItem.GetName() + " value").c_str(), "object")
                               .SetPosition(resourceItem.GetFilePath()));
                return false;
            }
            cJSON *valueNode = cJSON_GetObjectItem(arrayItem, TAG_VALUE.c_str());
            if (!CheckJsonStringValue(valueNode, resourceItem)) {
                return false;
            }
            values.push_back(valueNode->valuestring);
            return true;
    });
}

bool JsonCompiler::HandleIntegerArray(const cJSON *objectNode, ResourceItem &resourceItem) const
{
    vector<string> extra;
    return ParseValueArray(objectNode, resourceItem, extra,
        [this](const cJSON *arrayItem, const ResourceItem &resourceItem, vector<string> &values) -> bool {
            if (!CheckJsonIntegerValue(arrayItem, resourceItem)) {
                return false;
            }
            if (cJSON_IsString(arrayItem)) {
                values.push_back(arrayItem->valuestring);
            } else {
                values.push_back(to_string(arrayItem->valueint));
            }
            return true;
    });
}

bool JsonCompiler::HandleTheme(const cJSON *objectNode, ResourceItem &resourceItem) const
{
    vector<string> extra;
    if (!ParseParent(objectNode, resourceItem, extra)) {
        return false;
    }
    return ParseValueArray(objectNode, resourceItem, extra,
        [this](const cJSON *arrayItem, const ResourceItem &resourceItem, vector<string> &values) {
            return ParseAttribute(arrayItem, resourceItem, values);
        });
}

bool JsonCompiler::HandlePattern(const cJSON *objectNode, ResourceItem &resourceItem) const
{
    return HandleTheme(objectNode, resourceItem);
}

bool JsonCompiler::HandlePlural(const cJSON *objectNode, ResourceItem &resourceItem) const
{
    vector<string> extra;
    vector<string> attrs;
    bool result = ParseValueArray(objectNode, resourceItem, extra,
        [&attrs, this](const cJSON *arrayItem, const ResourceItem &resourceItem, vector<string> &values) {
            if (!CheckPluralValue(arrayItem, resourceItem)) {
                return false;
            }
            cJSON *quantityNode = cJSON_GetObjectItem(arrayItem, TAG_QUANTITY.c_str());
            if (!quantityNode || !cJSON_IsString(quantityNode)) {
                return false;
            }
            string quantityValue = quantityNode->valuestring;
            if (find(attrs.begin(), attrs.end(), quantityValue) != attrs.end()) {
                PrintError(GetError(ERR_CODE_DUPLICATE_QUANTITY)
                               .FormatCause(quantityValue.c_str(), resourceItem.GetName().c_str())
                               .SetPosition(resourceItem.GetFilePath()));
                return false;
            }
            attrs.push_back(quantityValue);
            values.push_back(quantityValue);
            cJSON *valueNode = cJSON_GetObjectItem(arrayItem, TAG_VALUE.c_str());
            if (!valueNode || !cJSON_IsString(valueNode)) {
                return false;
            }
            values.push_back(valueNode->valuestring);
            return true;
        });
    if (!result) {
        return false;
    }
    if (find(attrs.begin(), attrs.end(), "other") == attrs.end()) {
        PrintError(GetError(ERR_CODE_QUANTITY_NO_OTHER).FormatCause(resourceItem.GetName().c_str())
            .SetPosition(resourceItem.GetFilePath()));
        return false;
    }
    return true;
}

bool JsonCompiler::HandleSymbol(const cJSON *objectNode, ResourceItem &resourceItem) const
{
    cJSON *valueNode = cJSON_GetObjectItem(objectNode, TAG_VALUE.c_str());
    if (!CheckJsonSymbolValue(valueNode, resourceItem)) {
        return false;
    }
    return PushString(valueNode->valuestring, resourceItem);
}

bool JsonCompiler::PushString(const string &value, ResourceItem &resourceItem) const
{
    if (!resourceItem.SetData(reinterpret_cast<const int8_t *>(value.c_str()), value.length())) {
        std::string msg = "item data is null, resource name: " + resourceItem.GetName();
        PrintError(GetError(ERR_CODE_UNDEFINED_ERROR).FormatCause(msg.c_str()).SetPosition(resourceItem.GetFilePath()));
        return false;
    }
    return true;
}

bool JsonCompiler::CheckJsonStringValue(const cJSON *valueNode, const ResourceItem &resourceItem) const
{
    if (!valueNode || !cJSON_IsString(valueNode)) {
        PrintError(GetError(ERR_CODE_JSON_NODE_MISMATCH)
                       .FormatCause(string(resourceItem.GetName() + " value").c_str(), "string")
                       .SetPosition(resourceItem.GetFilePath()));
        return false;
    }

    const map<ResType, string> REFS = {
        { ResType::STRING, "\\$(ohos:)?string:" },
        { ResType::STRARRAY, "\\$(ohos:)?string:" },
        { ResType::COLOR, "\\$(ohos:)?color:" },
        { ResType::FLOAT, "\\$(ohos:)?float:" }
    };

    string value = valueNode->valuestring;
    ResType type = resourceItem.GetResType();
    if (type ==  ResType::COLOR && !CheckColorValue(value.c_str())) {
        PrintError(GetError(ERR_CODE_INVALID_COLOR_VALUE).FormatCause(value.c_str(), resourceItem.GetName().c_str())
            .SetPosition(resourceItem.GetFilePath()));
        return false;
    }
    regex ref("^\\$.+:");
    smatch result;
    if (regex_search(value, result, ref) && !regex_match(result[0].str(), regex(REFS.at(type)))) {
        PrintError(GetError(ERR_CODE_INVALID_RESOURCE_REF).FormatCause(value.c_str(), REFS.at(type).c_str())
            .SetPosition(resourceItem.GetFilePath()));
        return false;
    }
    return true;
}

bool JsonCompiler::CheckJsonIntegerValue(const cJSON *valueNode, const ResourceItem &resourceItem) const
{
    if (!valueNode) {
        PrintError(GetError(ERR_CODE_JSON_NODE_MISSING).FormatCause(string(resourceItem.GetName() + " value").c_str())
            .SetPosition(resourceItem.GetFilePath()));
        return false;
    }
    if (cJSON_IsString(valueNode)) {
        regex ref("^\\$(ohos:)?integer:.*");
        if (!regex_match(valueNode->valuestring, ref)) {
            PrintError(GetError(ERR_CODE_INVALID_RESOURCE_REF).FormatCause(valueNode->valuestring, "$(ohos:)?integer:")
                .SetPosition(resourceItem.GetFilePath()));
            return false;
        }
    } else if (!ResourceUtil::IsIntValue(valueNode)) {
        PrintError(GetError(ERR_CODE_JSON_NODE_MISMATCH)
                       .FormatCause(string(resourceItem.GetName() + " value").c_str(), "integer")
                       .SetPosition(resourceItem.GetFilePath()));
        return false;
    }
    return true;
}

bool JsonCompiler::CheckJsonSymbolValue(const cJSON *valueNode, const ResourceItem &resourceItem) const
{
    if (!valueNode || !cJSON_IsString(valueNode)) {
        PrintError(GetError(ERR_CODE_JSON_NODE_MISMATCH)
                       .FormatCause(string(resourceItem.GetName() + " value").c_str(), "string")
                       .SetPosition(resourceItem.GetFilePath()));
        return false;
    }
    string unicodeStr = valueNode->valuestring;
    if (regex_match(unicodeStr, regex("^\\$(ohos:)?symbol:.*"))) {
        return true;
    }
    int unicode = 0;
    if (!ResourceUtil::StrToInt(unicodeStr, unicode, HEX_BASE) || !ResourceUtil::isUnicodeInPlane15or16(unicode)) {
        PrintError(GetError(ERR_CODE_INVALID_SYMBOL).FormatCause(unicodeStr.c_str(), resourceItem.GetName().c_str())
            .SetPosition(resourceItem.GetFilePath()));
        return false;
    }
    return true;
}

bool JsonCompiler::ParseValueArray(const cJSON *objectNode, ResourceItem &resourceItem,
                                   const vector<string> &extra, HandleValue callback) const
{
    cJSON *arrayNode = cJSON_GetObjectItem(objectNode, TAG_VALUE.c_str());
    if (arrayNode == nullptr) {
        PrintError(GetError(ERR_CODE_JSON_NODE_MISSING)
                       .FormatCause(string(resourceItem.GetName() + " value").c_str())
                       .SetPosition(resourceItem.GetFilePath()));
        return false;
    }

    if (!cJSON_IsArray(arrayNode)) {
        PrintError(GetError(ERR_CODE_JSON_NODE_MISMATCH)
                       .FormatCause(string(resourceItem.GetName() + " value").c_str(), "array")
                       .SetPosition(resourceItem.GetFilePath()));
        return false;
    }

    if (cJSON_GetArraySize(arrayNode) == 0) {
        PrintError(GetError(ERR_CODE_JSON_NODE_EMPTY)
                       .FormatCause(string(resourceItem.GetName() + " value").c_str())
                       .SetPosition(resourceItem.GetFilePath()));
        return false;
    }

    vector<string> contents;
    if (!extra.empty()) {
        contents.assign(extra.begin(), extra.end());
    }
    for (cJSON *item = arrayNode->child; item; item = item->next) {
        vector<string> values;
        if (!callback(item, resourceItem, values)) {
            return false;
        }
        contents.insert(contents.end(), values.begin(), values.end());
    }

    string data = ResourceUtil::ComposeStrings(contents);
    if (data.empty()) {
        PrintError(GetError(ERR_CODE_ARRAY_TOO_LARGE).FormatCause(resourceItem.GetName().c_str())
            .SetPosition(resourceItem.GetFilePath()));
        return false;
    }
    return PushString(data, resourceItem);
}

bool JsonCompiler::ParseParent(const cJSON *objectNode, const ResourceItem &resourceItem,
                               vector<string> &extra) const
{
    cJSON *parentNode = cJSON_GetObjectItem(objectNode, TAG_PARENT.c_str());
    string type = ResourceUtil::ResTypeToString(resourceItem.GetResType());
    if (parentNode) {
        if (!cJSON_IsString(parentNode)) {
            PrintError(GetError(ERR_CODE_JSON_NODE_MISMATCH)
                           .FormatCause(string(resourceItem.GetName() + " parent").c_str(), "string")
                           .SetPosition(resourceItem.GetFilePath()));
            return false;
        }
        string parentValue = parentNode->valuestring;
        if (parentValue.empty()) {
            PrintError(GetError(ERR_CODE_PARENT_EMPTY).FormatCause(resourceItem.GetName().c_str())
                .SetPosition(resourceItem.GetFilePath()));
            return false;
        }
        if (regex_match(parentValue, regex("^ohos:" + type + ":.+"))) {
            parentValue = "$" + parentValue;
        } else {
            parentValue = "$" + type + ":" + parentValue;
        }
        extra.push_back(parentValue);
    }
    return true;
}

bool JsonCompiler::ParseAttribute(const cJSON *arrayItem, const ResourceItem &resourceItem,
                                  vector<string> &values) const
{
    string type = ResourceUtil::ResTypeToString(resourceItem.GetResType());
    string nodeAttr = string(resourceItem.GetName() + " attribute");
    string filePath = resourceItem.GetFilePath();
    if (!arrayItem || !cJSON_IsObject(arrayItem)) {
        PrintError(GetError(ERR_CODE_JSON_NODE_MISMATCH).FormatCause(nodeAttr.c_str(), "object")
            .SetPosition(filePath));
        return false;
    }
    cJSON *nameNode = cJSON_GetObjectItem(arrayItem, TAG_NAME.c_str());
    if (!nameNode) {
        PrintError(GetError(ERR_CODE_JSON_NODE_MISSING).FormatCause(nodeAttr.c_str()).SetPosition(filePath));
        return false;
    }
    if (!cJSON_IsString(nameNode)) {
        PrintError(GetError(ERR_CODE_JSON_NODE_MISMATCH).FormatCause(string(nodeAttr + " name").c_str(), "string")
            .SetPosition(filePath));
        return false;
    }
    values.push_back(nameNode->valuestring);

    cJSON *valueNode = cJSON_GetObjectItem(arrayItem, TAG_VALUE.c_str());
    if (!valueNode) {
        PrintError(GetError(ERR_CODE_JSON_NODE_MISSING)
                       .FormatCause(string(nodeAttr + " '" + nameNode->valuestring + "'").c_str())
                       .SetPosition(filePath));
        return false;
    }
    if (!cJSON_IsString(valueNode)) {
        PrintError(GetError(ERR_CODE_JSON_NODE_MISMATCH)
                       .FormatCause(string(nodeAttr + " '" + nameNode->valuestring + "'").c_str(), "object")
                       .SetPosition(filePath));
        return false;
    }
    values.push_back(valueNode->valuestring);
    return true;
}

bool JsonCompiler::CheckPluralValue(const cJSON *arrayItem, const ResourceItem &resourceItem) const
{
    string filePath = resourceItem.GetFilePath();
    if (!arrayItem || !cJSON_IsObject(arrayItem)) {
        PrintError(GetError(ERR_CODE_JSON_NODE_MISMATCH)
                       .FormatCause(string(resourceItem.GetName() + " array item").c_str(), "object")
                       .SetPosition(filePath));
        return false;
    }
    cJSON *quantityNode = cJSON_GetObjectItem(arrayItem, TAG_QUANTITY.c_str());
    if (!quantityNode) {
        PrintError(GetError(ERR_CODE_JSON_NODE_MISSING)
                       .FormatCause(string(resourceItem.GetName() + " quantity").c_str())
                       .SetPosition(filePath));
        return false;
    }
    if (!cJSON_IsString(quantityNode)) {
        PrintError(GetError(ERR_CODE_JSON_NODE_MISMATCH)
                       .FormatCause(string(resourceItem.GetName() + " quantity").c_str(), "string")
                       .SetPosition(filePath));
        return false;
    }
    string quantityValue = quantityNode->valuestring;
    if (find(QUANTITY_ATTRS.begin(), QUANTITY_ATTRS.end(), quantityValue) == QUANTITY_ATTRS.end()) {
        string buffer("[");
        for_each(QUANTITY_ATTRS.begin(), QUANTITY_ATTRS.end(), [&buffer](auto iter) {
            buffer.append("\"").append(iter).append("\",");
        });
        buffer.pop_back();
        buffer.append("]");
        PrintError(GetError(ERR_CODE_INVALID_QUANTITY)
                       .FormatCause(quantityValue.c_str(), resourceItem.GetName().c_str(), buffer.c_str())
                       .SetPosition(filePath));
        return false;
    }

    cJSON *valueNode = cJSON_GetObjectItem(arrayItem, TAG_VALUE.c_str());
    if (!valueNode) {
        PrintError(GetError(ERR_CODE_JSON_NODE_MISSING)
                       .FormatCause(string(resourceItem.GetName() + " '" + quantityValue + "' value").c_str())
                       .SetPosition(filePath));
        return false;
    }
    if (!cJSON_IsString(valueNode)) {
        PrintError(GetError(ERR_CODE_JSON_NODE_MISMATCH)
                       .FormatCause(string(resourceItem.GetName() + " '" + quantityValue + "' value").c_str(), "string")
                       .SetPosition(filePath));
        return false;
    }
    return true;
}

bool JsonCompiler::CheckColorValue(const char *s) const
{
    if (s == nullptr) {
        return false;
    }
    // color regex
    string regColor = "^#([A-Fa-f0-9]{3}|[A-Fa-f0-9]{4}|[A-Fa-f0-9]{6}|[A-Fa-f0-9]{8})$";
    if (regex_match(s, regex("^\\$.*")) || regex_match(s, regex(regColor))) {
        return true;
    }
    return false;
}
}
}
}
