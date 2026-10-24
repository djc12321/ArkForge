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

#ifndef OHOS_RESTOOL_RESOURCE_UTIL_H
#define OHOS_RESTOOL_RESOURCE_UTIL_H

#include <vector>
#include <cJSON.h>
#include "file_entry.h"
#include "resource_data.h"

namespace OHOS {
namespace Global {
namespace Restool {
class ResourceUtil {
public:
    /**
     * @brief split the string with given splitter.
     * @param str: input string.
     * @param out: the array of strings computed by splitter.
     * @param splitter: the split string.
     */
    static void Split(const std::string &str, std::vector<std::string> &out, const std::string &splitter);

    /**
     * @brief Replace sub-string in string
     * @param sourceStr: The original string to operate on
     * @param oldStr: The string to be replaced
     * @param newStr: The new string used
     */
    static void StringReplace(std::string &sourceStr, const std::string &oldStr, const std::string &newStr);

    /**
     * @brief check file exist.
     * @param path: file path.
     * @return true if exist, other false.
     */
    static bool FileExist(const std::string &path);

    /**
     * @brief remove all files in the directory.
     * @param path: input directory.
     * @return true if remove success, other false.
     */
    static bool RmoveAllDir(const std::string &path);

    /**
     * @brief remove file.
     * @param path: input file.
     * @return true if remove success, other false.
     */
    static bool RmoveFile(const std::string &path);

    /**
     * @brief open json file.
     * @param path: json file path.
     * @param root: json root node
     * @param printError: if true, print error message.
     * @return true if open success, other false.
     */
    static bool OpenJsonFile(const std::string &path, cJSON **root, const bool &printError = true);

    /**
     * @brief save json file.
     * @param path: json file path.
     * @param root: json root node
     * @return true if save success, other false.
     */
    static bool SaveToJsonFile(const std::string &path, const cJSON *root);

    /**
     * @brief get resource type from directory.
     * @param name: directory name.
     * @return resource type.
     */
    static ResType GetResTypeByDir(const std::string &name);

    /**
     * @brief get all resource type directorys.
     * @return all resource type.
     */
    static std::string GetAllResTypeDirs();

    /**
     * @brief ResType to string
     * @param type: ResType
     * @return resource type string.
     */
    static std::string ResTypeToString(ResType type);

    /**
     * @brief get id name
     * @param name; id name or file name
     * @param type: ResType
     * @return return id name.
     */
    static std::string GetIdName(const std::string &name, ResType type);

    /**
     * @brief compose multi strings to string
     * @param contents: multi strings
     * @param addNull: if true, string length contains '\0'.
     * @return return string, empty if error
     */
    static std::string ComposeStrings(const std::vector<std::string> &contents, bool addNull = false);

    /**
     * @brief decompose string to multi strings
     * @param content: string
     * @return return string vector, empty if error
     */
    static std::vector<std::string> DecomposeStrings(const std::string &content);

    /**
     * @brief string to ResType
     * @param type: string
     * @return return ResType
     */
    static ResType GetResTypeFromString(const std::string &type);

    /**
     * @brief copy file
     * @param src: source file path
     * @param dst: destination file path
     * @return true if success, other false
     */
    static bool CopyFileInner(const std::string &src, const std::string &dst);

    /**
     * @brief create directories
     * @param filePath: directory path
     * @return true if success, other false
     */
    static bool CreateDirs(const std::string &filePath);

    /**
     * @brief ignore file or directory
     * @param fileEntry: file or directory
     * @return true if ignore, other false
     */
    static bool IsIgnoreFile(const FileEntry &fileEntry);

    /**
     * @brief generate hash string
     * @param key: string
     * @return hash string
     */
    static std::string GenerateHash(const std::string &key);

    /**
     * @brief get an absolute pathname
     * @param path pathname
     * @return absolut pathname
     */
    static std::string RealPath(const std::string &path);

    /**
     * @brief check the directory is legal
     * @param path pathname
     * @return true is legal, other false;
     */
    static bool IslegalPath(const std::string &path);

    /**
     * @brief get an keyParams for limitkey
     * @param keyParams
     * @return limitkey
     */
    static std::string PaserKeyParam(const std::vector<KeyParam> &keyParams);

