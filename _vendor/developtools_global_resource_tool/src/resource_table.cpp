/*
 * Copyright (c) 2021-2025 Huawei Device Co., Ltd.
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

#include "resource_table.h"

#include <algorithm>
#include <cJSON.h>
#include <cstdint>
#include "cmd/cmd_parser.h"
#include "file_entry.h"
#include "file_manager.h"
#include "resource_util.h"
#include "securec.h"

namespace OHOS {
namespace Global {
namespace Restool {
using namespace std;
ResourceTable::ResourceTable(bool isNewModule)
{
    auto &parser = CmdParser::GetInstance();
    auto &packageParser = parser.GetPackageParser();
    if (!packageParser.GetIdDefinedOutput().empty()) {
        idDefinedPath_ = FileEntry::FilePath(packageParser.GetIdDefinedOutput()).Append(ID_DEFINED_FILE).GetPath();
    }
    indexFilePath_ = FileEntry::FilePath(packageParser.GetOutput()).Append(RESOURCE_INDEX_FILE).GetPath();
    newResIndex_ = isNewModule;
}

ResourceTable::~ResourceTable()
{
}

uint32_t ResourceTable::CreateResourceTable()
{
    FileManager &fileManager = FileManager::GetInstance();
    auto &allResource = fileManager.GetResources();
    map<string, vector<TableData>> configs;
    for (const auto &item : allResource) {
        for (const auto &resourceItem : item.second) {
            if (resourceItem.GetResType() == ResType::ID) {
                break;
            }
            TableData tableData;
            tableData.id = item.first;
            tableData.resourceItem = resourceItem;
            configs[resourceItem.GetLimitKey()].push_back(tableData);
        }
    }

    if (!newResIndex_) {
        if (SaveToResouorceIndex(configs) != RESTOOL_SUCCESS) {
            return RESTOOL_ERROR;
        }
    } else {
        if (SaveToNewResouorceIndex(configs) != RESTOOL_SUCCESS) {
            return RESTOOL_ERROR;
        }
    }

    if (!idDefinedPath_.empty()) {
        if (CreateIdDefined(allResource) != RESTOOL_SUCCESS) {
            return RESTOOL_ERROR;
        }
    }
    return RESTOOL_SUCCESS;
}

uint32_t ResourceTable::CreateResourceTable(const map<int64_t, vector<shared_ptr<ResourceItem>>> &items)
{
    map<string, vector<TableData>> configs;
    map<int64_t, vector<ResourceItem>> allResource;
    for (const auto &item : items) {
        vector<ResourceItem> resourceItems;
        for (const auto &resourceItemPtr : item.second) {
            if (resourceItemPtr->GetResType() == ResType::ID) {
                break;
            }
            TableData tableData;
            tableData.id = item.first;
            tableData.resourceItem = *resourceItemPtr;
            resourceItems.push_back(*resourceItemPtr);
            configs[resourceItemPtr->GetLimitKey()].push_back(tableData);
        }
        allResource.emplace(item.first, resourceItems);
    }

    if (!newResIndex_) {
        if (SaveToResouorceIndex(configs) != RESTOOL_SUCCESS) {
            return RESTOOL_ERROR;
        }
    } else {
        if (SaveToNewResouorceIndex(configs) != RESTOOL_SUCCESS) {
            return RESTOOL_ERROR;
        }
    }

    if (!idDefinedPath_.empty()) {
        if (CreateIdDefined(allResource) != RESTOOL_SUCCESS) {
            return RESTOOL_ERROR;
        }
    }
    return RESTOOL_SUCCESS;
}

uint32_t ResourceTable::LoadResTable(const string path, map<int64_t, vector<ResourceItem>> &resInfos)
{
    ifstream in(path, ios::binary);
    if (!in.is_open()) {
        PrintError(GetError(ERR_CODE_OPEN_FILE_ERROR).FormatCause(path.c_str(), strerror(errno)));
        return RESTOOL_ERROR;
    }

    in.seekg(0, ios::end);
    int64_t length = in.tellg();
    if (length <= 0) {
        in.close();
        PrintError(GetError(ERR_CODE_READ_FILE_ERROR).FormatCause(path.c_str(), "file is empty"));
        return RESTOOL_ERROR;
    }
    in.seekg(0, ios::beg);
    uint32_t errorCode = LoadResTable(in, resInfos);
    in.close();
    return errorCode;
}

uint32_t ResourceTable::LoadResTable(basic_istream<char> &in, map<int64_t, vector<ResourceItem>> &resInfos)
{
    if (!in) {
        std::string msg = "file stream bad, state code: " + std::to_string(in.rdstate());
        PrintError(GetError(ERR_CODE_READ_FILE_ERROR).FormatCause(in.rdstate(), msg.c_str()));
        return RESTOOL_ERROR;
    }
    in.seekg(0, ios::end);
    int64_t length = in.tellg();
    in.seekg(0, ios::beg);
    uint64_t pos = 0;
    IndexHeader indexHeader;
    if (!ReadFileHeader(in, indexHeader, pos, static_cast<uint64_t>(length))) {
        return RESTOOL_ERROR;
    }

    if (IsNewModule(indexHeader)) {
        return LoadNewResTable(in, resInfos);
    }

    map<int64_t, vector<KeyParam>> limitKeys;
    if (!ReadLimitKeys(in, limitKeys, indexHeader.limitKeyConfigSize, pos, static_cast<uint64_t>(length))) {
        return RESTOOL_ERROR;
    }

    map<int64_t, pair<int64_t, int64_t>> datas;
    if (!ReadIdTables(in, datas, indexHeader.limitKeyConfigSize, pos, static_cast<uint64_t>(length))) {
        return RESTOOL_ERROR;
    }

    while (in.tellg() < length) {
        RecordItem record;
        if (!ReadDataRecordPrepare(in, record, pos, static_cast<uint64_t>(length)) ||
            !ReadDataRecordStart(in, record, limitKeys, datas, resInfos)) {
            return RESTOOL_ERROR;
        }
    }
    return RESTOOL_SUCCESS;
}

uint32_t ResourceTable::CreateIdDefined(const map<int64_t, vector<ResourceItem>> &allResource) const
{
    cJSON *root = cJSON_CreateObject();
    cJSON *recordArray = cJSON_CreateArray();
    if (recordArray == nullptr) {
        PrintError(GetError(ERR_CODE_UNDEFINED_ERROR).FormatCause("failed to create cJSON object for record array")
            .SetPosition(idDefinedPath_));
        cJSON_Delete(root);
        return RESTOOL_ERROR;
    }
    cJSON_AddItemToObject(root, "record", recordArray);
    for (const auto &pairPtr : allResource) {
        if (pairPtr.second.empty()) {
            PrintError(GetError(ERR_CODE_UNDEFINED_ERROR).FormatCause("resource item vector is empty")
                .SetPosition(idDefinedPath_));
            cJSON_Delete(root);
            return RESTOOL_ERROR;
        }
        cJSON *jsonItem = cJSON_CreateObject();
        if (jsonItem == nullptr) {
            PrintError(GetError(ERR_CODE_UNDEFINED_ERROR).FormatCause("failed to create cJSON object for resource item")
                .SetPosition(idDefinedPath_));
            cJSON_Delete(root);
            return RESTOOL_ERROR;
        }
        ResourceItem item = pairPtr.second.front();
        ResType resType = item.GetResType();
        string type = ResourceUtil::ResTypeToString(resType);
        string name = item.GetName();
        int64_t id = pairPtr.first;
        if (type.empty()) {
            string errMsg = "name = ";
            errMsg.append(name);
            errMsg.append("invalid restype, type must in ").append(ResourceUtil::GetAllRestypeString());
            PrintError(GetError(ERR_CODE_UNDEFINED_ERROR).FormatCause(errMsg.c_str()).SetPosition(idDefinedPath_));
            cJSON_Delete(jsonItem);
            cJSON_Delete(root);
            return RESTOOL_ERROR;
        }
        cJSON_AddStringToObject(jsonItem, "type", type.c_str());
        cJSON_AddStringToObject(jsonItem, "name", ResourceUtil::GetIdName(name, resType).c_str());
        cJSON_AddStringToObject(jsonItem, "id", ResourceUtil::DecToHexStr(id).c_str());
        cJSON_AddItemToArray(recordArray, jsonItem);
    }
    if (!ResourceUtil::SaveToJsonFile(idDefinedPath_, root)) {
        cJSON_Delete(root);
        return RESTOOL_ERROR;
    }
    cJSON_Delete(root);
    return RESTOOL_SUCCESS;
}

// below private
uint32_t ResourceTable::SaveToResouorceIndex(const map<string, vector<TableData>> &configs) const
{
    uint32_t pos = 0;
    IndexHeader indexHeader;
    if (!InitIndexHeader(indexHeader, configs.size())) {
        return RESTOOL_ERROR;
    }
    pos += sizeof(IndexHeader);

    map<string, LimitKeyConfig> limitKeyConfigs;
    map<string, IdSet> idSets;
    if (!Prepare(configs, limitKeyConfigs, idSets, pos)) {
        return RESTOOL_ERROR;
    }

    ofstream out(indexFilePath_, ofstream::out | ofstream::binary);
    if (!out.is_open()) {
        PrintError(GetError(ERR_CODE_OPEN_FILE_ERROR).FormatCause(indexFilePath_.c_str(), strerror(errno)));
        return RESTOOL_ERROR;
    }

    ostringstream outStreamData;
    if (!SaveRecordItem(configs, outStreamData, idSets, pos)) {
        return RESTOOL_ERROR;
    }

    ostringstream outStreamHeader;
    indexHeader.fileSize = pos;
    SaveHeader(indexHeader, outStreamHeader);
    SaveLimitKeyConfigs(limitKeyConfigs, outStreamHeader);
    SaveIdSets(idSets, outStreamHeader);
    out << outStreamHeader.str();
    out << outStreamData.str();
    return RESTOOL_SUCCESS;
}

bool ResourceTable::InitHeader(IndexHeaderV2 &indexHeader, IdSetHeader &idSetHeader,
    DataHeader &dataHeader, uint32_t count)
{
    std::string restoolVersion = RESTOOLV2_NAME + RESTOOL_VERSION;
    if (memcpy_s(indexHeader.version, VERSION_MAX_LEN, restoolVersion.data(), restoolVersion.length()) != EOK) {
        PrintError(GetError(ERR_CODE_UNDEFINED_ERROR).FormatCause("memcpy error when init index header"));
        return false;
    }

    indexHeader.keyCount = count;
    indexHeader.length = IndexHeaderV2::INDEX_HEADER_LEN;
    indexHeader.keyConfigs.reserve(indexHeader.keyCount);
    idSetHeader.length = IdSetHeader::ID_SET_HEADER_LEN;
    dataHeader.length = DataHeader::DATA_HEADER_LEN;
    return true;
}

void ResourceTable::PrepareKeyConfig(IndexHeaderV2 &indexHeader, const uint32_t configId,
    const string &config, const vector<TableData> &data)
{
    const vector<KeyParam> &keyParams = data[0].resourceItem.GetKeyParam();
    KeyConfig keyConfig;
    keyConfig.configId = configId;
    keyConfig.keyCount = keyParams.size();
    keyConfig.configs.reserve(keyConfig.keyCount);
    indexHeader.length += KeyConfig::KEY_CONFIG_HEADER_LEN;
    for (const auto &param : keyParams) {
        keyConfig.configs.push_back(param);
        indexHeader.length += KeyParam::KEY_PARAM_LEN;
    }
    indexHeader.keyConfigs[config] = keyConfig;
}

void ResourceTable::PrepareResIndex(IdSetHeader &idSetHeader, const TableData &tableData)
{
    ResType resType = tableData.resourceItem.GetResType();
    if (idSetHeader.resTypes.find(resType) == idSetHeader.resTypes.end()) {
        ResTypeHeader resTypeHeader;
        resTypeHeader.resType = resType;
        resTypeHeader.length = ResTypeHeader::RES_TYPE_HEADER_LEN;
        idSetHeader.resTypes[resTypeHeader.resType] = resTypeHeader;
        idSetHeader.typeCount++;
        idSetHeader.length += ResTypeHeader::RES_TYPE_HEADER_LEN;
    }
    if (idSetHeader.resTypes[resType].resIndexs.find(tableData.id) != idSetHeader.resTypes[resType].resIndexs.end()) {
        return;
    }

    ResIndex resIndex;
    resIndex.resId = tableData.id;
    resIndex.name = ResourceUtil::GetIdName(tableData.resourceItem.GetName(), resType);
    resIndex.length = resIndex.name.length();
    idSetHeader.resTypes[resType].resIndexs[tableData.id] = resIndex;
    idSetHeader.resTypes[resType].length += ResIndex::RES_INDEX_LEN + resIndex.length;
    idSetHeader.resTypes[resType].count++;
    idSetHeader.length += ResIndex::RES_INDEX_LEN + resIndex.length;
}

void ResourceTable::PrepareResInfo(DataHeader &dataHeader, const uint32_t resId,
    const uint32_t configId, const uint32_t dataPoolLen)
{
    if (dataHeader.resInfos.find(resId) == dataHeader.resInfos.end()) {
        ResInfo resInfo;
        resInfo.resId = resId;
        resInfo.length = ResInfo::RES_INFO_LEN;
        dataHeader.resInfos[resInfo.resId] = resInfo;
        dataHeader.length += ResInfo::RES_INFO_LEN;
        dataHeader.idCount++;
    }
    dataHeader.resInfos[resId].dataOffset[configId] = dataPoolLen;
    dataHeader.resInfos[resId].length += ResInfo::DATA_OFFSET_LEN;
    dataHeader.resInfos[resId].valueCount++;
    dataHeader.length += ResInfo::DATA_OFFSET_LEN;
}

void ResourceTable::WriteDataPool(ostringstream &dataPool, const ResourceItem &resourceItem, uint32_t &dataPoolLen)
{
    uint32_t length = resourceItem.GetDataLength();
    const int8_t *data = resourceItem.GetData();
    dataPool.write(reinterpret_cast<const char *>(&length), sizeof(uint16_t));
    dataPool.write(reinterpret_cast<const char *>(data), length);
    dataPoolLen += sizeof(uint16_t) + length;
}

void ResourceTable::WriteResInfo(ostringstream &dataBlock, const DataHeader &idSetHeader,
    const uint32_t dataBlockOffset, unordered_map<uint32_t, uint32_t> &idOffsetMap)
{
    uint32_t offset = dataBlockOffset;
    dataBlock.write(reinterpret_cast<const char *>(idSetHeader.idTag), TAG_LEN);
    dataBlock.write(reinterpret_cast<const char *>(&idSetHeader.length), sizeof(uint32_t));
    dataBlock.write(reinterpret_cast<const char *>(&idSetHeader.idCount), sizeof(uint32_t));
    offset += DataHeader::DATA_HEADER_LEN;
    for (const auto &resInfo : idSetHeader.resInfos) {
        idOffsetMap[resInfo.second.resId] = offset;
        dataBlock.write(reinterpret_cast<const char *>(&resInfo.second.resId), sizeof(uint32_t));
        dataBlock.write(reinterpret_cast<const char *>(&resInfo.second.length), sizeof(uint32_t));
        dataBlock.write(reinterpret_cast<const char *>(&resInfo.second.valueCount), sizeof(uint32_t));
        offset += ResInfo::RES_INFO_LEN;
        for (const auto &dataOffset : resInfo.second.dataOffset) {
            uint32_t configId = dataOffset.first;
            uint32_t realOffset = dataOffset.second + idSetHeader.length + dataBlockOffset;
            dataBlock.write(reinterpret_cast<const char *>(&configId), sizeof(uint32_t));
            dataBlock.write(reinterpret_cast<const char *>(&realOffset), sizeof(uint32_t));
            offset += ResInfo::DATA_OFFSET_LEN;
        }
    }
}

void ResourceTable::WriteIdSet(ostringstream &idSetBlock, const IdSetHeader &idSetHeader,
    const unordered_map<uint32_t, uint32_t> &idOffsetMap)
{
    idSetBlock.write(reinterpret_cast<const char *>(idSetHeader.idTag), TAG_LEN);
    idSetBlock.write(reinterpret_cast<const char *>(&idSetHeader.length), sizeof(uint32_t));
    idSetBlock.write(reinterpret_cast<const char *>(&idSetHeader.typeCount), sizeof(uint32_t));
    idSetBlock.write(reinterpret_cast<const char *>(&idSetHeader.idCount), sizeof(uint32_t));
    for (const auto &resType : idSetHeader.resTypes) {
        idSetBlock.write(reinterpret_cast<const char *>(&resType.second.resType), sizeof(uint32_t));
        idSetBlock.write(reinterpret_cast<const char *>(&resType.second.length), sizeof(uint32_t));
        idSetBlock.write(reinterpret_cast<const char *>(&resType.second.count), sizeof(uint32_t));
        for (const auto &resIndex : resType.second.resIndexs) {
            uint32_t realOffset = idOffsetMap.find(resIndex.second.resId)->second;
            idSetBlock.write(reinterpret_cast<const char *>(&resIndex.second.resId), sizeof(uint32_t));
            idSetBlock.write(reinterpret_cast<const char *>(&realOffset), sizeof(uint32_t));
            idSetBlock.write(reinterpret_cast<const char *>(&resIndex.second.length), sizeof(uint32_t));
            idSetBlock.write(reinterpret_cast<const char *>(
                resIndex.second.name.c_str()), resIndex.second.length);
        }
    }
}

void ResourceTable::WriteResHeader(ostringstream &resHeaderBlock, const IndexHeaderV2 &indexHeader)
{
    resHeaderBlock.write(reinterpret_cast<const char *>(indexHeader.version), VERSION_MAX_LEN);
    resHeaderBlock.write(reinterpret_cast<const char *>(&indexHeader.length), sizeof(uint32_t));
    resHeaderBlock.write(reinterpret_cast<const char *>(&indexHeader.keyCount), sizeof(uint32_t));
    resHeaderBlock.write(reinterpret_cast<const char *>(&indexHeader.dataBlockOffset), sizeof(uint32_t));
    for (const auto &keyConfig : indexHeader.keyConfigs) {
        resHeaderBlock.write(reinterpret_cast<const char *>(keyConfig.second.keyTag), TAG_LEN);
        resHeaderBlock.write(reinterpret_cast<const char *>(&keyConfig.second.configId), sizeof(uint32_t));
        resHeaderBlock.write(reinterpret_cast<const char *>(&keyConfig.second.keyCount), sizeof(uint32_t));
        for (const auto &config : keyConfig.second.configs) {
            resHeaderBlock.write(reinterpret_cast<const char *>(&config.keyType), sizeof(int32_t));
            resHeaderBlock.write(reinterpret_cast<const char *>(&config.value), sizeof(int32_t));
        }
    }
}

void ResourceTable::WriteToIndex(const IndexHeaderV2 &indexHeader, const IdSetHeader &idSetHeader,
    const DataHeader &dataHeader, const ostringstream &dataPool, ofstream &out)
{
    ostringstream dataBlock;
    unordered_map<uint32_t, uint32_t> idOffsetMap;
    WriteResInfo(dataBlock, dataHeader, indexHeader.dataBlockOffset, idOffsetMap);

    ostringstream idSetBlock;
    WriteIdSet(idSetBlock, idSetHeader, idOffsetMap);

    ostringstream resHeaderBlock;
    WriteResHeader(resHeaderBlock, indexHeader);

    out << resHeaderBlock.str();
    out << idSetBlock.str();
    out << dataBlock.str();
    out << dataPool.str();
}

uint32_t ResourceTable::SaveToNewResouorceIndex(const map<string, vector<TableData>> &configs) const
{
    IndexHeaderV2 indexHeader;
    IdSetHeader idSetHeader;
    DataHeader dataHeader;

    if (!InitHeader(indexHeader, idSetHeader, dataHeader, configs.size())) {
        return false;
    }

    ostringstream dataPool;
    uint32_t dataPoolLen = 0;
    uint32_t configId = 0;
    for (const auto &config : configs) {
        PrepareKeyConfig(indexHeader, configId, config.first, config.second);
        for (const auto &tableData : config.second) {
            PrepareResIndex(idSetHeader, tableData);
            PrepareResInfo(dataHeader, tableData.id, configId, dataPoolLen);
            WriteDataPool(dataPool, tableData.resourceItem, dataPoolLen);
        }
        configId++;
    }
    idSetHeader.idCount = dataHeader.idCount;
    indexHeader.dataBlockOffset = indexHeader.length + idSetHeader.length;
    indexHeader.length += idSetHeader.length + dataHeader.length + dataPoolLen;

    ofstream out(indexFilePath_, ofstream::out | ofstream::binary);
    if (!out.is_open()) {
        PrintError(GetError(ERR_CODE_OPEN_FILE_ERROR).FormatCause(indexFilePath_.c_str(), strerror(errno)));
        return RESTOOL_ERROR;
    }
    WriteToIndex(indexHeader, idSetHeader, dataHeader, dataPool, out);
    return RESTOOL_SUCCESS;
}

bool ResourceTable::InitIndexHeader(IndexHeader &indexHeader, uint32_t count) const
{
    std::string restoolVersion = RESTOOL_NAME + RESTOOL_VERSION;
    if (memcpy_s(indexHeader.version, VERSION_MAX_LEN, restoolVersion.data(), restoolVersion.length()) != EOK) {
        PrintError(GetError(ERR_CODE_UNDEFINED_ERROR).FormatCause("memcpy error when init index header"));
        return false;
    }
    indexHeader.limitKeyConfigSize = count;
    return true;
}

bool ResourceTable::Prepare(const map<string, vector<TableData>> &configs,
                            map<string, LimitKeyConfig> &limitKeyConfigs,
                            map<string, IdSet> &idSets, uint32_t &pos) const
{
    for (const auto &config : configs) {
        LimitKeyConfig limitKeyConfig;
        const auto &keyParams = config.second.at(0).resourceItem.GetKeyParam();
        limitKeyConfig.keyCount = keyParams.size();
        pos += sizeof(limitKeyConfig.keyTag) + sizeof(limitKeyConfig.offset) + sizeof(limitKeyConfig.keyCount);
        for (const auto &keyParam : keyParams) {
            limitKeyConfig.data.push_back(static_cast<int32_t>(keyParam.keyType));
            limitKeyConfig.data.push_back(static_cast<int32_t>(keyParam.value));
            pos += sizeof(KeyParam);
        }
        limitKeyConfigs.emplace(config.first, limitKeyConfig);
    }

    for (const auto &config : configs) {
        auto limitKeyConfig = limitKeyConfigs.find(config.first);
        if (limitKeyConfig == limitKeyConfigs.end()) {
            PrintError(GetError(ERR_CODE_INVALID_LIMIT_KEY).FormatCause(config.first.c_str()));
            return false;
        }
        limitKeyConfig->second.offset = pos;

        IdSet idSet;
        idSet.idCount = config.second.size();
        pos += sizeof(idSet.idTag) + sizeof(idSet.idCount);
        for (const auto &tableData : config.second) {
            idSet.data.emplace(tableData.id, 0);
            pos += sizeof(uint32_t) + sizeof(uint32_t);
        }
        idSets.emplace(config.first, idSet);
    }
    return true;
}

bool ResourceTable::SaveRecordItem(const map<string, vector<TableData>> &configs,
                                   ostringstream &out, map<string, IdSet> &idSets, uint32_t &pos) const
{
    for (const auto &config : configs) {
        auto idSet = idSets.find(config.first);
        if (idSet == idSets.end()) {
            PrintError(GetError(ERR_CODE_INVALID_LIMIT_KEY).FormatCause(config.first.c_str()));
            return false;
        }

        for (const auto &tableData : config.second) {
            if (idSet->second.data.find(tableData.id) == idSet->second.data.end()) {
                string resType = ResourceUtil::ResTypeToString(tableData.resourceItem.GetResType());
                string name = tableData.resourceItem.GetName();
                PrintError(GetError(ERR_CODE_RESOURCE_ID_NOT_DEFINED).FormatCause(name.c_str(), resType.c_str()));
                return false;
            }
            idSet->second.data[tableData.id] = pos;
            RecordItem recordItem;
            recordItem.id = tableData.id;
            recordItem.resType = static_cast<int32_t>(tableData.resourceItem.GetResType());
            vector<string> contents;
            string value(reinterpret_cast<const char *>(tableData.resourceItem.GetData()),
                tableData.resourceItem.GetDataLength());
            contents.push_back(value);
            string name = ResourceUtil::GetIdName(tableData.resourceItem.GetName(),
                tableData.resourceItem.GetResType());
            contents.push_back(name);
            string data = ResourceUtil::ComposeStrings(contents, true);
            recordItem.size = sizeof(RecordItem) + data.length() - sizeof(uint32_t);
            pos += recordItem.size + sizeof(uint32_t);

            out.write(reinterpret_cast<const char *>(&recordItem.size), sizeof(uint32_t));
            out.write(reinterpret_cast<const char *>(&recordItem.resType), sizeof(uint32_t));
            out.write(reinterpret_cast<const char *>(&recordItem.id), sizeof(uint32_t));
            out.write(reinterpret_cast<const char *>(data.c_str()), data.length());
        }
    }
    return true;
}

void ResourceTable::SaveHeader(const IndexHeader &indexHeader, ostringstream &out) const
{
    out.write(reinterpret_cast<const char *>(&indexHeader.version), VERSION_MAX_LEN);
    out.write(reinterpret_cast<const char *>(&indexHeader.fileSize), sizeof(uint32_t));
    out.write(reinterpret_cast<const char *>(&indexHeader.limitKeyConfigSize), sizeof(uint32_t));
}

void ResourceTable::SaveLimitKeyConfigs(const map<string, LimitKeyConfig> &limitKeyConfigs, ostringstream &out) const
{
    for (const auto &iter : limitKeyConfigs) {
        out.write(reinterpret_cast<const char *>(iter.second.keyTag), TAG_LEN);
        out.write(reinterpret_cast<const char *>(&iter.second.offset), sizeof(uint32_t));
        out.write(reinterpret_cast<const char *>(&iter.second.keyCount), sizeof(uint32_t));
        for (const auto &value : iter.second.data) {
            out.write(reinterpret_cast<const char *>(&value), sizeof(int32_t));
        }
    }
}

void ResourceTable::SaveIdSets(const map<string, IdSet> &idSets, ostringstream &out) const
{
    for (const auto &iter : idSets) {
        out.write(reinterpret_cast<const char *>(iter.second.idTag), TAG_LEN);
        out.write(reinterpret_cast<const char *>(&iter.second.idCount), sizeof(uint32_t));
        for (const auto &keyValue : iter.second.data) {
            out.write(reinterpret_cast<const char *>(&keyValue.first), sizeof(uint32_t));
            out.write(reinterpret_cast<const char *>(&keyValue.second), sizeof(uint32_t));
        }
    }
}

bool ResourceTable::ReadFileHeader(basic_istream<char> &in, IndexHeader &indexHeader, uint64_t &pos, uint64_t length)
{
    pos += sizeof(indexHeader);
    if (pos > length) {
        PrintError(GetError(ERR_CODE_INVALID_RESOURCE_INDEX).FormatCause("header length error"));
        return false;
    }
    in.read(reinterpret_cast<char *>(indexHeader.version), VERSION_MAX_LEN);
    in.read(reinterpret_cast<char *>(&indexHeader.fileSize), sizeof(uint32_t));
    in.read(reinterpret_cast<char *>(&indexHeader.limitKeyConfigSize), sizeof(uint32_t));
    return true;
}

bool ResourceTable::ReadLimitKeys(basic_istream<char> &in, map<int64_t, vector<KeyParam>> &limitKeys,
                                  uint32_t count, uint64_t &pos, uint64_t length)
{
    for (uint32_t i = 0; i< count; i++) {
        pos = pos + TAG_LEN + sizeof(uint32_t) + sizeof(uint32_t);
        if (pos > length) {
            PrintError(GetError(ERR_CODE_INVALID_RESOURCE_INDEX).FormatCause("KEYS length error"));
            return false;
        }
        LimitKeyConfig limitKey;
        in.read(reinterpret_cast<char *>(limitKey.keyTag), TAG_LEN);
        string keyTag(reinterpret_cast<const char *>(limitKey.keyTag), TAG_LEN);
        if (keyTag != "KEYS") {
            PrintError(GetError(ERR_CODE_INVALID_RESOURCE_INDEX)
                .FormatCause(string("invalid key tag = " + keyTag).c_str()));
            return false;
        }
        in.read(reinterpret_cast<char *>(&limitKey.offset), sizeof(uint32_t));
        in.read(reinterpret_cast<char *>(&limitKey.keyCount), sizeof(uint32_t));

        vector<KeyParam> keyParams;
        for (uint32_t j = 0; j < limitKey.keyCount; j++) {
            pos = pos + sizeof(uint32_t) + sizeof(uint32_t);
            if (pos > length) {
                PrintError(GetError(ERR_CODE_INVALID_RESOURCE_INDEX).FormatCause("keyParams length error"));
                return false;
            }
            KeyParam keyParam;
            in.read(reinterpret_cast<char *>(&keyParam.keyType), sizeof(uint32_t));
            in.read(reinterpret_cast<char *>(&keyParam.value), sizeof(uint32_t));
            keyParams.push_back(keyParam);
        }
        limitKeys[limitKey.offset] = keyParams;
    }
    return true;
}

bool ResourceTable::ReadIdTables(basic_istream<char> &in, std::map<int64_t, std::pair<int64_t, int64_t>> &datas,
                                 uint32_t count, uint64_t &pos, uint64_t length)
{
    for (uint32_t i = 0; i< count; i++) {
        pos = pos + TAG_LEN + sizeof(uint32_t);
        if (pos > length) {
            PrintError(GetError(ERR_CODE_INVALID_RESOURCE_INDEX).FormatCause("IDSS length error"));
            return false;
        }
        IdSet idss;
        int64_t offset = in.tellg();
        in.read(reinterpret_cast<char *>(idss.idTag), TAG_LEN);
        string idTag(reinterpret_cast<const char *>(idss.idTag), TAG_LEN);
        if (idTag != "IDSS") {
            PrintError(GetError(ERR_CODE_INVALID_RESOURCE_INDEX)
                .FormatCause(string("invalid id tag = " + idTag).c_str()));
            return false;
        }
        in.read(reinterpret_cast<char *>(&idss.idCount), sizeof(uint32_t));

        for (uint32_t j = 0; j < idss.idCount; j++) {
            pos = pos + sizeof(uint32_t) + sizeof(uint32_t);
            if (pos > length) {
                PrintError(GetError(ERR_CODE_INVALID_RESOURCE_INDEX).FormatCause("id data length error"));
                return false;
            }
            IdData data;
            in.read(reinterpret_cast<char *>(&data.id), sizeof(uint32_t));
            in.read(reinterpret_cast<char *>(&data.dataOffset), sizeof(uint32_t));
            datas[data.dataOffset] = make_pair(data.id, offset);
        }
    }
    return true;
}

bool ResourceTable::ReadDataRecordPrepare(basic_istream<char> &in, RecordItem &record, uint64_t &pos, uint64_t length)
{
    pos = pos + sizeof(uint32_t);
    if (pos > length) {
        PrintError(GetError(ERR_CODE_INVALID_RESOURCE_INDEX).FormatCause("data record length error"));
        return false;
    }
    in.read(reinterpret_cast<char *>(&record.size), sizeof(uint32_t));
    pos = pos + record.size;
    if (pos > length) {
        PrintError(GetError(ERR_CODE_INVALID_RESOURCE_INDEX).FormatCause("record.size length error"));
        return false;
    }
    in.read(reinterpret_cast<char *>(&record.resType), sizeof(uint32_t));
    in.read(reinterpret_cast<char *>(&record.id), sizeof(uint32_t));
    return true;
}

bool ResourceTable::ReadDataRecordStart(basic_istream<char> &in, RecordItem &record,
                                        const map<int64_t, vector<KeyParam>> &limitKeys,
                                        const map<int64_t, pair<int64_t, int64_t>> &datas,
                                        map<int64_t, vector<ResourceItem>> &resInfos)
{
    int64_t offset = in.tellg();
    offset = offset - sizeof(uint32_t) - sizeof(uint32_t) - sizeof(uint32_t);
    uint16_t value_size = 0;
    in.read(reinterpret_cast<char *>(&value_size), sizeof(uint16_t));
    if (value_size + sizeof(uint16_t) > record.size) {
        PrintError(GetError(ERR_CODE_INVALID_RESOURCE_INDEX).FormatCause("value size error"));
        return false;
    }
    std::vector<int8_t> values(value_size);
    in.read(reinterpret_cast<char *>(values.data()), value_size);

    uint16_t name_size = 0;
    in.read(reinterpret_cast<char *>(&name_size), sizeof(uint16_t));
    if (value_size + sizeof(uint16_t) + name_size + sizeof(uint16_t) > record.size) {
        PrintError(GetError(ERR_CODE_INVALID_RESOURCE_INDEX).FormatCause("name size error"));
        return false;
    }
    std::vector<int8_t> name(name_size);
    in.read(reinterpret_cast<char *>(name.data()), name_size);
    string filename(reinterpret_cast<char *>(name.data()), name_size);

    auto idTableOffset = datas.find(offset);
    if (idTableOffset == datas.end()) {
        PrintError(GetError(ERR_CODE_INVALID_RESOURCE_INDEX).FormatCause("invalid id offset"));
        return false;
    }

    if (idTableOffset->second.first != record.id) {
        PrintError(GetError(ERR_CODE_INVALID_RESOURCE_INDEX).FormatCause("invalid id"));
        return false;
    }

    if (limitKeys.find(idTableOffset->second.second) == limitKeys.end()) {
        PrintError(GetError(ERR_CODE_INVALID_RESOURCE_INDEX).FormatCause("invalid limit key offset"));
        return false;
    }

    const vector<KeyParam> &keyparams = limitKeys.find(datas.find(offset)->second.second)->second;
    ResourceItem resourceitem(filename, keyparams, g_resTypeMap.find(record.resType)->second);
    resourceitem.SetLimitKey(ResourceUtil::PaserKeyParam(keyparams));
    resourceitem.SetData(values.data(), value_size);
    resourceitem.MarkCoverable();
    resInfos[record.id].push_back(resourceitem);
    return true;
}

bool ResourceTable::IsNewModule(const IndexHeader &indexHeader)
{
    string version = string(reinterpret_cast<const char *>(indexHeader.version), VERSION_MAX_LEN);
    if (version.substr(0, version.find(" ")) == "Restool") {
        return false;
    }
    return true;
}

uint32_t ResourceTable::LoadNewResTable(basic_istream<char> &in, map<int64_t, vector<ResourceItem>> &resInfos)
{
    if (!in) {
        std::string msg = "file stream bad, state code: " + std::to_string(in.rdstate());
        PrintError(GetError(ERR_CODE_READ_FILE_ERROR).FormatCause(in.rdstate(), msg.c_str()));
        return RESTOOL_ERROR;
    }
    in.seekg(0, ios::end);
    int64_t length = in.tellg();
    in.seekg(0, ios::beg);
    uint64_t pos = 0;
    IndexHeaderV2 indexHeader;
    if (!ReadNewFileHeader(in, indexHeader, pos, static_cast<uint64_t>(length))) {
        return RESTOOL_ERROR;
    }
    IdSetHeader idSetHeader;
    if (!ReadIdSetHeader(in, idSetHeader, pos, static_cast<uint64_t>(length))) {
        return RESTOOL_ERROR;
    }
    for (const auto &resType : idSetHeader.resTypes) {
        for (const auto &resId : resType.second.resIndexs) {
            ReadResources(in, resId.second, resType.second, indexHeader, static_cast<uint64_t>(length), resInfos);
        }
    }
    return RESTOOL_SUCCESS;
}

bool ResourceTable::ReadNewFileHeader(basic_istream<char> &in, IndexHeaderV2 &indexHeader,
    uint64_t &pos, uint64_t length)
{
    pos += IndexHeaderV2::INDEX_HEADER_LEN;
    if (pos > length) {
        PrintError(GetError(ERR_CODE_INVALID_RESOURCE_INDEX).FormatCause("header length error"));
        return false;
    }
    in.read(reinterpret_cast<char *>(indexHeader.version), VERSION_MAX_LEN);
    in.read(reinterpret_cast<char *>(&indexHeader.length), sizeof(uint32_t));
    in.read(reinterpret_cast<char *>(&indexHeader.keyCount), sizeof(uint32_t));
    in.read(reinterpret_cast<char *>(&indexHeader.dataBlockOffset), sizeof(uint32_t));

    for (uint32_t key = 0; key < indexHeader.keyCount; key++) {
        pos += KeyConfig::KEY_CONFIG_HEADER_LEN;
        if (pos > length) {
            PrintError(GetError(ERR_CODE_INVALID_RESOURCE_INDEX).FormatCause("KeyConfig header length error"));
            return false;
        }
        KeyConfig keyConfig;
        in.read(reinterpret_cast<char *>(keyConfig.keyTag), TAG_LEN);
        in.read(reinterpret_cast<char *>(&keyConfig.configId), sizeof(uint32_t));
        in.read(reinterpret_cast<char *>(&keyConfig.keyCount), sizeof(uint32_t));

        for (uint32_t keyType = 0; keyType < keyConfig.keyCount; keyType++) {
            pos += KeyParam::KEY_PARAM_LEN;
            if (pos > length) {
                PrintError(GetError(ERR_CODE_INVALID_RESOURCE_INDEX).FormatCause("KeyParam length error"));
                return false;
            }
            KeyParam keyParam;
            in.read(reinterpret_cast<char *>(&keyParam.keyType), sizeof(uint32_t));
            in.read(reinterpret_cast<char *>(&keyParam.value), sizeof(uint32_t));
            keyConfig.configs.push_back(keyParam);
        }
        indexHeader.idKeyConfigs[keyConfig.configId] = keyConfig;
    }
    return true;
}

bool ResourceTable::ReadIdSetHeader(basic_istream<char> &in, IdSetHeader &idSetHeader, uint64_t &pos, uint64_t length)
{
    pos += IdSetHeader::ID_SET_HEADER_LEN;
    if (pos > length) {
        PrintError(GetError(ERR_CODE_INVALID_RESOURCE_INDEX).FormatCause("IdSet header length error"));
        return false;
    }
    in.read(reinterpret_cast<char *>(idSetHeader.idTag), TAG_LEN);
    in.read(reinterpret_cast<char *>(&idSetHeader.length), sizeof(uint32_t));
    in.read(reinterpret_cast<char *>(&idSetHeader.typeCount), sizeof(uint32_t));
    in.read(reinterpret_cast<char *>(&idSetHeader.idCount), sizeof(uint32_t));

    for (uint32_t resType = 0; resType < idSetHeader.typeCount; resType++) {
        pos += ResTypeHeader::RES_TYPE_HEADER_LEN;
        if (pos > length) {
            PrintError(GetError(ERR_CODE_INVALID_RESOURCE_INDEX).FormatCause("ResType header length error"));
            return false;
        }
        ResTypeHeader resTypeHeader;
        in.read(reinterpret_cast<char *>(&resTypeHeader.resType), sizeof(uint32_t));
        in.read(reinterpret_cast<char *>(&resTypeHeader.length), sizeof(uint32_t));
        in.read(reinterpret_cast<char *>(&resTypeHeader.count), sizeof(uint32_t));

        for (uint32_t resId = 0; resId < resTypeHeader.count; resId++) {
            pos += ResIndex::RES_INDEX_LEN;
            if (pos > length) {
                PrintError(GetError(ERR_CODE_INVALID_RESOURCE_INDEX).FormatCause("ResIndex length error"));
                return false;
            }
            ResIndex resIndex;
            in.read(reinterpret_cast<char *>(&resIndex.resId), sizeof(uint32_t));
            in.read(reinterpret_cast<char *>(&resIndex.offset), sizeof(uint32_t));
            in.read(reinterpret_cast<char *>(&resIndex.length), sizeof(uint32_t));
            pos += resIndex.length;
            if (pos > length) {
                PrintError(GetError(ERR_CODE_INVALID_RESOURCE_INDEX).FormatCause("resource name length error"));
                return false;
            }
            char *name = new char[resIndex.length + 1]();
            in.read(name, resIndex.length);
            resIndex.name = string(name, resIndex.length);
            delete[] name;

            resTypeHeader.resIndexs[resIndex.resId] = resIndex;
        }
        idSetHeader.resTypes[resTypeHeader.resType] = resTypeHeader;
    }
    return true;
}

bool ResourceTable::ReadResources(std::basic_istream<char> &in, const ResIndex &resIndex,
    const ResTypeHeader &resTypeHeader, IndexHeaderV2 &indexHeader, uint64_t length,
    map<int64_t, vector<ResourceItem>> &resInfos)
{
    ResInfo resInfo;
    if (!ReadResInfo(in, resInfo, resIndex.offset, length)) {
        return RESTOOL_ERROR;
    }
    uint64_t pos = (uint64_t)resIndex.offset + (uint64_t)ResInfo::RES_INFO_LEN;
    for (uint32_t resConfig = 0; resConfig < resInfo.valueCount; resConfig++) {
        uint32_t resConfigId;
        uint32_t dataOffset;
        if (!ReadResConfig(in, resConfigId, dataOffset, pos, length)) {
            return RESTOOL_ERROR;
        }
        ResourceItem resourceItem(resIndex.name, indexHeader.idKeyConfigs[resConfigId].configs,
            resTypeHeader.resType);
        resourceItem.SetLimitKey(ResourceUtil::PaserKeyParam(indexHeader.idKeyConfigs[resConfigId].configs));
        if (!ReadResourceItem(in, resourceItem, dataOffset, pos, length)) {
            return RESTOOL_ERROR;
        }
        resInfos[resIndex.resId].push_back(resourceItem);
    }
    return true;
}

bool ResourceTable::ReadResInfo(std::basic_istream<char> &in, ResInfo &resInfo, uint32_t offset, uint64_t length)
{
    in.seekg(offset, ios::beg);
    if ((uint64_t)offset + (uint64_t)ResInfo::RES_INFO_LEN > length) {
        PrintError(GetError(ERR_CODE_INVALID_RESOURCE_INDEX).FormatCause("ResInfo length error"));
        return false;
    }
    in.read(reinterpret_cast<char *>(&resInfo.resId), sizeof(uint32_t));
    in.read(reinterpret_cast<char *>(&resInfo.length), sizeof(uint32_t));
    in.read(reinterpret_cast<char *>(&resInfo.valueCount), sizeof(uint32_t));
    return true;
}

bool ResourceTable::ReadResConfig(std::basic_istream<char> &in, uint32_t &resConfigId, uint32_t &dataOffset,
    uint64_t &pos, uint64_t length)
{
    in.seekg(pos, ios::beg);
    pos += sizeof(uint32_t) + sizeof(uint32_t);
    if (pos > length) {
        PrintError(GetError(ERR_CODE_INVALID_RESOURCE_INDEX).FormatCause("Config id length error"));
        return false;
    }
    in.read(reinterpret_cast<char *>(&resConfigId), sizeof(uint32_t));
    in.read(reinterpret_cast<char *>(&dataOffset), sizeof(uint32_t));
    return true;
}

bool ResourceTable::ReadResourceItem(std::basic_istream<char> &in, ResourceItem &resourceItem,
    uint32_t dataOffset, uint64_t &pos, uint64_t length)
{
    if (dataOffset + sizeof(uint16_t) > length) {
        PrintError(GetError(ERR_CODE_INVALID_RESOURCE_INDEX).FormatCause("resource length error"));
        return false;
    }
    in.seekg(dataOffset, ios::beg);
    uint16_t dataLen;
    in.read(reinterpret_cast<char *>(&dataLen), sizeof(uint16_t));
    if (dataOffset + sizeof(uint16_t) + dataLen > length) {
        PrintError(GetError(ERR_CODE_INVALID_RESOURCE_INDEX).FormatCause("resource length error"));
        return false;
    }
    std::vector<int8_t> data(dataLen + 1, 0);
    in.read(reinterpret_cast<char *>(data.data()), dataLen);

    resourceItem.SetData(data.data(), dataLen + 1);
    resourceItem.MarkCoverable();
    return true;
}
}
}
}
