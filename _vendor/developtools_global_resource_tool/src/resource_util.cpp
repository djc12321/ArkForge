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

#include "resource_util.h"
#include <algorithm>
#include <climits>
#include <cstdlib>
#include <fstream>
#include <mutex>
#include <iostream>
#include <iomanip>
#include <regex>
#include <sstream>
#include "file_entry.h"
#include "restool_errors.h"

namespace OHOS {
namespace Global {
namespace Restool {
using namespace std;
const map<string, IgnoreType> ResourceUtil::DEFAULT_IGNORE_FILE_REGEX = {
    { "\\.git", IgnoreType::IGNORE_ALL },
    { "\\.svn", IgnoreType::IGNORE_ALL },
    { ".+\\.scc", IgnoreType::IGNORE_ALL },
    { "\\.ds_store", IgnoreType::IGNORE_ALL },
    { "desktop\\.ini", IgnoreType::IGNORE_ALL },
    { "picasa\\.ini", IgnoreType::IGNORE_ALL },
    { "\\..+", IgnoreType::IGNORE_ALL },
    { "cvs", IgnoreType::IGNORE_ALL },
    { "thumbs\\.db", IgnoreType::IGNORE_ALL },
    { ".+~", IgnoreType::IGNORE_ALL }
};
const std::set<std::string> IGNORE_PATH_OPTIONS = { "--ignored-path", "ignoreResourcePathPattern" };
static std::map<std::string, IgnoreType> g_userIgnoreRegex;
static bool g_isUseCustomRegex = false;
static std::string g_ignoreOption;
static bool g_isIgnorePath = false;
static std::set<int64_t> g_harResourceIds;

static std::mutex fileMutex_;
static std::mutex g_harResourceMutex;

void ResourceUtil::Split(const string &str, vector<string> &out, const string &splitter)
{
    string::size_type len = str.size();
    string::size_type begin = 0;
    string::size_type end = str.find(splitter, begin);
    while (end != string::npos) {
        string sub = str.substr(begin, end - begin);
        out.push_back(sub);
        begin = end + splitter.size();
        if (begin >= len) {
            break;
        }
        end = str.find(splitter, begin);
    }

    if (begin < len) {
        out.push_back(str.substr(begin));
    }
}

bool ResourceUtil::FileExist(const string &path)
{
    return FileEntry::Exist(path);
}

bool ResourceUtil::RmoveAllDir(const string &path)
{
    return FileEntry::RemoveAllDir(path);
}

bool ResourceUtil::RmoveFile(const string &path)
{
    return FileEntry::RemoveFile(path);
}

bool ResourceUtil::OpenJsonFile(const string &path, cJSON **root, const bool &printError)
{
    ifstream ifs(FileEntry::AdaptLongPath(path), ios::binary);
    if (!ifs.is_open()) {
        if (printError) {
            PrintError(GetError(ERR_CODE_OPEN_JSON_FAIL).FormatCause(path.c_str(), strerror(errno)));
        }
        return false;
    }

    string jsonString((istreambuf_iterator<char>(ifs)), istreambuf_iterator<char>());
    *root = cJSON_Parse(jsonString.c_str());
    if (!*root) {
        if (printError) {
            PrintError(GetError(ERR_CODE_JSON_FORMAT_ERROR).SetPosition(path));
        }
        ifs.close();
        return false;
    }
    ifs.close();
    return true;
}

bool ResourceUtil::SaveToJsonFile(const string &path, const cJSON *root)
{
    FileEntry::MakeWritable(path);
    ofstream out(FileEntry::AdaptLongPath(path), ofstream::out | ofstream::binary);
    if (!out.is_open()) {
        PrintError(GetError(ERR_CODE_OPEN_FILE_ERROR).FormatCause(path.c_str(), strerror(errno)));
        return false;
    }
    char *jsonString = cJSON_Print(root);
    out << jsonString;
    cJSON_free(jsonString);
    jsonString = nullptr;
    out.close();
    return true;
}

ResType ResourceUtil::GetResTypeByDir(const string &name)
{
    auto ret = g_fileClusterMap.find(name);
    if (ret == g_fileClusterMap.end()) {
        return ResType::INVALID_RES_TYPE;
    }
    return ret->second;
}

string ResourceUtil::GetAllResTypeDirs()
{
    string dirs = "[";
    for (auto iter = g_fileClusterMap.begin(); iter != g_fileClusterMap.end(); ++iter) {
        dirs.append("\"").append(iter->first).append("\",");
    }
    dirs.pop_back();
    dirs.append("]");
    return dirs;
}

string ResourceUtil::ResTypeToString(ResType type)
{
    auto ret = find_if(g_fileClusterMap.begin(), g_fileClusterMap.end(), [type](auto iter) {
        return iter.second == type;
    });
    if (ret != g_fileClusterMap.end()) {
        return ret->first;
    }

    ret = find_if(g_contentClusterMap.begin(), g_contentClusterMap.end(), [type](auto iter) {
        return iter.second == type;
    });
    if (ret != g_contentClusterMap.end()) {
        return ret->first;
    }
    return "";
}

string ResourceUtil::GetIdName(const string &name, ResType type)
{
    if (type != ResType::MEDIA && type != ResType::PROF) {
        return name;
    }

    string::size_type pos = name.find_last_of(".");
    if (pos != string::npos) {
        return name.substr(0, pos);
    }
    return name;
}

string ResourceUtil::ComposeStrings(const vector<string> &contents, bool addNull)
{
    string result;
    for (const auto &iter : contents) {
        if (iter.length() > UINT16_MAX) {
            return "";
        }

        uint16_t size = iter.length();
        if (addNull) {
            size += sizeof(char);
        }
        result.append(sizeof(char), (size & 0xff));
        result.append(sizeof(char), (size >> 8)); // Move 8 bits to the right
        result.append(iter);
        result.append(sizeof(char), '\0');
        if (result.length() > UINT16_MAX) {
            return "";
        }
    }
    return result;
}

vector<string> ResourceUtil::DecomposeStrings(const string &content)
{
    vector<string> result;
    size_t length = content.length();
    size_t pos = 0;
    const size_t HEAD_LENGTH = 2;
    while (pos < length) {
        if (pos + HEAD_LENGTH >= length) {
            result.clear();
            return result;
        }
        uint16_t size = (content[pos] & 0xff) | ((content[pos + 1] & 0xff) << 8); // Move 8 bits to the left
        pos += HEAD_LENGTH;

        if (pos + size >= length) {
            result.clear();
            return result;
        }
        string buffer = content.substr(pos, size);
        result.push_back(buffer);
        pos += size + sizeof(char);
    }
    return result;
}

ResType ResourceUtil::GetResTypeFromString(const string &type)
{
    ResType resType = GetResTypeByDir(type);
    if (resType != ResType::INVALID_RES_TYPE) {
        return resType;
    }

    auto ret = g_contentClusterMap.find(type);
    if (ret != g_contentClusterMap.end()) {
        return ret->second;
    }
    return ResType::INVALID_RES_TYPE;
}

bool ResourceUtil::CopyFileInner(const string &src, const string &dst)
{
    return FileEntry::CopyFileInner(src, dst);
}

bool ResourceUtil::CreateDirs(const string &filePath)
{
    std::lock_guard<std::mutex> lock(fileMutex_);
    if (FileExist(filePath)) {
        return true;
    }

    if (!FileEntry::CreateDirs(filePath)) {
        PrintError(GetError(ERR_CODE_CREATE_FILE_ERROR).FormatCause(filePath.c_str(), strerror(errno)));
        return false;
    }
    return true;
}

bool ResourceUtil::IsIgnoreFile(const FileEntry &fileEntry)
{
    map<string, IgnoreType> regexs;
    std::string regexSources;
    string fileName = fileEntry.GetFilePath().GetFilename();
    string filePath = fileEntry.GetFilePath().GetPath();
    filePath = regex_replace(filePath, regex("\\\\+"), "/");
    if (g_isUseCustomRegex) {
        regexs = g_userIgnoreRegex;
        regexSources = "user";
    } else {
        regexs = DEFAULT_IGNORE_FILE_REGEX;
        regexSources = "default";
        transform(fileName.begin(), fileName.end(), fileName.begin(), ::tolower);
    }
    bool isFile = fileEntry.IsFile();
    for (const auto &iter : regexs) {
        if ((iter.second == IgnoreType::IGNORE_FILE && !isFile) || (iter.second == IgnoreType::IGNORE_DIR && isFile)) {
            continue;
        }
        if (regex_match(fileName, regex(iter.first))) {
            cout << "Info: file '" << fileName << "' is ignored by " << regexSources << " filename pattern '"
                 << iter.first << "'." << endl;
            return true;
        }

        if (g_isUseCustomRegex && g_isIgnorePath) {
            if (regex_match(filePath, regex(iter.first))) {
                cout << "Info: file '" << filePath << "' is ignored by " << regexSources << " filepath pattern '"
                     << iter.first << "'." << endl;
                return true;
            }
        }
    }
    return false;
}

string ResourceUtil::GenerateHash(const string &key)
{
    hash<string> hash_function;
    return to_string(hash_function(key));
}

string ResourceUtil::RealPath(const string &path)
{
    return FileEntry::RealPath(path);
}

bool ResourceUtil::IslegalPath(const string &path)
{
    return path == "element" || path == "media" || path == "profile";
}

void ResourceUtil::StringReplace(string &sourceStr, const string &oldStr, const string &newStr)
{
    string::size_type pos = 0;
    string::size_type oldSize = oldStr.size();
    string::size_type newSize = newStr.size();
    while ((pos = sourceStr.find(oldStr, pos)) != string::npos) {
        sourceStr.replace(pos, oldSize, newStr.c_str());
        pos += newSize;
    }
}

string ResourceUtil::GetLocaleLimitkey(const KeyParam &KeyParam)
{
    const char *rawValue = reinterpret_cast<const char *>(&KeyParam.value);
    size_t len = sizeof(KeyParam.value);
    char tmp[len + 1];
    int j = 0;
    for (size_t i = 1; i <= len; i++) {
        if (*(rawValue + len - i)) {
            tmp[j++] = *(rawValue + len - i);
        }
    }
    tmp[j] = '\0';
    return string(tmp);
}

string ResourceUtil::GetDeviceTypeLimitkey(const KeyParam &KeyParam)
{
    auto ret = find_if(g_deviceMap.begin(), g_deviceMap.end(), [KeyParam](const auto &iter) {
        return KeyParam.value == static_cast<const uint32_t>(iter.second);
    });
    if (ret == g_deviceMap.end()) {
        return string();
    }
    return ret->first;
}

string ResourceUtil::GetResolutionLimitkey(const KeyParam &KeyParam)
{
    auto ret = find_if(g_resolutionMap.begin(), g_resolutionMap.end(), [KeyParam](const auto &iter) {
        return KeyParam.value == static_cast<const uint32_t>(iter.second);
    });
    if (ret == g_resolutionMap.end()) {
        return string();
    }
    return ret->first;
}

string ResourceUtil::GetKeyParamValue(const KeyParam &KeyParam)
{
    string val;
    switch (KeyParam.keyType) {
        case KeyType::ORIENTATION:
            val = KeyParam.value == static_cast<const uint32_t>(OrientationType::VERTICAL) ? "vertical" : "horizontal";
            break;
        case KeyType::NIGHTMODE:
            val = KeyParam.value == static_cast<const uint32_t>(NightMode::DARK) ? "dark" : "light";
            break;
        case KeyType::DEVICETYPE:
            val = GetDeviceTypeLimitkey(KeyParam);
            break;
        case KeyType::RESOLUTION:
            val = GetResolutionLimitkey(KeyParam);
            break;
        case KeyType::LANGUAGE:
        case KeyType::SCRIPT:
        case KeyType::REGION:
            val = GetLocaleLimitkey(KeyParam);
            break;
        case KeyType::INPUTDEVICE:
            val = KeyParam.value == static_cast<const uint32_t>(InputDevice::INPUTDEVICE_NOT_SET) ?
            "not set" : "pointDevice";
            break;
        default:
            val = to_string(KeyParam.value);
            break;
    }
    return val;
}

string ResourceUtil::PaserKeyParam(const vector<KeyParam> &keyParams)
{
    if (keyParams.size() == 0) {
        return "base";
    }
    string result;
    for (const auto &keyparam : keyParams) {
        string limitKey = GetKeyParamValue(keyparam);
        if (limitKey.empty()) {
            continue;
        }
        if (keyparam.keyType == KeyType::MCC) {
            limitKey = "mcc" + limitKey;
        }
        if (keyparam.keyType == KeyType::MNC) {
            limitKey = "mnc" + limitKey;
        }
        if (keyparam.keyType == KeyType::REGION || keyparam.keyType == KeyType::MNC) {
            result = result + "_" + limitKey;
        } else {
            result = result + "-" + limitKey;
        }
    }
    if (!result.empty()) {
        result = result.substr(1);
    }
    return result;
}

string ResourceUtil::DecToHexStr(const uint32_t i)
{
    stringstream ot;
    string result;
    ot << setiosflags(ios::uppercase) << "0x" << hex << setw(8) << setfill('0') << i; // 0x expadding 8 bit
    ot >> result;
    return result;
}

bool ResourceUtil::CheckHexStr(const string &hex)
{
    if (regex_match(hex, regex("^0[xX][0-9a-fA-F]{8}"))) {
        return true;
    }
    return false;
}

string ResourceUtil::GetAllRestypeString()
{
    string result = "[";
    for (auto iter = g_contentClusterMap.begin(); iter != g_contentClusterMap.end(); ++iter) {
        result.append("\"").append(iter->first).append("\"").append(",");
    }
    result.pop_back();
    result.append("]");
    return result;
}

FileEntry::FilePath ResourceUtil::GetBaseElementPath(const string input)
{
    return FileEntry::FilePath(input).Append("base").Append("element");
}

FileEntry::FilePath ResourceUtil::GetMainPath(const string input)
{
    return FileEntry::FilePath(input).GetParent();
}

uint32_t ResourceUtil::GetNormalSize(const vector<KeyParam> &keyParams, uint32_t index)
{
    string device;
    string dpi;
    if (keyParams.size() == 0) {
        device = "phone";
        dpi = "sdpi";
    }
    for (const auto &keyparam : keyParams) {
        string limitKey = GetKeyParamValue(keyparam);
        if (limitKey.empty()) {
            continue;
        }
        if (keyparam.keyType == KeyType::DEVICETYPE) {
            device = limitKey;
        } else if (keyparam.keyType == KeyType::RESOLUTION) {
            dpi = limitKey;
        }
    }
    if (device.empty()) {
        device = "phone";
    }
    if (dpi.empty()) {
        dpi = "sdpi";
    }
    if (device != "phone" && device != "tablet") {
        return 0;
    }
    return g_normalIconMap.find(dpi + "-" + device)->second[index];
}

bool ResourceUtil::isUnicodeInPlane15or16(int unicode)
{
    return (unicode >= 0xF0000 && unicode <= 0xFFFFF) || (unicode >= 0x100000 && unicode <= 0x10FFFF);
}

void ResourceUtil::RemoveSpaces(string &str)
{
    str.erase(0, str.find_first_not_of(" "));
    str.erase(str.find_last_not_of(" ") + 1); // move back one place
}

bool ResourceUtil::IsIntValue(const cJSON *node)
{
    if (node && cJSON_IsNumber(node)) {
        double num = node->valuedouble;
        if (num == static_cast<int>(num)) {
            return true;
        } else {
            return false;
        }
    }
    return false;
}

bool ResourceUtil::IsValidName(const string &name)
{
    if (!regex_match(name, regex("[a-zA-Z0-9_]+"))) {
        return false;
    }
    return true;
}

void ResourceUtil::PrintWarningMsg(vector<pair<ResType, string>> &noBaseResource)
{
    for (const auto &item : noBaseResource) {
        cerr << "Warning: the " << ResourceUtil::ResTypeToString(item.first);
        cerr << " of '" << item.second << "' does not have a base resource." << endl;
    }
}

string ResourceUtil::KeyTypeToStr(KeyType type)
{
    string ret("unknown type: ");
    auto it = g_keyTypeToStrMap.find(type);
    if (it != g_keyTypeToStrMap.end()) {
        ret = it->second;
    } else {
        ret += to_string(static_cast<uint32_t>(type));
    }
    return ret;
}

bool ResourceUtil::AddIgnoreRegex(const std::string &regex, IgnoreType ignoreType, const std::string &option)
{
    try {
        std::regex rg(regex);
    } catch (std::regex_error err) {
        PrintError(GetError(ERR_CODE_INVALID_IGNORE_FILE).FormatCause(regex.c_str(), err.what())
            .FormatSolution(0, option.c_str()));
        return false;
    }
    g_userIgnoreRegex[regex] = ignoreType;
    return true;
}

void ResourceUtil::SetIgnoreOption(const std::string &option)
{
    g_ignoreOption = option;
    g_isUseCustomRegex = !option.empty();
    g_isIgnorePath = IGNORE_PATH_OPTIONS.count(option);
}

bool ResourceUtil::CheckIgnoreOption(const std::string &option)
{
    if (!g_ignoreOption.empty()) {
        std::string msg = g_ignoreOption == option ? "are duplicate" : "cannot be used together";
        PrintError(
            GetError(ERR_CODE_EXCLUSIVE_OPTION).FormatCause(g_ignoreOption.c_str(), option.c_str(), msg.c_str()));
        return false;
    }
    return true;
}

void ResourceUtil::AddHarResourceId(int64_t harId)
{
    std::lock_guard<std::mutex> lock(g_harResourceMutex);
    g_harResourceIds.emplace(harId);
}

bool ResourceUtil::IsHarResource(int64_t id)
{
    std::lock_guard<std::mutex> lock(g_harResourceMutex);
    return g_harResourceIds.count(id);
}

bool ResourceUtil::StrToInt(const string &str, int &value, int base)
{
    if (str.empty()) {
        return false;
    }
    char *end = nullptr;
    errno = 0;
    long result = strtol(str.c_str(), &end, base);
    if (end == str.c_str() || *end != '\0') {
        return false;
    }
    if (errno == ERANGE || result < INT_MIN || result > INT_MAX) {
        return false;
    }
    value = static_cast<int>(result);
    return true;
}

bool ResourceUtil::StrToLongLong(const string &str, long long &value, int base)
{
    if (str.empty()) {
        return false;
    }
    char *end = nullptr;
    errno = 0;
    long long result = strtoll(str.c_str(), &end, base);
    if (end == str.c_str() || *end != '\0') {
        return false;
    }
    if (errno == ERANGE || result == LLONG_MAX || result == LLONG_MIN) {
        return false;
    }
    value = result;
    return true;
}
}
}
}