    /**
     * @brief Decimal to hexadecimal string
     * @param int32_t Decimal
     * @return Hexadecimal string
     */
    static std::string DecToHexStr(const uint32_t id);

    /**
     * @brief Check hexadecimal string
     * @param string Hexadecimal string
     * @return ture Hexadecimal string is legal, other false;
     */
    static bool CheckHexStr(const std::string &hex);

    /**
     * @brief get g_contentClusterMap key string
     * @return All restype string
     */
    static std::string GetAllRestypeString();

    /**
     * @brief get \base\element dir
     * @param string inputpath
     * @return resource\base\element dir
     */
    static FileEntry::FilePath GetBaseElementPath(const std::string input);

    /**
     * @brief get main dir
     * @param string inputpath
     * @return main dir
     */
    static FileEntry::FilePath GetMainPath(const std::string input);

    /**
     * @brief Gets the standard size of icons under different qualifier phrases
     * @param keyParams set of qualifiers
     * @param uint32_t index
     * @return standard size of the png
    */
    static uint32_t GetNormalSize(const std::vector<KeyParam> &keyParams, uint32_t index);

    /**
     * @brief Check if the Unicode code belongs to the 15 plane or 16 plane
     * @param int unicode
     * @return ture Unicode code belongs to the 15 plane or 16 plane, other false;
    */
    static bool isUnicodeInPlane15or16(int unicode);

    /**
     * @brief Remove spaces before and after strings
     * @param str input string
    */
    static void RemoveSpaces(std::string &str);

    /**
     * @brief Check whether the value is int
     * @param snode cJSON node
    */
    static bool IsIntValue(const cJSON *node);

    /**
     * @brief Check whether the resource name is valid
     * @param str resource name
    */
    static bool IsValidName(const std::string &name);

    /**
     * @brief print warning msg
     * @param noBaseResource set of no base resources
    */
    static void PrintWarningMsg(std::vector<std::pair<ResType, std::string>> &noBaseResource);

    /**
     * @brief covert KeyParam to str by keytype
     * @param keyParam: KeyParam
     * @return limit value string
    */
    static std::string GetKeyParamValue(const KeyParam &KeyParam);

    /**
     * @brief keyType to string
     * @param type: KeyType
     * @return limit type string
    */
    static std::string KeyTypeToStr(KeyType type);

    /**
     * @brief add ignore file regex pattern
     * @param regex: the regex pattern
     * @param ignoreType: the ignore file type
     * @param option: the ignore option name
     * @return if add succeed, return true
    */
    static bool AddIgnoreRegex(const std::string &regex, IgnoreType ignoreType, const std::string &option);

    /**
     * @brief set whether use custom ignore regex pattern
     * @param option:the ignore option name
    */
    static void SetIgnoreOption(const std::string &option);

    /**
     * @brief check whether the ignore options is duplicate or conflict.
    */
    static bool CheckIgnoreOption(const std::string &option);

    /**
     * @brief Add resource id of har to global set
     * @param harId: the resource id of har
    */
    static void AddHarResourceId(int64_t harId);

    /**
     * @brief Check whether the resource id is in har
     * @param id: the resource id
    */
    static bool IsHarResource(int64_t id);

    /**
     * @brief Convert string to int
     * @param str: input string
     * @param value: output int value
     * @param base: conversion base, default 10
     * @return true if success, other false
    */
    static bool StrToInt(const std::string &str, int &value, int base = 10);

    /**
     * @brief Convert string to long long
     * @param str: input string
     * @param value: output long long value
     * @param base: conversion base, default 10
     * @return true if success, other false
    */
    static bool StrToLongLong(const std::string &str, long long &value, int base = 10);

private:
    static const std::map<std::string, IgnoreType> DEFAULT_IGNORE_FILE_REGEX;
    static std::string GetLocaleLimitkey(const KeyParam &KeyParam);
    static std::string GetDeviceTypeLimitkey(const KeyParam &KeyParam);
    static std::string GetResolutionLimitkey(const KeyParam &KeyParam);
};
}
}
}
#endif
