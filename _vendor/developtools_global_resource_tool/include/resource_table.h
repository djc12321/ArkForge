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

#ifndef OHOS_RESTOOL_RESOURCE_TABLE_H
#define OHOS_RESTOOL_RESOURCE_TABLE_H

#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <unordered_map>

#include "resource_item.h"
#include "restool_errors.h"

namespace OHOS {
namespace Global {
namespace Restool {
class ResourceTable {
public:
    ResourceTable(bool isNewModule = false);
    virtual ~ResourceTable();
    uint32_t CreateResourceTable();
    uint32_t CreateResourceTable(const std::map<int64_t, std::vector<std::shared_ptr<ResourceItem>>> &items);
    static uint32_t LoadResTable(const std::string path, std::map<int64_t, std::vector<ResourceItem>> &resInfos);
    static uint32_t LoadResTable(std::basic_istream<char> &in, std::map<int64_t, std::vector<ResourceItem>> &resInfos);
private:
    struct TableData {
        uint32_t id;
        ResourceItem resourceItem;
    };

    struct IndexHeader {
        int8_t version[VERSION_MAX_LEN];
        uint32_t fileSize;
        uint32_t limitKeyConfigSize;
    };

    struct LimitKeyConfig {
        int8_t keyTag[TAG_LEN] = {'K', 'E', 'Y', 'S'};
        uint32_t offset; // IdSet file address offset
        uint32_t keyCount; // KeyParam count
        std::vector<int32_t> data;
    };

    struct IdSet {
        int8_t idTag[TAG_LEN] = {'I', 'D', 'S', 'S'};
        uint32_t idCount;
        std::map<uint32_t, uint32_t> data; // pair id and offset
    };

    struct RecordItem {
        uint32_t size;
        int32_t resType;
        uint32_t id;
    };

    struct KeyConfig {
        static const uint32_t KEY_CONFIG_HEADER_LEN = 12;
        int8_t keyTag[TAG_LEN] = {'K', 'E', 'Y', 'S'};
        uint32_t configId = 0;
        uint32_t keyCount = 0;
        std::vector<KeyParam> configs;
    };

    struct IndexHeaderV2 {
        static const uint32_t INDEX_HEADER_LEN = VERSION_MAX_LEN + 12;
        int8_t version[VERSION_MAX_LEN];
        uint32_t length = 0;
        uint32_t keyCount = 0;
        uint32_t dataBlockOffset = 0;
        std::unordered_map<std::string, KeyConfig> keyConfigs; // <resConfig, KeyConfig>
        std::unordered_map<uint32_t, KeyConfig> idKeyConfigs; // <configId, KeyConfig>
    };

    struct ResIndex {
        static const uint32_t RES_INDEX_LEN = 12;
        uint32_t resId = 0;
        uint32_t offset = 0;
        uint32_t length = 0;
        std::string name;
    };

    struct ResTypeHeader {
        static const uint32_t RES_TYPE_HEADER_LEN = 12;
        ResType resType;
        uint32_t length = 0;
        uint32_t count = 0;
        std::unordered_map<uint32_t, ResIndex> resIndexs; // <resId, resIndex>
    };

    struct IdSetHeader {
        static const uint32_t ID_SET_HEADER_LEN = 16;
        int8_t idTag[TAG_LEN] = {'I', 'D', 'S', 'S'};
        uint32_t length = 0;
        uint32_t typeCount = 0;
        uint32_t idCount = 0;
        std::unordered_map<ResType, ResTypeHeader> resTypes; // <resType, ResTypeHeader>
    };

    struct ResInfo {
        static const uint32_t RES_INFO_LEN = 12;
        static const uint32_t DATA_OFFSET_LEN = 8;
        uint32_t resId = 0;
        uint32_t length = 0;
        uint32_t valueCount = 0;
        std::map<uint32_t, uint32_t> dataOffset; // <resConfigId, offset>
    };

    struct DataHeader {
        static const uint32_t DATA_HEADER_LEN = 12;
        int8_t idTag[TAG_LEN] = {'D', 'A', 'T', 'A'};
        uint32_t length = 0;
        uint32_t idCount = 0;
        std::unordered_map<uint32_t, ResInfo> resInfos; // <resID, ResInfo>
    };

