/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include "resource_append.h"
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <limits>
#include <regex>
#include "config_parser.h"
#include "header.h"
#include "id_worker.h"
#include "key_parser.h"
#include "reference_parser.h"
#include "resource_table.h"
#include "resource_util.h"
#include "select_compile_parse.h"
#ifdef __WIN32
#include "windows.h"
#endif
#include "securec.h"

namespace OHOS {
namespace Global {
namespace Restool {
using namespace std;

ResourceAppend::ResourceAppend(const PackageParser &packageParser) : packageParser_(packageParser)
{
}

uint32_t ResourceAppend::Append()
{
    string outputPath = packageParser_.GetOutput();
    for (const auto &iter : packageParser_.GetAppend()) {
        if (!ScanResources(iter, outputPath)) {
            return RESTOOL_ERROR;
        }
    }
    return RESTOOL_SUCCESS;
}

uint32_t ResourceAppend::Combine()
{
    vector<pair<ResType, string>> noBaseResource;
    for (const auto &iter : packageParser_.GetInputs()) {
        if (!Combine(iter)) {
            return RESTOOL_ERROR;
        }
        CheckAllItems(noBaseResource);
    }
    if (!noBaseResource.empty()) {
        ResourceUtil::PrintWarningMsg(noBaseResource);
    }

    if (!ParseRef()) {
        return RESTOOL_ERROR;
    }

    ResourceTable resourceTable;
    if (resourceTable.CreateResourceTable(items_) != RESTOOL_SUCCESS) {
        return RESTOOL_ERROR;
    }
    return RESTOOL_SUCCESS;
}

// private
bool ResourceAppend::Combine(const string &folderPath)
{
    FileEntry entry(folderPath);
    if (!entry.Init()) {
        return false;
    }

    itemsForModule_.clear();
    for (const auto &child : entry.GetChilds()) {
        if (!child->IsFile()) {
            PrintError(GetError(ERR_CODE_INVALID_RESOURCE_PATH)
                .FormatCause(child->GetFilePath().GetPath().c_str(), "not a file"));
            return false;
        }
        const std::string fileName = child->GetFilePath().GetFilename();
        if (fileName == ID_DEFINED_FILE) {
            continue;
        }
        if (fileName.find('.') == 0) {
            // The file starts with '.' is invalid
            continue;
        }
        if (!LoadResourceItem(child->GetFilePath().GetPath())) {
            return false;
        }
    }
    return true;
}

bool ResourceAppend::ParseRef()
{
    for (auto &iter : refs_) {
        ReferenceParser ref;
        if (iter->GetResType() == ResType::PROF || iter->GetResType() == ResType::MEDIA) {
            if (ref.ParseRefInJsonFile(*iter, packageParser_.GetOutput(), true) != RESTOOL_SUCCESS) {
                return false;
            }
        } else if (ref.ParseRefInResourceItem(*iter) != RESTOOL_SUCCESS) {
            return false;
        }
    }
    return true;
}

bool ResourceAppend::ScanResources(const string &resourcePath, const string &outputPath)
{
    if (!ResourceUtil::FileExist(resourcePath)) {
        string filePath = FileEntry::FilePath(outputPath).Append(ResourceUtil::GenerateHash(resourcePath)).GetPath();
        if (remove(filePath.c_str()) != 0) {
            PrintError(GetError(ERR_CODE_REMOVE_FILE_ERROR).FormatCause(filePath.c_str(), strerror(errno)));
            return false;
        }
        return true;
    }

    FileEntry entry(resourcePath);
    if (!entry.Init()) {
        return false;
    }

    if (entry.IsFile()) {
        return ScanSingleFile(resourcePath, outputPath);
    }

    return ScanSubResources(entry, resourcePath, outputPath);
}

bool ResourceAppend::ScanSubResources(const FileEntry entry, const string &resourcePath, const string &outputPath)
{
    vector<KeyParam> keyParams;
    if (KeyParser::Parse(entry.GetFilePath().GetFilename(), keyParams)) {
        for (const auto &child : entry.GetChilds()) {
            if (!ResourceUtil::IslegalPath(child->GetFilePath().GetFilename())) {
                continue;
            }
            if (!ScanIegalResources(child->GetFilePath().GetPath(), outputPath)) {
                return false;
            }
        }
        return true;
    }

    if (ResourceUtil::IslegalPath(entry.GetFilePath().GetFilename())) {
        return ScanIegalResources(resourcePath, outputPath);
    }

    return ScanSubLimitkeyResources(entry, resourcePath, outputPath);
}

bool ResourceAppend::ScanSubLimitkeyResources(const FileEntry entry, const string &resourcePath,
    const string &outputPath)
{
    for (const auto &child : entry.GetChilds()) {
        string limitKey = child->GetFilePath().GetFilename();
        if (ResourceUtil::IsIgnoreFile(*child)) {
            continue;
        }

        if (limitKey == RAW_FILE_DIR || limitKey == RES_FILE_DIR) {
            if (!ScanRawFilesOrResFiles(child->GetFilePath().GetPath(), outputPath, limitKey)) {
                return false;
            }
            continue;
        }

        if (child->IsFile()) {
            PrintError(GetError(ERR_CODE_INVALID_RESOURCE_PATH)
                .FormatCause(child->GetFilePath().GetPath().c_str(), "not a directory"));
            return false;
        }

        if (!ScanLimitKey(child, limitKey, outputPath)) {
            return false;
        }
    }
    return true;
}

bool ResourceAppend::ScanIegalResources(const string &resourcePath, const string &outputPath)
{
    FileEntry entry(resourcePath);
    if (!entry.Init()) {
        return false;
    }
    for (const auto &child : entry.GetChilds()) {
        if (!ScanSingleFile(child->GetFilePath().GetPath(), outputPath)) {
            return false;
        }
    }
    return true;
}
bool ResourceAppend::ScanLimitKey(const unique_ptr<FileEntry> &entry,
    const string &limitKey, const string outputPath)
{
    vector<KeyParam> keyParams;
    if (!KeyParser::Parse(limitKey, keyParams)) {
        PrintError(GetError(ERR_CODE_INVALID_LIMIT_KEY).FormatCause(limitKey.c_str())
            .SetPosition(entry->GetFilePath().GetPath().c_str()));
        return false;
    }

    for (const auto &child : entry->GetChilds()) {
        string fileCuster = child->GetFilePath().GetFilename();
        if (ResourceUtil::IsIgnoreFile(*child)) {
            continue;
        }

        if (child->IsFile()) {
            PrintError(GetError(ERR_CODE_INVALID_RESOURCE_PATH)
                .FormatCause(child->GetFilePath().GetPath().c_str(), "not a directory"));
            return false;
        }

        ResType resType = ResourceUtil::GetResTypeByDir(fileCuster);
        if (resType == ResType::INVALID_RES_TYPE) {
            PrintError(GetError(ERR_CODE_INVALID_RESOURCE_DIR)
                           .FormatCause(fileCuster.c_str(), ResourceUtil::GetAllResTypeDirs().c_str())
                           .SetPosition(child->GetFilePath().GetPath().c_str()));
            return false;
        }

        DirectoryInfo directoryInfo = { limitKey, fileCuster, child->GetFilePath().GetPath(), keyParams, resType};
        if (!ScanFiles(child, directoryInfo, outputPath)) {
            return false;
        }
    }
    return true;
}


bool ResourceAppend::ScanFiles(const unique_ptr<FileEntry> &entry,
    const DirectoryInfo &directoryInfo, const string &outputPath)
{
    for (const auto &child : entry->GetChilds()) {
        string filename =  child->GetFilePath().GetFilename();
        if (ResourceUtil::IsIgnoreFile(*child)) {
            continue;
        }

        if (!child->IsFile()) {
            PrintError(GetError(ERR_CODE_INVALID_RESOURCE_PATH)
                .FormatCause(child->GetFilePath().GetPath().c_str(), "not a file"));
            return false;
        }

        FileInfo fileInfo = {directoryInfo, child->GetFilePath().GetPath(), filename};
        if (!ScanFile(fileInfo, outputPath)) {
            return false;
        }
    }
    return true;
}

bool ResourceAppend::ScanFile(const FileInfo &fileInfo, const string &outputPath)
{
    if (ResourceAppend::IsBaseIdDefined(fileInfo)) {
        cout << "Warning: id_defined.json does not compile to generate intermediate files" << endl;
        FileEntry::FilePath outPath(outputPath);
        return ResourceUtil::CopyFileInner(fileInfo.filePath, outPath.Append(ID_DEFINED_FILE).GetPath());
    }

    unique_ptr<IResourceCompiler> resourceCompiler =
        ResourceCompilerFactory::CreateCompilerForAppend(fileInfo.dirType, outputPath);
    if (resourceCompiler == nullptr) {
        return true;
    }

    if (resourceCompiler->CompileForAppend(fileInfo) != RESTOOL_SUCCESS) {
        return false;
    }

    ostringstream outStream;
    const auto &items = resourceCompiler->GetResourceItems();
    for (const auto &item : items) {
        for (const auto &resourceItem : item.second) {
            if (!WriteResourceItem(resourceItem, outStream)) {
                return false;
            }
        }
    }

    string hash = ResourceUtil::GenerateHash(fileInfo.filePath);
    FileEntry::FilePath output(outputPath);
    if (!WriteFileInner(outStream, output.Append(hash).GetPath())) {
        return false;
    }
    return true;
}

bool ResourceAppend::ScanSingleFile(const string &filePath, const string &outputPath)
{
    if (filePath.find(RAW_FILE_DIR) != string::npos) {
        return WriteRawFilesOrResFiles(filePath, outputPath, RAW_FILE_DIR);
    }

    if (filePath.find(RES_FILE_DIR) != string::npos) {
        return WriteRawFilesOrResFiles(filePath, outputPath, RES_FILE_DIR);
    }

    FileEntry::FilePath path(filePath);
    string fileCuster = path.GetParent().GetFilename();
    ResType resType = ResourceUtil::GetResTypeByDir(fileCuster);
    if (resType == ResType::INVALID_RES_TYPE) {
        PrintError(GetError(ERR_CODE_INVALID_RESOURCE_DIR)
                       .FormatCause(fileCuster.c_str(), ResourceUtil::GetAllResTypeDirs().c_str())
                       .SetPosition(filePath));
        return false;
    }

    string limitKey = path.GetParent().GetParent().GetFilename();
    vector<KeyParam> keyParams;
    if (!KeyParser::Parse(limitKey, keyParams)) {
        PrintError(GetError(ERR_CODE_INVALID_LIMIT_KEY).FormatCause(limitKey.c_str()).SetPosition(filePath));
        return false;
    }

    DirectoryInfo directoryInfo = {limitKey, fileCuster, path.GetParent().GetPath(), keyParams, resType};
    FileInfo fileInfo = {directoryInfo, filePath, path.GetFilename() };
    if (!ScanFile(fileInfo, outputPath)) {
        return false;
    }
    return true;
}

bool ResourceAppend::WriteFileInner(ostringstream &outStream, const string &outputPath) const
{
#ifdef __WIN32
    HANDLE hWriteFile = CreateFile(outputPath.c_str(), GENERIC_WRITE, FILE_SHARE_WRITE,
        nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hWriteFile == INVALID_HANDLE_VALUE) {
        PrintError(GetError(ERR_CODE_CREATE_FILE_ERROR)
            .FormatCause(outputPath.c_str(), to_string(GetLastError()).c_str()));
        return false;
    }

    DWORD writeBytes;
    if (!WriteFile(hWriteFile, outStream.str().c_str(), outStream.tellp(), &writeBytes, nullptr)) {
        PrintError(GetError(ERR_CODE_OPEN_FILE_ERROR)
            .FormatCause(outputPath.c_str(), to_string(GetLastError()).c_str()));
        CloseHandle(hWriteFile);
        return false;
    }
    CloseHandle(hWriteFile);
#else
    ofstream out(outputPath, ofstream::out | ofstream::binary);
    if (!out.is_open()) {
        PrintError(GetError(ERR_CODE_OPEN_FILE_ERROR).FormatCause(outputPath.c_str(), strerror(errno)));
        return false;
    }
    out << outStream.str();
#endif
    return true;
}

bool ResourceAppend::WriteResourceItem(const ResourceItem &resourceItem, ostringstream &out)
{
    uint32_t size = resourceItem.GetName().length();
    out.write(reinterpret_cast<const char *>(&size), sizeof(int32_t));
    out.write(reinterpret_cast<const char *>(resourceItem.GetName().c_str()), size);

    size = resourceItem.GetLimitKey().length();
    out.write(reinterpret_cast<const char *>(&size), sizeof(int32_t));
    out.write(reinterpret_cast<const char *>(resourceItem.GetLimitKey().c_str()), size);

    size = resourceItem.GetFilePath().length();
    out.write(reinterpret_cast<const char *>(&size), sizeof(int32_t));
    out.write(reinterpret_cast<const char *>(resourceItem.GetFilePath().c_str()), size);

    int32_t type = static_cast<int32_t>(resourceItem.GetResType());
    out.write(reinterpret_cast<const char *>(&type), sizeof(int32_t));

    size = resourceItem.GetKeyParam().size();
    out.write(reinterpret_cast<const char *>(&size), sizeof(int32_t));
    for (const auto &keyParam : resourceItem.GetKeyParam()) {
        out.write(reinterpret_cast<const char *>(&keyParam.keyType), sizeof(int32_t));
        out.write(reinterpret_cast<const char *>(&keyParam.value), sizeof(int32_t));
    }

    size =  resourceItem.GetDataLength();
    out.write(reinterpret_cast<const char *>(&size), sizeof(int32_t));
    out.write(reinterpret_cast<const char *>(resourceItem.GetData()), size);
    return true;
}

bool ResourceAppend::LoadResourceItem(const string &filePath)
{
#ifdef __WIN32
    return LoadResourceItemWin(filePath);
#else
    ifstream in(filePath, ifstream::in | ifstream::binary);
    if (!in.is_open()) {
        PrintError(GetError(ERR_CODE_OPEN_FILE_ERROR).FormatCause(filePath.c_str(), strerror(errno)));
        return false;
    }
    
    in.seekg(0, in.end);
    int64_t length = static_cast<int64_t>(in.tellg());
    in.seekg(0, in.beg);
    if (length <= 0) {
        PrintError(GetError(ERR_CODE_READ_FILE_ERROR).FormatCause(filePath.c_str(), "file is empty"));
        return false;
    }
    if (length > static_cast<int64_t>(std::numeric_limits<int32_t>::max())) {
        PrintError(GetError(ERR_CODE_READ_FILE_ERROR).FormatCause(filePath.c_str(), "file too large"));
        return false;
    }
    std::vector<char> buffer(static_cast<size_t>(length));
    in.read(buffer.data(), length);
    return LoadResourceItemFromMem(buffer.data(), static_cast<int32_t>(length));
#endif
}

bool ResourceAppend::ScanRawFilesOrResFiles(const string &path, const string &outputPath, const string &limit)
{
    FileEntry entry(path);
    if (!entry.Init()) {
        return false;
    }

    for (const auto &child : entry.GetChilds()) {
        string filename =  child->GetFilePath().GetFilename();
        if (ResourceUtil::IsIgnoreFile(*child)) {
            continue;
        }

        bool ret = false;
        if (child->IsFile()) {
            ret = WriteRawFilesOrResFiles(child->GetFilePath().GetPath(), outputPath, limit);
        } else {
            ret = ScanRawFilesOrResFiles(child->GetFilePath().GetPath(), outputPath, limit);
        }

        if (!ret) {
            return false;
        }
    }
    return true;
}

bool ResourceAppend::WriteRawFilesOrResFiles(const string &filePath, const string &outputPath, const string &limit)
{
    string::size_type pos = filePath.find(limit);
    if (pos == string::npos) {
        PrintError(GetError(ERR_CODE_INVALID_RESOURCE_PATH).FormatCause(filePath.c_str(), "missing separator"));
        return false;
    }

    string sub = filePath.substr(pos);
    sub = FileEntry::FilePath(RESOURCES_DIR).Append(sub).GetPath();
    vector<KeyParam> keyParams;
    auto iter = g_copyFileMap.find(limit);
    ResourceItem resourceItem("", keyParams, iter->second);
    resourceItem.SetData(sub);
    resourceItem.SetFilePath(filePath);
    resourceItem.SetLimitKey("");

    ostringstream outStream;
    if (!WriteResourceItem(resourceItem, outStream)) {
        return false;
    }

    string hash = ResourceUtil::GenerateHash(filePath);
    FileEntry::FilePath output(outputPath);
    if (!WriteFileInner(outStream, output.Append(hash).GetPath())) {
        return false;
    }
    return true;
}

bool ResourceAppend::Push(const shared_ptr<ResourceItem> &resourceItem)
{
    string idName = ResourceUtil::GetIdName(resourceItem->GetName(), resourceItem->GetResType());
    int64_t id = IdWorker::GetInstance().GenerateId(resourceItem->GetResType(), idName);
    if (id < 0) {
        PrintError(GetError(ERR_CODE_RESOURCE_ID_NOT_DEFINED)
                       .FormatCause(resourceItem->GetName().c_str(),
                                    ResourceUtil::ResTypeToString(resourceItem->GetResType()).c_str()));
        return false;
    }

    if (!CheckModuleResourceItem(resourceItem, id)) {
        return false;
    }

    const auto &result = items_.find(id);
    if (result == items_.end()) {
        items_[id].push_back(resourceItem);
        AddRef(resourceItem);
        return true;
    }

    if (find_if(result->second.begin(), result->second.end(), [resourceItem](auto &iter) {
              return resourceItem->GetLimitKey() == iter->GetLimitKey();
        }) != result->second.end()) {
        return true;
    }

    items_[id].push_back(resourceItem);
    AddRef(resourceItem);
    return true;
}

void ResourceAppend::AddRef(const shared_ptr<ResourceItem> &resourceItem)
{
    string data(reinterpret_cast<const char *>(resourceItem->GetData()), resourceItem->GetDataLength());
    ResType resType = resourceItem->GetResType();
    if (resType == ResType::MEDIA) {
        if (FileEntry::FilePath(resourceItem->GetFilePath()).GetExtension() == JSON_EXTENSION) {
            refs_.push_back(resourceItem);
        }
        return;
    }

    if (resType == ResType::PROF) {
        if (resourceItem->GetLimitKey() != "base" ||
            FileEntry::FilePath(resourceItem->GetFilePath()).GetExtension() != JSON_EXTENSION) {
            return;
        }
        refs_.push_back(resourceItem);
        return;
    }

    if (regex_match(data, regex(".*\\$.+:.*"))) {
        refs_.push_back(resourceItem);
    }
}

bool ResourceAppend::LoadResourceItemFromMem(const char buffer[], int32_t length)
{
    int32_t offset = 0;
    do {
        // name
        string nameStr = ParseString(buffer, length, offset);
        // limit key
        string limitKeyStr = ParseString(buffer, length, offset);
        // file path
        string filePathStr = ParseString(buffer, length, offset);
        // ResType
        int32_t type = ParseInt32(buffer, length, offset);
        ResType resType = static_cast<ResType>(type);
        // keyParam
        int32_t keyParamSize = ParseInt32(buffer, length, offset);
        vector<KeyParam> keyParams;
        for (int i = 0; i < keyParamSize; i++) {
            KeyParam keyParam;
            keyParam.keyType = static_cast<KeyType>(ParseInt32(buffer, length, offset));
            int32_t value = ParseInt32(buffer, length, offset);
            if (value == -1) {
                return false;
            }
            keyParam.value = static_cast<uint32_t>(value);
            keyParams.push_back(keyParam);
        }
        if (limitKeyStr != "base" && !limitKeyStr.empty() && !SelectCompileParse::IsSelectCompile(keyParams)) {
            return true;
        }
        // data
        string data = ParseString(buffer, length, offset);
        if (resType ==  ResType::RAW || resType ==  ResType::RES) {
            FileEntry::FilePath outPath(packageParser_.GetOutput());
            if (ResourceUtil::FileExist(outPath.Append(data).GetPath())) {
                continue;
            }
            if (!ResourceUtil::CreateDirs(outPath.Append(data).GetParent().GetPath())) {
                return false;
            }

            if (!ResourceUtil::FileExist(filePathStr)) {
                continue;
            }

            if (!ResourceUtil::CopyFileInner(filePathStr, outPath.Append(data).GetPath())) {
                return false;
            }
            continue;
        }

        shared_ptr<ResourceItem> resourceItem = make_shared<ResourceItem>(nameStr, keyParams, resType);
        resourceItem->SetData(reinterpret_cast<const int8_t *>(data.c_str()), data.length());
        resourceItem->SetLimitKey(limitKeyStr);
        resourceItem->SetFilePath(filePathStr);
        if (!Push(resourceItem)) {
            return false;
        }
    } while (offset < length);
    return true;
}

string ResourceAppend::ParseString(const char buffer[], int32_t length, int32_t &offset) const
{
    int32_t size = ParseInt32(buffer, length, offset);
    if (size < 0 || static_cast<int64_t>(offset) + static_cast<int64_t>(size) > static_cast<int64_t>(length)) {
        offset = length;
        return "";
    }

    if (size == 0) {
        return "";
    }

    string value(buffer + offset, size);
    offset += size;
    return value;
}

int32_t ResourceAppend::ParseInt32(const char buffer[], int32_t length, int32_t &offset) const
{
    if (static_cast<int64_t>(offset) + static_cast<int64_t>(sizeof(int32_t)) > static_cast<int64_t>(length)) {
        offset = length;
        return -1;
    }

    int32_t size = 0;
    if (memcpy_s(&size, sizeof(int32_t), buffer  + offset, sizeof(int32_t)) != EOK) {
        offset = length;
        return -1;
    }
    offset += sizeof(int32_t);
    return size;
}

bool ResourceAppend::CheckModuleResourceItem(const shared_ptr<ResourceItem> &resourceItem, int64_t id)
{
    const auto &result = itemsForModule_.find(id);
    if (result == itemsForModule_.end()) {
        itemsForModule_[id].push_back(resourceItem);
        return true;
    }

    const auto &ret = find_if(result->second.begin(), result->second.end(), [resourceItem](auto iter) {
             return  resourceItem->GetLimitKey() == iter->GetLimitKey();
    });

    if (ret != result->second.end()) {
        PrintError(GetError(ERR_CODE_RESOURCE_DUPLICATE)
                       .FormatCause(resourceItem->GetName().c_str(), (*ret)->GetFilePath().c_str(),
                                    resourceItem->GetFilePath().c_str()));
        return false;
    }

    itemsForModule_[id].push_back(resourceItem);
    return true;
}

#ifdef __WIN32
bool ResourceAppend::LoadResourceItemWin(const string &filePath)
{
    bool result = false;
    HANDLE hReadFile = CreateFile(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_READONLY | FILE_FLAG_RANDOM_ACCESS, nullptr);
    if (hReadFile == INVALID_HANDLE_VALUE) {
        PrintError(GetError(ERR_CODE_CREATE_FILE_ERROR)
            .FormatCause(filePath.c_str(), to_string(GetLastError()).c_str()).SetPosition(filePath));
        return result;
    }

    DWORD fileSize = GetFileSize(hReadFile, nullptr);
    HANDLE hFileMap = CreateFileMapping(hReadFile, nullptr, PAGE_READONLY, 0, fileSize, nullptr);
    if (hFileMap == INVALID_HANDLE_VALUE) {
        string errMsg = "create mapping error: " + to_string(GetLastError());
        PrintError(GetError(ERR_CODE_READ_FILE_ERROR).FormatCause(filePath.c_str(), errMsg.c_str()));
        CloseHandle(hReadFile);
        return result;
    }

    void* pBuffer = MapViewOfFile(hFileMap, FILE_MAP_READ, 0, 0, 0);
    if (pBuffer == nullptr) {
        string errMsg = "map view of file error: " + to_string(GetLastError());
        PrintError(GetError(ERR_CODE_READ_FILE_ERROR).FormatCause(filePath.c_str(), errMsg.c_str()));
        CloseHandle(hFileMap);
        CloseHandle(hReadFile);
        return result;
    }

    char* buffer = reinterpret_cast<char *>(pBuffer);
    result = LoadResourceItemFromMem(buffer, fileSize);
    UnmapViewOfFile(pBuffer);
    CloseHandle(hFileMap);
    CloseHandle(hReadFile);
    return result;
}
#endif

bool ResourceAppend::IsBaseIdDefined(const FileInfo &fileInfo)
{
    FileEntry::FilePath filePath(fileInfo.filePath);
    return filePath.GetParent().GetParent().GetFilename() == "base" &&
        filePath.GetParent().GetFilename() == "element" &&
        fileInfo.filename == ID_DEFINED_FILE;
}

void ResourceAppend::CheckAllItems(vector<pair<ResType, string>> &noBaseResource)
{
    for (const auto &item : items_) {
        bool found = any_of(item.second.begin(), item.second.end(), [](const auto &iter) {
            return iter->GetLimitKey() == "base";
        });
        if (!found) {
            auto firstItem = item.second.front();
            bool ret = any_of(noBaseResource.begin(), noBaseResource.end(), [firstItem](const auto &iterItem) {
                return (firstItem->GetResType() == iterItem.first)  &&
                    (firstItem->GetName() == iterItem.second);
            });
            if (!ret) {
                noBaseResource.push_back(make_pair(firstItem->GetResType(), firstItem->GetName()));
            }
        }
    }
}
}
}
}