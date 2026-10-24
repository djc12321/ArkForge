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

#include "resconfig_parser.h"

#include <iostream>

#include "restool_errors.h"
#include "select_compile_parse.h"

namespace OHOS {
namespace Global {
namespace Restool {
using namespace std;
ResConfigParser::ResConfigParser() : root_(nullptr) {}
ResConfigParser::~ResConfigParser()
{
    if (root_) {
        cJSON_Delete(root_);
    }
}
uint32_t ResConfigParser::Init(const string &filePath, HandleBack callback)
{
    filePath_ = filePath;
    if (!ResourceUtil::OpenJsonFile(filePath, &root_)) {
        return RESTOOL_ERROR;
    }
    if (!root_ || !cJSON_IsObject(root_)) {
        PrintError(GetError(ERR_CODE_JSON_FORMAT_ERROR).SetPosition(filePath));
        return RESTOOL_ERROR;
    }
    if (!callback) {
        return RESTOOL_SUCCESS;
    }

    InitFileListCommand(callback);

    for (cJSON *item = root_->child; item; item = item->next) {
        auto handler = fileListHandles_.find(item->string);
        if (handler == fileListHandles_.end()) {
            cout << "Warning: unsupport " << item->string << endl;
            continue;
        }
        if (handler->second(item) != RESTOOL_SUCCESS) {
            return RESTOOL_ERROR;
        }
    }

    callback(Option::FORCEWRITE, "");
    return RESTOOL_SUCCESS;
}

// below private
void ResConfigParser::InitFileListCommand(HandleBack callback)
{
    using namespace placeholders;
    fileListHandles_.emplace("configPath", bind(&ResConfigParser::GetString, this, "configPath", _1,
        Option::JSON, callback));
    fileListHandles_.emplace("packageName", bind(&ResConfigParser::GetString, this, "packageName", _1,
        Option::PACKAGENAME, callback));
    fileListHandles_.emplace("output", bind(&ResConfigParser::GetString, this, "output", _1,
        Option::OUTPUTPATH, callback));
    fileListHandles_.emplace("startId", bind(&ResConfigParser::GetString, this, "startId", _1,
        Option::STARTID, callback));
    fileListHandles_.emplace("entryCompiledResource", bind(&ResConfigParser::GetString, this, "entryCompiledResource",
        _1, Option::DEPENDENTRY, callback));
    fileListHandles_.emplace("ids", bind(&ResConfigParser::GetString, this, "ids", _1, Option::IDS, callback));
    fileListHandles_.emplace("definedIds", bind(&ResConfigParser::GetString, this, "definedIds", _1,
        Option::DEFINED_IDS, callback));
    fileListHandles_.emplace("applicationResource", bind(&ResConfigParser::GetString, this, "applicationResource", _1,
        Option::INPUTPATH, callback));
    fileListHandles_.emplace("ResourceTable", bind(&ResConfigParser::GetArray, this, "ResourceTable", _1,
        Option::RESHEADER, callback));
    fileListHandles_.emplace("moduleResources", bind(&ResConfigParser::GetArray, this, "moduleResources", _1,
        Option::INPUTPATH, callback));
    fileListHandles_.emplace("dependencies", bind(&ResConfigParser::GetArray, this, "dependencies", _1,
        Option::INPUTPATH, callback));
    fileListHandles_.emplace("moduleNames", bind(&ResConfigParser::GetModuleNames, this, _1,
        Option::MODULES, callback));
    fileListHandles_.emplace("iconCheck", bind(&ResConfigParser::GetBool, this, "iconCheck", _1,
        Option::ICON_CHECK, callback));
    fileListHandles_.emplace("definedSysIds", bind(&ResConfigParser::GetString, this, "definedSysIds", _1,
        Option::DEFINED_SYSIDS, callback));
    fileListHandles_.emplace("compression", bind(&ResConfigParser::GetString, this, "compression", _1,
        Option::COMPRESSED_CONFIG, callback));
    fileListHandles_.emplace("thread", bind(&ResConfigParser::GetNumber, this, "thread", _1,
        Option::THREAD, callback));
    fileListHandles_.emplace("ignoreResourcePattern", bind(&ResConfigParser::GetIgnorePatterns, this,
        "ignoreResourcePattern", _1, Option::IGNORED_FILE));
    fileListHandles_.emplace("ignoreResourcePathPattern", bind(&ResConfigParser::GetIgnorePatterns, this,
        "ignoreResourcePathPattern", _1, Option::IGNORED_PATH));
    fileListHandles_.emplace("qualifiersConfig", bind(&ResConfigParser::GetQualifiersConfig, this,
        "qualifiersConfig", _1, Option::TARGET_CONFIG));
}

uint32_t ResConfigParser::GetString(const std::string &nodeName, const cJSON *node, int c, HandleBack callback)
{
    if (!node) {
        PrintError(GetError(ERR_CODE_JSON_NODE_MISSING).FormatCause(nodeName.c_str()).SetPosition(filePath_));
        return RESTOOL_ERROR;
    }
    if (!cJSON_IsString(node)) {
        PrintError(GetError(ERR_CODE_JSON_NODE_MISMATCH).FormatCause(node->string, "string").SetPosition(filePath_));
        return RESTOOL_ERROR;
    }

    if (callback(c, node->valuestring) != RESTOOL_SUCCESS) {
        return RESTOOL_ERROR;
    }
    return RESTOOL_SUCCESS;
}

uint32_t ResConfigParser::GetArray(const std::string &nodeName, const cJSON *node, int c, HandleBack callback)
{
    if (!node) {
        PrintError(GetError(ERR_CODE_JSON_NODE_MISSING).FormatCause(nodeName.c_str()).SetPosition(filePath_));
        return RESTOOL_ERROR;
    }
    if (!cJSON_IsArray(node)) {
        PrintError(GetError(ERR_CODE_JSON_NODE_MISMATCH).FormatCause(node->string, "array").SetPosition(filePath_));
        return RESTOOL_ERROR;
    }

    for (cJSON *item = node->child; item; item = item->next) {
        if (!cJSON_IsString(item)) {
            PrintError(GetError(ERR_CODE_JSON_NODE_MISMATCH).FormatCause(item->string, "string")
                .SetPosition(filePath_));
            return RESTOOL_ERROR;
        }
        if (callback(c, item->valuestring) != RESTOOL_SUCCESS) {
            return RESTOOL_ERROR;
        }
    }
    return RESTOOL_SUCCESS;
}

uint32_t ResConfigParser::GetModuleNames(const cJSON *node, int c, HandleBack callback)
{
    if (!node) {
        PrintError(GetError(ERR_CODE_JSON_NODE_MISSING).FormatCause("moduleNames").SetPosition(filePath_));
        return RESTOOL_ERROR;
    }
    if (cJSON_IsString(node)) {
        return GetString("moduleNames", node, c, callback);
    }
    string moduleNames;
    if (GetArray("moduleNames", node, c, [&moduleNames](int c, const string &argValue) {
        if (!moduleNames.empty()) {
            moduleNames.append(",");
        }
        moduleNames.append(argValue);
        return RESTOOL_SUCCESS;
    }) != RESTOOL_SUCCESS) {
        return RESTOOL_ERROR;
    }

    if (!moduleNames.empty() && callback(c, moduleNames) != RESTOOL_SUCCESS) {
        return RESTOOL_ERROR;
    }
    return RESTOOL_SUCCESS;
}

uint32_t ResConfigParser::GetBool(const std::string &nodeName, const cJSON *node, int c, HandleBack callback)
{
    if (!node) {
        PrintError(GetError(ERR_CODE_JSON_NODE_MISSING).FormatCause(nodeName.c_str()).SetPosition(filePath_));
        return RESTOOL_ERROR;
    }
    if (!cJSON_IsBool(node)) {
        PrintError(GetError(ERR_CODE_JSON_NODE_MISMATCH).FormatCause(node->string, "bool").SetPosition(filePath_));
        return RESTOOL_ERROR;
    }

    if (cJSON_IsTrue(node) == 1 && callback(c, "") != RESTOOL_SUCCESS) {
        return RESTOOL_ERROR;
    }
    return RESTOOL_SUCCESS;
}

uint32_t ResConfigParser::GetNumber(const std::string &nodeName, const cJSON *node, int c, HandleBack callback)
{
    if (!node) {
        PrintError(GetError(ERR_CODE_JSON_NODE_MISSING).FormatCause(nodeName.c_str()).SetPosition(filePath_));
        return RESTOOL_ERROR;
    }
    if (!cJSON_IsNumber(node)) {
        PrintError(GetError(ERR_CODE_JSON_NODE_MISMATCH).FormatCause(node->string, "number").SetPosition(filePath_));
        return RESTOOL_ERROR;
    }

    if (callback(c, std::to_string(node->valueint)) != RESTOOL_SUCCESS) {
        return RESTOOL_ERROR;
    }
    return RESTOOL_SUCCESS;
}

uint32_t ResConfigParser::GetIgnorePatterns(const std::string &nodeName, const cJSON *node, int c)
{
    if (!node) {
        PrintError(GetError(ERR_CODE_JSON_NODE_MISSING).FormatCause(nodeName.c_str()).SetPosition(filePath_));
        return RESTOOL_ERROR;
    }
    if (!ResourceUtil::CheckIgnoreOption(nodeName)) {
        return RESTOOL_ERROR;
    }
    ResourceUtil::SetIgnoreOption(nodeName);
    HandleBack callback = [nodeName](int c, const string &argValue) {
        bool isSucceed = ResourceUtil::AddIgnoreRegex(argValue, IgnoreType::IGNORE_ALL, nodeName);
        if (!isSucceed) {
            return RESTOOL_ERROR;
        }
        return RESTOOL_SUCCESS;
    };
    if (GetArray(nodeName, node, c, callback) != RESTOOL_SUCCESS) {
        return RESTOOL_ERROR;
    }
    return RESTOOL_SUCCESS;
}

uint32_t ResConfigParser::GetQualifiersConfig(const std::string &nodeName, const cJSON *node, int c)
{
    if (!node) {
        PrintError(GetError(ERR_CODE_JSON_NODE_MISSING).FormatCause(nodeName.c_str()).SetPosition(filePath_));
        return RESTOOL_ERROR;
    }
    if (!cJSON_IsObject(node)) {
        PrintError(GetError(ERR_CODE_JSON_NODE_MISMATCH).FormatCause(node->string, "object").SetPosition(filePath_));
        return RESTOOL_ERROR;
    }
    string configs;
    for (cJSON *qualifierNode = node->child; qualifierNode; qualifierNode = qualifierNode->next) {
        const string qualifier = qualifierNode->string;
        if (!cJSON_IsArray(qualifierNode)) {
            PrintError(GetError(ERR_CODE_JSON_NODE_MISMATCH).FormatCause(qualifier.c_str(), "array")
                .SetPosition(filePath_));
            return RESTOOL_ERROR;
        }
        if (cJSON_GetArraySize(qualifierNode) == 0) {
            PrintError(GetError(ERR_CODE_JSON_NODE_EMPTY).FormatCause(qualifier.c_str()).SetPosition(filePath_));
            return RESTOOL_ERROR;
        }
        configs.append(qualifier).append("[");
        for (cJSON *value = qualifierNode->child; value; value = value->next) {
            if (!cJSON_IsString(value)) {
                PrintError(GetError(ERR_CODE_JSON_NODE_MISMATCH)
                    .FormatCause(string(qualifier + "'s child").c_str(), "string").SetPosition(filePath_));
                return RESTOOL_ERROR;
            }
            configs.append(value->valuestring).append(",");
        }
        configs.pop_back();
        configs.append("]").append(";");
    }
    configs.pop_back();
    return SelectCompileParse::ParseTargetConfig(configs, nodeName);
}
}
}
}