    uint32_t SaveToResouorceIndex(const std::map<std::string, std::vector<TableData>> &configs) const;
    uint32_t SaveToNewResouorceIndex(const std::map<std::string, std::vector<TableData>> &configs) const;
    uint32_t CreateIdDefined(const std::map<int64_t, std::vector<ResourceItem>> &allResource) const;
    bool InitIndexHeader(IndexHeader &indexHeader, uint32_t count) const;
    bool Prepare(const std::map<std::string, std::vector<TableData>> &configs,
                 std::map<std::string, LimitKeyConfig> &limitKeyConfigs,
                 std::map<std::string, IdSet> &idSets, uint32_t &pos) const;
    bool SaveRecordItem(const std::map<std::string, std::vector<TableData>> &configs, std::ostringstream &out,
                        std::map<std::string, IdSet> &idSets, uint32_t &pos) const;
    void SaveHeader(const IndexHeader &indexHeader, std::ostringstream &out) const;
    void SaveLimitKeyConfigs(const std::map<std::string, LimitKeyConfig> &limitKeyConfigs,
                             std::ostringstream &out) const;
    void SaveIdSets(const std::map<std::string, IdSet> &idSets, std::ostringstream &out) const;
    static bool ReadFileHeader(std::basic_istream<char> &in, IndexHeader &indexHeader, uint64_t &pos, uint64_t length);
    static bool ReadLimitKeys(std::basic_istream<char> &in, std::map<int64_t, std::vector<KeyParam>> &limitKeys,
        uint32_t count, uint64_t &pos, uint64_t length);
    static bool ReadIdTables(std::basic_istream<char> &in, std::map<int64_t, std::pair<int64_t, int64_t>> &datas,
        uint32_t count, uint64_t &pos, uint64_t length);
    static bool ReadDataRecordPrepare(std::basic_istream<char> &in, RecordItem &record,
        uint64_t &pos, uint64_t length);
    static bool ReadDataRecordStart(std::basic_istream<char> &in, RecordItem &record,
        const std::map<int64_t, std::vector<KeyParam>> &limitKeys,
        const std::map<int64_t, std::pair<int64_t, int64_t>> &datas,
        std::map<int64_t, std::vector<ResourceItem>> &resInfos);
    static bool InitHeader(IndexHeaderV2 &indexHeader, IdSetHeader &idSetHeader,
        DataHeader &dataHeader, uint32_t count);
    static void PrepareKeyConfig(IndexHeaderV2 &indexHeader, const uint32_t configId,
        const std::string &config, const std::vector<TableData> &data);
    static void PrepareResIndex(IdSetHeader &idSetHeader, const TableData &tableData);
    static void PrepareResInfo(DataHeader &dataHeader, const uint32_t resId,
        const uint32_t configId, const uint32_t dataPoolLen);
    static void WriteDataPool(std::ostringstream &dataPool, const ResourceItem &resourceItem, uint32_t &dataPoolLen);
    static void WriteResInfo(std::ostringstream &dataBlock, const DataHeader &idSetHeader,
        const uint32_t dataBlockOffset, std::unordered_map<uint32_t, uint32_t> &idOffsetMap);
    static void WriteIdSet(std::ostringstream &idSetBlock, const IdSetHeader &idSetHeader,
                        const std::unordered_map<uint32_t, uint32_t> &idOffsetMap);
    static void WriteResHeader(std::ostringstream &resHeaderBlock, const IndexHeaderV2 &indexHeader);
    static void WriteToIndex(const IndexHeaderV2 &indexHeader, const IdSetHeader &idSetHeader,
                const DataHeader &dataHeader, const std::ostringstream &dataPool, std::ofstream &out);
    static bool IsNewModule(const IndexHeader &indexHeader);
    static uint32_t LoadNewResTable(std::basic_istream<char> &in,
        std::map<int64_t, std::vector<ResourceItem>> &resInfos);
    static bool ReadNewFileHeader(std::basic_istream<char> &in, IndexHeaderV2 &indexHeader,
        uint64_t &pos, uint64_t length);
    static bool ReadIdSetHeader(std::basic_istream<char> &in, IdSetHeader &idSetHeader,
        uint64_t &pos, uint64_t length);
    static bool ReadResources(std::basic_istream<char> &in, const ResIndex &resIndex,
        const ResTypeHeader &resTypeHeader, IndexHeaderV2 &indexHeader, uint64_t length,
        std::map<int64_t, std::vector<ResourceItem>> &resInfos);
    static bool ReadResInfo(std::basic_istream<char> &in, ResInfo &resInfo, uint32_t offset, uint64_t length);
    static bool ReadResConfig(std::basic_istream<char> &in, uint32_t &resConfigId, uint32_t &dataOffset,
        uint64_t &pos, uint64_t length);
    static bool ReadResourceItem(std::basic_istream<char> &in, ResourceItem &resourceItem, uint32_t dataOffset,
        uint64_t &pos, uint64_t length);
    std::string indexFilePath_;
    std::string idDefinedPath_;
    bool newResIndex_ = false;
};
}
}
}
#endif