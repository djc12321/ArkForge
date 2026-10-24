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

#include "resource_dumper.h"

#include <cstdint>
#include <iostream>
#include <memory>
#include <ostream>
#include <set>
#include <sstream>
#include <functional>
#include "cJSON.h"
#include "resource_item.h"
#include "resource_table.h"
#include "resource_util.h"
#include "restool_errors.h"


namespace OHOS {
namespace Global {
namespace Restool {

constexpr int PAIR_SIZE = 2;

static std::map<std::string, std::function<std::unique_ptr<ResourceDumper>()>> dumpMap_ = {
    {"config", std::make_unique<ConfigDumper>}
};

uint32_t ResourceDumper::Dump(const DumpParserBase &packageParser)
{
    inputPath_ = packageParser.GetInputPath();
    if (LoadHap() != RESTOOL_SUCCESS) {
        return RESTOOL_ERROR;
    }
    std::string jsonStr;
    if (DumpRes(jsonStr) != RESTOOL_SUCCESS) {
        return RESTOOL_ERROR;
    }
    std::cout << jsonStr << std::endl;
    return RESTOOL_SUCCESS;
}

uint32_t ResourceDumper::LoadHap()
{
    unzFile zipFile = unzOpen64(inputPath_.c_str());
    if (!zipFile) {
        PrintError(GetError(ERR_CODE_PARSE_HAP_ERROR).FormatCause("can't unzip hap").SetPosition(inputPath_));
        return RESTOOL_ERROR;
    }
    std::unique_ptr<char[]> buffer;
    size_t len;
    if (ReadFileFromZip(zipFile, "module.json", buffer, len) == RESTOOL_SUCCESS) {
        ReadHapInfo(buffer, len);
    } else if (ReadFileFromZip(zipFile, "config.json", buffer, len) == RESTOOL_SUCCESS) {
        ReadHapInfo(buffer, len);
    }
    if (ReadFileFromZip(zipFile, "resources.index", buffer, len) != RESTOOL_SUCCESS) {
        unzClose(zipFile);
        PrintError(GetError(ERR_CODE_PARSE_HAP_ERROR)
            .FormatCause("read resources.index failed").SetPosition(inputPath_));
        return RESTOOL_ERROR;
    }
    unzClose(zipFile);
    std::stringstream stream;
    stream.write(buffer.get(), len);
    if (ResourceTable::LoadResTable(stream, resInfos_) != RESTOOL_SUCCESS) {
        return RESTOOL_ERROR;
    }
    return RESTOOL_SUCCESS;
}

void ResourceDumper::ReadHapInfo(const std::unique_ptr<char[]> &buffer, size_t len)
{
    cJSON *config = cJSON_Parse(buffer.get());
    if (!config) {
        return;
    }
    cJSON *appConfig = cJSON_GetObjectItemCaseSensitive(config, "app");
    cJSON *moduleConfig = cJSON_GetObjectItemCaseSensitive(config, "module");
    if (appConfig) {
        cJSON *bundleName = cJSON_GetObjectItemCaseSensitive(appConfig, "bundleName");
        if (cJSON_IsString(bundleName) && bundleName->valuestring) {
            bundleName_ = bundleName->valuestring;
        }
    }
    if (moduleConfig) {
        cJSON *moduleName = cJSON_GetObjectItemCaseSensitive(moduleConfig, "name");
        if (cJSON_IsString(moduleName) && moduleName->valuestring) {
            moduleName_ = moduleName->valuestring;
        }
    }
    cJSON_Delete(config);
}

uint32_t ResourceDumper::ReadFileFromZip(
    unzFile &zipFile, const char *fileName, std::unique_ptr<char[]> &buffer, size_t &len)
{
    unz_file_info fileInfo;
    if (unzLocateFile2(zipFile, fileName, 1) != UNZ_OK) {
        return RESTOOL_ERROR;
    }
    char filenameInZip[256];
    int err = unzGetCurrentFileInfo(zipFile, &fileInfo, filenameInZip, sizeof(filenameInZip), nullptr, 0, nullptr, 0);
    if (err != UNZ_OK) {
        return RESTOOL_ERROR;
    }

    len = fileInfo.uncompressed_size;
    if (len == 0 || len > SIZE_MAX - 1) {
        PrintError(GetError(ERR_CODE_PARSE_HAP_ERROR).FormatCause("invalid file size in zipfile"));
        return RESTOOL_ERROR;
    }
    buffer = std::make_unique<char[]>(len + 1);
    if (!buffer) {
        return RESTOOL_ERROR;
    }

    err = unzOpenCurrentFilePassword(zipFile, nullptr);
    if (err != UNZ_OK) {
        return RESTOOL_ERROR;
    }
    err = unzReadCurrentFile(zipFile, buffer.get(), len);
    if (err < 0 || (size_t)err != len) {
        PrintError(GetError(ERR_CODE_PARSE_HAP_ERROR).FormatCause("read file from zip failed"));
        return RESTOOL_ERROR;
    }
    buffer[len] = '\0';
    return RESTOOL_SUCCESS;
}

uint32_t CommonDumper::DumpRes(std::string &out) const
{
    std::unique_ptr<cJSON, std::function<void(cJSON*)>> root(cJSON_CreateObject(),
        [](cJSON* node) { cJSON_Delete(node); });
    if (!root) {
        PrintError(GetError(ERR_CODE_UNDEFINED_ERROR).FormatCause("unable to create cJSON object for root object"));
        return RESTOOL_ERROR;
    }
    if (!bundleName_.empty() && !moduleName_.empty()) {
        cJSON *bundleName = cJSON_CreateString(bundleName_.c_str());
        cJSON *moduleName = cJSON_CreateString(moduleName_.c_str());
        if (bundleName) {
            cJSON_AddItemToObject(root.get(), "bundleName", bundleName);
        }
        if (moduleName) {
            cJSON_AddItemToObject(root.get(), "moduleName", moduleName);
        }
    }
    cJSON *resource = cJSON_CreateArray();
    if (!resource) {
        PrintError(GetError(ERR_CODE_UNDEFINED_ERROR)
            .FormatCause("unable to create cJSON object for resource array"));
        return RESTOOL_ERROR;
    }
    cJSON_AddItemToObject(root.get(), "resource", resource);
    for (auto it = resInfos_.begin(); it != resInfos_.end(); it++) {
        if (AddResourceToJson(it->first, it->second, resource)) {
            return RESTOOL_ERROR;
        }
    }
    char *rawStr = cJSON_Print(root.get());
    if (!rawStr) {
        PrintError(GetError(ERR_CODE_UNDEFINED_ERROR).FormatCause("covert json object to str failed"));
        return RESTOOL_ERROR;
    }
    out = rawStr;
    cJSON_free(rawStr);
    rawStr = nullptr;
    return RESTOOL_SUCCESS;
}

uint32_t CommonDumper::AddKeyParamsToJson(const std::vector<KeyParam> &keyParams, cJSON *json) const
{
    if (!json) {
        PrintError(GetError(ERR_CODE_UNDEFINED_ERROR).FormatCause("add keyparam to null json object"));
        return RESTOOL_ERROR;
    }
    for (const auto &keyParam : keyParams) {
        std::string valueStr = ResourceUtil::GetKeyParamValue(keyParam);
        cJSON *value = cJSON_CreateString(valueStr.c_str());
        if (!value) {
            PrintError(GetError(ERR_CODE_UNDEFINED_ERROR).FormatCause("unable to create cJSON object for keyparam"));
            return RESTOOL_ERROR;
        }
        cJSON_AddItemToObject(json, ResourceUtil::KeyTypeToStr(keyParam.keyType).c_str(), value);
    }
    return RESTOOL_SUCCESS;
}

uint32_t CommonDumper::AddItemCommonPropToJson(int64_t resId, const ResourceItem &item, cJSON* json) const
{
    if (!json) {
        PrintError(GetError(ERR_CODE_UNDEFINED_ERROR).FormatCause("add item common property to null json object"));
        return RESTOOL_ERROR;
    }
    cJSON *id = cJSON_CreateNumber(resId);
    if (!id) {
        PrintError(GetError(ERR_CODE_UNDEFINED_ERROR).FormatCause("unable to create cJSON object for resource id"));
        return RESTOOL_ERROR;
    }
    cJSON_AddItemToObject(json, "id", id);

    cJSON *name = cJSON_CreateString(item.GetName().c_str());
    if (!name) {
        PrintError(GetError(ERR_CODE_UNDEFINED_ERROR).FormatCause("unable to create cJSON object for resource name"));
        return RESTOOL_ERROR;
    }
    cJSON_AddItemToObject(json, "name", name);

    cJSON *type = cJSON_CreateString((ResourceUtil::ResTypeToString(item.GetResType())).c_str());
    if (!type) {
        PrintError(GetError(ERR_CODE_UNDEFINED_ERROR).FormatCause("unable to create cJSON object for resource type"));
        return RESTOOL_ERROR;
    }
    cJSON_AddItemToObject(json, "type", type);
    return RESTOOL_SUCCESS;
}

uint32_t CommonDumper::AddResourceToJson(int64_t resId, const std::vector<ResourceItem> &items, cJSON *json) const
{
    if (items.empty()) {
        PrintError(GetError(ERR_CODE_UNDEFINED_ERROR).FormatCause("resource items is empty"));
        return  RESTOOL_ERROR;
    }
    if (!json) {
        PrintError(GetError(ERR_CODE_UNDEFINED_ERROR).FormatCause("add resource to null json object"));
        return RESTOOL_ERROR;
    }
    cJSON *resource = cJSON_CreateObject();
    if (!resource) {
        PrintError(GetError(ERR_CODE_UNDEFINED_ERROR).FormatCause("unable to create cJSON object for resource item"));
        return RESTOOL_ERROR;
    }
    cJSON_AddItemToArray(json, resource);
    if (AddItemCommonPropToJson(resId, items[0], resource) != RESTOOL_SUCCESS) {
        return RESTOOL_ERROR;
    };
    cJSON *entryCount = cJSON_CreateNumber(items.size());
    if (!entryCount) {
        PrintError(
            GetError(ERR_CODE_UNDEFINED_ERROR).FormatCause("unable to create cJSON object for resource value count"));
        return RESTOOL_ERROR;
    }
    cJSON_AddItemToObject(resource, "entryCount", entryCount);

    cJSON *entryValues = cJSON_CreateArray();
    if (!resource) {
        PrintError(
            GetError(ERR_CODE_UNDEFINED_ERROR).FormatCause("unable to create cJSON object for resource value array"));
        return RESTOOL_ERROR;
    }
    cJSON_AddItemToObject(resource, "entryValues", entryValues);

    for (const ResourceItem &item : items) {
        cJSON *value = cJSON_CreateObject();
        if (!value) {
            PrintError(GetError(ERR_CODE_UNDEFINED_ERROR)
                .FormatCause("unable to create cJSON object for value item"));
            return RESTOOL_ERROR;
        }
        cJSON_AddItemToArray(entryValues, value);
        if (AddValueToJson(item, value) != RESTOOL_SUCCESS) {
            return RESTOOL_ERROR;
        }
        if (AddKeyParamsToJson(item.GetKeyParam(), value) != RESTOOL_SUCCESS) {
            return RESTOOL_ERROR;
        }
    }
    return RESTOOL_SUCCESS;
}

uint32_t CommonDumper::AddPairVauleToJson(const ResourceItem &item, cJSON *json) const
{
    if (!json) {
        PrintError(GetError(ERR_CODE_UNDEFINED_ERROR).FormatCause("add pair vaule to null json object"));
        return RESTOOL_ERROR;
    }
    cJSON *value = cJSON_CreateObject();
    if (!value) {
        PrintError(GetError(ERR_CODE_UNDEFINED_ERROR).FormatCause("unable to create cJSON object for value object"));
        return RESTOOL_ERROR;
    }
    cJSON_AddItemToObject(json, "value", value);
    const std::vector<std::string> rawValues = item.SplitValue();
    uint32_t index = 1;
    if (rawValues.size() % PAIR_SIZE != 0) {
        cJSON *parent = cJSON_CreateString(rawValues[0].c_str());
        if (!parent) {
            PrintError(GetError(ERR_CODE_UNDEFINED_ERROR).FormatCause("unable to create cJSON object for parent"));
            return RESTOOL_ERROR;
        }
        cJSON_AddItemToObject(json, "parent", parent);
        index++;
    }
    for (; index < rawValues.size(); index += PAIR_SIZE) {
        cJSON *item = cJSON_CreateString(rawValues[index].c_str());
        if (!item) {
            PrintError(GetError(ERR_CODE_UNDEFINED_ERROR)
                .FormatCause("unable to create cJSON object for value item"));
            return RESTOOL_ERROR;
        }
        cJSON_AddItemToObject(value, rawValues[index -1].c_str(), item);
    }
    return RESTOOL_SUCCESS;
}

uint32_t CommonDumper::AddValueToJson(const ResourceItem &item, cJSON *json) const
{
    if (!json) {
        PrintError(GetError(ERR_CODE_UNDEFINED_ERROR).FormatCause("add value to null json object"));
        return RESTOOL_ERROR;
    }
    if (item.IsArray()) {
        cJSON *values = cJSON_CreateArray();
        if (!values) {
            PrintError(GetError(ERR_CODE_UNDEFINED_ERROR)
                .FormatCause("unable to create cJSON object for value array"));
            return RESTOOL_ERROR;
        }
        cJSON_AddItemToObject(json, "value", values);
        const std::vector<std::string> rawValues = item.SplitValue();
        for (const std::string &value : rawValues) {
            cJSON *valueItem = cJSON_CreateString(value.c_str());
            if (!valueItem) {
                PrintError(
                    GetError(ERR_CODE_UNDEFINED_ERROR).FormatCause("unable to create cJSON object for value item"));
                return RESTOOL_ERROR;
            }
            cJSON_AddItemToArray(values, valueItem);
        }
        return RESTOOL_SUCCESS;
    }
    if (item.IsPair()) {
        return AddPairVauleToJson(item, json);
    }
    std::string rawValue = std::string(reinterpret_cast<const char*>(item.GetData()), item.GetDataLength());
    cJSON *value = cJSON_CreateString(rawValue.c_str());
    if (!value) {
        PrintError(GetError(ERR_CODE_UNDEFINED_ERROR).FormatCause("unable to create cJSON object for value"));
        return RESTOOL_ERROR;
    }
    cJSON_AddItemToObject(json, "value", value);
    return RESTOOL_SUCCESS;
}

uint32_t ConfigDumper::DumpRes(std::string &out) const
{
    std::unique_ptr<cJSON, std::function<void(cJSON*)>> root(cJSON_CreateObject(),
        [](cJSON* node) { cJSON_Delete(node); });
    if (!root) {
        PrintError(GetError(ERR_CODE_UNDEFINED_ERROR).FormatCause("unable to create cJSON object for root object"));
        return RESTOOL_ERROR;
    }
    cJSON *configs = cJSON_CreateArray();
    if (!configs) {
        PrintError(GetError(ERR_CODE_UNDEFINED_ERROR).FormatCause("unable to create cJSON object for config array"));
        return RESTOOL_ERROR;
    }
    cJSON_AddItemToObject(root.get(), "config", configs);

    std::set<std::string> configSet;
    for (auto it = resInfos_.cbegin(); it != resInfos_.cend(); it++) {
        for (const auto &item : it->second) {
            const std::string &limitKey = item.GetLimitKey();
            if (configSet.count(limitKey) != 0) {
                continue;
            }
            configSet.emplace(limitKey);
            cJSON *config = cJSON_CreateString(limitKey.c_str());
            if (!config) {
                PrintError(
                    GetError(ERR_CODE_UNDEFINED_ERROR).FormatCause("unable to create cJSON object for limitkey"));
                return RESTOOL_ERROR;
            }
            cJSON_AddItemToArray(configs, config);
        }
    }
    char *rawStr = cJSON_Print(root.get());
    if (!rawStr) {
        PrintError(GetError(ERR_CODE_UNDEFINED_ERROR).FormatCause("covert config json object to str failed"));
        return RESTOOL_ERROR;
    }
    out = rawStr;
    cJSON_free(rawStr);
    rawStr = nullptr;
    return RESTOOL_SUCCESS;
}
}
}
}