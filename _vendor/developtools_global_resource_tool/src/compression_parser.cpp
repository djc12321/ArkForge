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

#include "compression_parser.h"

#include <algorithm>
#include <iostream>
#include <mutex>
#include "restool_errors.h"

namespace OHOS {
namespace Global {
namespace Restool {
using namespace std;
static shared_ptr<CompressionParser> compressionParseMgr = nullptr;
static once_flag compressionParserMgrFlag;

const map<TranscodeError, string> ERRORCODEMAP = {
    { TranscodeError::SUCCESS, "SUCCESS" },
    { TranscodeError::INVALID_PARAMETERS, "INVALID_PARAMETERS" },
    { TranscodeError::IMAGE_ERROR, "IMAGE_ERROR" },
    { TranscodeError::ANIMATED_IMAGE_SKIP, "ANIMATED_IMAGE_SKIP" },
    { TranscodeError::MALLOC_FAILED, "MALLOC_FAILED" },
    { TranscodeError::ENCODE_ASTC_FAILED, "ENCODE_ASTC_FAILED" },
    { TranscodeError::SUPER_COMPRESS_FAILED, "SUPER_COMPRESS_FAILED" },
    { TranscodeError::IMAGE_SIZE_NOT_MATCH, "IMAGE_SIZE_NOT_MATCH" },
    { TranscodeError::IMAGE_RESOLUTION_NOT_MATCH, "IMAGE_RESOLUTION_NOT_MATCH" },
    { TranscodeError::EXCLUDE_MATCH, "EXCLUDE_MATCH" },
    { TranscodeError::LOAD_COMPRESS_FAILED, "LOAD_COMPRESS_FAILED" },
};

CompressionParser::CompressionParser()
    : filePath_(""), extensionPath_(""), mediaSwitch_(false), root_(nullptr), defaultCompress_(false), outPath_("")
{
}

CompressionParser::CompressionParser(const string &filePath)
    : filePath_(filePath), extensionPath_(""), mediaSwitch_(false), root_(nullptr), defaultCompress_(false),
    outPath_("")
{
}

CompressionParser::~CompressionParser()
{
    if (root_) {
        cJSON_Delete(root_);
    }
#ifdef __WIN32
    if (handle_) {
        FreeLibrary(handle_);
        handle_ = nullptr;
    }
#else
    if (handle_) {
        dlclose(handle_);
        handle_ = nullptr;
    }
#endif
}

shared_ptr<CompressionParser> CompressionParser::GetCompressionParser(const string &filePath)
{
    call_once(compressionParserMgrFlag, [&] {
        compressionParseMgr = make_shared<CompressionParser>(filePath);
    });
    return compressionParseMgr;
}

shared_ptr<CompressionParser> CompressionParser::GetCompressionParser()
{
    return GetCompressionParser("");
}

uint32_t CompressionParser::Init()
{
    if (!ResourceUtil::OpenJsonFile(filePath_, &root_)) {
        return RESTOOL_ERROR;
    }
    if (!root_ || !cJSON_IsObject(root_)) {
        PrintError(GetError(ERR_CODE_JSON_FORMAT_ERROR).SetPosition(filePath_));
        return RESTOOL_ERROR;
    }
    cJSON *contextNode = cJSON_GetObjectItem(root_, "context");
    if (!ParseContext(contextNode)) {
        cout << NEW_LINE_PATH << filePath_ << endl;
        return RESTOOL_SUCCESS;
    }
    if (!LoadImageTranscoder()) {
        return RESTOOL_ERROR;
    }
    cJSON *compressionNode = cJSON_GetObjectItem(root_, "compression");
    if (!ParseCompression(compressionNode)) {
        return RESTOOL_ERROR;
    }
    if (!mediaSwitch_) {
        return RESTOOL_SUCCESS;
    }
    cJSON *filtersNode = cJSON_GetObjectItem(compressionNode, "filters");
    if (!ParseFilters(filtersNode)) {
        return RESTOOL_ERROR;
    }
    string caches = outPath_;
    caches.append(SEPARATOR_FILE).append(CACHES_DIR);
    if (!ResourceUtil::CreateDirs(caches)) {
        return RESTOOL_ERROR;
    }
    return RESTOOL_SUCCESS;
}

bool CompressionParser::ParseCompression(const cJSON *compressionNode)
{
    if (!compressionNode) {
        cerr << "Warning: get 'compression' node is empty, the compiled images are not transcoded.";
        cerr << NEW_LINE_PATH << filePath_ << endl;
        return true;
    }
    if (!cJSON_IsObject(compressionNode)) {
        PrintError(GetError(ERR_CODE_JSON_NODE_MISMATCH).FormatCause("compression", "object").SetPosition(filePath_));
        return false;
    }
    cJSON *mediaNode = cJSON_GetObjectItem(compressionNode, "media");
    if (!mediaNode) {
        cerr << "Warning: get 'media' node is empty, the compiled images are not transcoded.";
        cerr << NEW_LINE_PATH << filePath_ << endl;
        return true;
    }
    if (!cJSON_IsObject(mediaNode)) {
        PrintError(GetError(ERR_CODE_JSON_NODE_MISMATCH).FormatCause("media", "object").SetPosition(filePath_));
        return false;
    }
    cJSON *enableNode = cJSON_GetObjectItem(mediaNode, "enable");
    if (!enableNode) {
        cerr << "Warning: get 'enable' node is empty, the compiled images are not transcoded.";
        cerr << NEW_LINE_PATH << filePath_ << endl;
        return true;
    }
    if (!cJSON_IsBool(enableNode)) {
        PrintError(GetError(ERR_CODE_JSON_NODE_MISMATCH).FormatCause("enable", "bool").SetPosition(filePath_));
        return false;
    }
    mediaSwitch_ = cJSON_IsTrue(enableNode);
    return true;
}

bool CompressionParser::ParseContext(const cJSON *contextNode)
{
    if (!contextNode) {
        cout << "Warning: if image transcoding is supported, the 'context' node cannot be empty.";
        return false;
    }
    if (!cJSON_IsObject(contextNode)) {
        cout << "Warning: 'context' must be object.";
        return false;
    }
    cJSON *extensionPathNode = cJSON_GetObjectItem(contextNode, "extensionPath");
    if (!extensionPathNode) {
        cout << "Warning: if image transcoding is supported, the 'extensionPath' node cannot be empty.";
        return false;
    }
    if (!cJSON_IsString(extensionPathNode)) {
        cout << "Warning: 'extensionPath' must be string.";
        return false;
    }
    extensionPath_ = extensionPathNode->valuestring;
    if (extensionPath_.empty()) {
        cout << "Warning: 'extensionPath' value cannot be empty.";
        return false;
    }
    return true;
}

bool CompressionParser::ParseFilters(const cJSON *filtersNode)
{
    if (!filtersNode) {
        PrintError(GetError(ERR_CODE_JSON_NODE_MISSING).FormatCause("filters").SetPosition(filePath_));
        return false;
    }
    if (!cJSON_IsArray(filtersNode)) {
        PrintError(GetError(ERR_CODE_JSON_NODE_MISMATCH).FormatCause("filters", "array").SetPosition(filePath_));
        return false;
    }
    if (cJSON_GetArraySize(filtersNode) == 0) {
        PrintError(GetError(ERR_CODE_JSON_NODE_EMPTY).FormatCause("filters").SetPosition(filePath_));
        return false;
    }
    for (cJSON *item = filtersNode->child; item; item = item->next) {
        if (!cJSON_IsObject(item)) {
            PrintError(GetError(ERR_CODE_JSON_NODE_MISMATCH).FormatCause("filters's value", "object")
                .SetPosition(filePath_));
            return false;
        }
        cJSON *methodNode = cJSON_GetObjectItem(item, "method");
        if (!methodNode) {
            PrintError(GetError(ERR_CODE_JSON_NODE_MISSING).FormatCause("method").SetPosition(filePath_));
            return false;
        }
        if (!cJSON_IsObject(methodNode)) {
            PrintError(GetError(ERR_CODE_JSON_NODE_MISMATCH).FormatCause("method", "object").SetPosition(filePath_));
            return false;
        }
        shared_ptr<CompressFilter> compressFilter = make_shared<CompressFilter>();
        compressFilter->method = "\"method\":" + ParseJsonStr(methodNode);
        cJSON *pathNode = cJSON_GetObjectItem(item, "path");
        compressFilter->path = ParsePath(pathNode);
        cJSON *excludePathNode = cJSON_GetObjectItem(item, "exclude_path");
        compressFilter->excludePath = ParsePath(excludePathNode);
        cJSON *rulesNode = cJSON_GetObjectItem(item, "rules_origin");
        compressFilter->rules = ParseRules(rulesNode);
        cJSON *excludeRulesNode = cJSON_GetObjectItem(item, "rules_exclude");
        compressFilter->excludeRules = ParseRules(excludeRulesNode);
        compressFilters_.emplace_back(compressFilter);
    }
    defaultCompress_ = IsDefaultCompress();
    return true;
}

bool CompressionParser::IsDefaultCompress()
{
    if (compressFilters_.size() != 1) {
        return false;
    }
    auto compressFilter = compressFilters_[0];
    bool pathEmpty = (compressFilter->path.size() == 1) && (compressFilter->path[0] == "true");
    bool excludePathEmpty = (compressFilter->excludePath.size() == 1) && (compressFilter->excludePath[0] == "true");
    return pathEmpty && excludePathEmpty && (compressFilter->rules.empty()) && (compressFilter->excludeRules.empty());
}

void CompressionParser::SetOutPath(const string &path)
{
    outPath_ = path;
}

string CompressionParser::ParseRules(const cJSON *rulesNode)
{
    string res = "";
    if (!rulesNode || !cJSON_IsObject(rulesNode)) {
        cout << "Warning: rules is not exist or node type is wrong" << endl;
        return res;
    }
    for (cJSON *item = rulesNode->child; item; item = item->next) {
        if (!item || !cJSON_IsArray(item)) {
            continue;
        }
        string name(item->string);
        res.append("\"").append(name).append("\":").append(ParseJsonStr(item)).append(",");
    }
    if (res.size() - 1 < 0) {
        return res;
    }
    return res.substr(0, res.size() - 1);
}

vector<string> CompressionParser::ParsePath(const cJSON *pathNode)
{
    vector<string> res;
    if (!pathNode) {
        res.emplace_back("true");
        return res;
    }
    if (!cJSON_IsArray(pathNode)) {
        cout << "Warning: pathnode is not array." << endl;
        return res;
    }
    for (cJSON *item = pathNode->child; item; item = item->next) {
        if (!item || !cJSON_IsString(item)) {
            continue;
        }
        res.emplace_back(item->valuestring);
    }
    return res;
}

string CompressionParser::ParseJsonStr(const cJSON *node)
{
    if (!node) {
        return "";
    }
    char *jsonString = cJSON_Print(node);
    if (jsonString == nullptr) {
        return "";
    }
    string res(jsonString);
    cJSON_free(jsonString);
    jsonString = nullptr;
    return res;
}

bool CompressionParser::LoadImageTranscoder()
{
#ifdef __WIN32
    if (!handle_) {
        handle_ = LoadLibrary(TEXT(extensionPath_.c_str()));
        if (!handle_) {
            DWORD err = GetLastError();
            LPVOID lpMsgBuf;
            FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL, err, MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf, 0, NULL);
            const char *msg = static_cast<const char *>(lpMsgBuf);
            std::string errMsg = "(" + std::to_string(err) + ")" + string(msg);
            PrintError(GetError(ERR_CODE_LOAD_LIBRARY_FAIL).FormatCause(extensionPath_.c_str(), errMsg.c_str()));
            LocalFree(lpMsgBuf);
            return false;
        }
    }
#else
    if (!handle_) {
        string realPath = ResourceUtil::RealPath(extensionPath_);
        if (realPath.empty()) {
            PrintError(GetError(ERR_CODE_LOAD_LIBRARY_FAIL).FormatCause(extensionPath_.c_str(), "path invalid"));
            return false;
        }
        handle_ = dlopen(realPath.c_str(), RTLD_LAZY);
        if (!handle_) {
            char *err = dlerror();
            PrintError(GetError(ERR_CODE_LOAD_LIBRARY_FAIL).FormatCause(realPath.c_str(), err));
            return false;
        }
    }
#endif
    return true;
}

bool CompressionParser::SetTranscodeOptions(const string &optionJson, const string &optionJsonExclude)
{
    if (!handle_) {
        cout << "Warning: SetTranscodeOptions handle_ is nullptr." << endl;
        return false;
    }
#ifdef __WIN32
    ISetTranscodeOptions iSetTranscodeOptions = (ISetTranscodeOptions)GetProcAddress(handle_, "SetTranscodeOptions");
#else
    ISetTranscodeOptions iSetTranscodeOptions = (ISetTranscodeOptions)dlsym(handle_, "SetTranscodeOptions");
#endif
    if (!iSetTranscodeOptions) {
        cout << "Warning: Failed to get the 'SetTranscodeOptions'." << endl;
        return false;
    }
    bool ret = (*iSetTranscodeOptions)(optionJson, optionJsonExclude);
    if (!ret) {
        cout << "Warning: SetTranscodeOptions failed." << endl;
        return false;
    }
    return true;
}

TranscodeError CompressionParser::TranscodeImages(const string &imagePath, const bool extAppend,
    string &outputPath, TranscodeResult &result)
{
    if (!handle_) {
        cout << "Warning: TranscodeImages handle_ is nullptr." << endl;
        return TranscodeError::LOAD_COMPRESS_FAILED;
    }
#ifdef __WIN32
    ITranscodeImages iTranscodeImages = (ITranscodeImages)GetProcAddress(handle_, "Transcode");
#else
    ITranscodeImages iTranscodeImages = (ITranscodeImages)dlsym(handle_, "Transcode");
#endif
    if (!iTranscodeImages) {
        cout << "Warning: Failed to get the 'Transcode'." << endl;
        return TranscodeError::LOAD_COMPRESS_FAILED;
    }
    TranscodeError ret = (*iTranscodeImages)(imagePath, extAppend, outputPath, result);
    if (ret != TranscodeError::SUCCESS) {
        auto iter = ERRORCODEMAP.find(ret);
        if (iter != ERRORCODEMAP.end()) {
            cout << "Warning: TranscodeImages failed, error message: " << iter->second << ", file path = " <<
                imagePath << endl;
        } else {
            cout << "Warning: TranscodeImages failed" << ", file path = " << imagePath << endl;
        }
        return ret;
    }
    return TranscodeError::SUCCESS;
}

TranscodeError CompressionParser::ScaleImage(const std::string &imagePath, std::string &outputPath)
{
    if (!handle_) {
        cout << "Warning: ScaleImage handle_ is nullptr." << endl;
        return TranscodeError::LOAD_COMPRESS_FAILED;
    }
#ifdef __WIN32
    IScaleImage iScaleImage = (IScaleImage)GetProcAddress(handle_, "TranscodeSLR");
#else
    IScaleImage iScaleImage = (IScaleImage)dlsym(handle_, "TranscodeSLR");
#endif
    if (!iScaleImage) {
        cout << "Warning: Failed to get the 'TranscodeSLR'." << endl;
        return TranscodeError::LOAD_COMPRESS_FAILED;
    }
    TranscodeError ret = (*iScaleImage)(imagePath, outputPath, { 512, 512 });
    if (ret != TranscodeError::SUCCESS) {
        auto iter = ERRORCODEMAP.find(ret);
        if (iter != ERRORCODEMAP.end()) {
            cout << "Warning: ScaleImage failed, error message: " << iter->second << ", file path = " << imagePath
                 << endl;
        } else {
            cout << "Warning: ScaleImage failed" << ", file path = " << imagePath << endl;
        }
        return ret;
    }
    return TranscodeError::SUCCESS;
}

bool CompressionParser::CheckPath(const string &src, const vector<string> &paths)
{
    if (paths.size() == 1 && paths[0] == "true") {
        return true;
    }
    return any_of(paths.begin(), paths.end(), [src](const auto &iter) {
        return iter == src;
    });
}

bool CompressionParser::IsInPath(const string &src, const shared_ptr<CompressFilter> &compressFilter)
{
    return CheckPath(src, compressFilter->path);
}

bool CompressionParser::IsInExcludePath(const string &src, const shared_ptr<CompressFilter> &compressFilter)
{
    return CheckPath(src, compressFilter->excludePath);
}

string CompressionParser::GetMethod(const shared_ptr<CompressFilter> &compressFilter)
{
    return "{" + compressFilter->method + "}";
}

string CompressionParser::GetRules(const shared_ptr<CompressFilter> &compressFilter)
{
    return GetFileRules(compressFilter->rules, compressFilter->method);
}

string CompressionParser::GetExcludeRules(const shared_ptr<CompressFilter> &compressFilter)
{
    return GetFileRules(compressFilter->excludeRules, compressFilter->method);
}

string CompressionParser::GetFileRules(const string &rules, const string &method)
{
    if (rules.empty()) {
        return "{" + method + "}";
    }
    string res = "{";
    return res.append(method).append(",").append(rules).append("}");
}

void CompressionParser::CollectTime(uint32_t &count, unsigned long long &time,
    std::chrono::time_point<std::chrono::steady_clock> &start)
{
    unsigned long long costTime = static_cast<unsigned long long>(
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count());
    time += costTime;
    count++;
}

void CompressionParser::CollectTimeAndSize(TranscodeError res,
    std::chrono::time_point<std::chrono::steady_clock> &start, TranscodeResult &result)
{
    unsigned long long costTime = static_cast<unsigned long long>(
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count());
    if (res == TranscodeError::SUCCESS) {
        totalTime_ += costTime;
        totalCounts_++;
        compressTime_ += costTime;
        compressCounts_++;
        successTime_ += costTime;
        successCounts_++;
        originalSize_ += result.originSize;
        successSize_ += static_cast<unsigned long long>(result.size);
    } else if (res < TranscodeError::NOT_MATCH_BASE) {
        totalTime_ += costTime;
        compressTime_ += costTime;
        compressCounts_++;
    } else {
        totalTime_ += costTime;
    }
}

string CompressionParser::PrintTransMessage()
{
    string res = "Processing report:\n";
    res.append("total:").append(to_string(totalCounts_)).append(", ").append(to_string(totalTime_)).append(" us.\n");
    res.append("compressed:").append(to_string(compressCounts_)).append(", ").append(to_string(compressTime_))
        .append(" us.\n");
    res.append("success:").append(to_string(successCounts_)).append(", ").append(to_string(successTime_))
        .append(" us, ").append(to_string(originalSize_)).append(" Bytes to ").append(to_string(successSize_))
        .append(" Bytes.");
    return res;
}

bool CompressionParser::GetMediaSwitch()
{
    return mediaSwitch_;
}

bool CompressionParser::GetDefaultCompress()
{
    return defaultCompress_;
}

bool CompressionParser::CheckAndTranscode(const string &src, string &dst, string &output,
    const shared_ptr<CompressFilter> &compressFilter, const bool extAppend)
{
    auto t1 = std::chrono::steady_clock::now();
    TranscodeResult result = {0, 0, 0, 0};
    if (defaultCompress_) {
        if (!SetTranscodeOptions(GetMethod(compressFilter), "")) {
            return false;
        }
        auto res = TranscodeImages(src, extAppend, output, result);
        CollectTimeAndSize(res, t1, result);
        if (res != TranscodeError::SUCCESS) {
            return false;
        }
        dst = output;
        return true;
    }
    if (!IsInPath(src, compressFilter)) {
        return false;
    }
    if (IsInExcludePath(src, compressFilter)) {
        if (!SetTranscodeOptions(GetRules(compressFilter), GetExcludeRules(compressFilter))) {
            return false;
        }
        auto res = TranscodeImages(src, extAppend, output, result);
        CollectTimeAndSize(res, t1, result);
        if (res != TranscodeError::SUCCESS) {
            return false;
        }
        dst = output;
        return true;
    }
    if (!SetTranscodeOptions(GetRules(compressFilter), "")) {
        return false;
    }
    auto res = TranscodeImages(src, extAppend, output, result);
    CollectTimeAndSize(res, t1, result);
    if (res != TranscodeError::SUCCESS) {
        return false;
    }
    dst = output;
    return true;
}

bool CompressionParser::CopyForTrans(const string &src, const string &originDst, const string &dst)
{
    FileEntry::FilePath srcFile(src);
    FileEntry::FilePath dstFile(dst);
    string srcSuffix = srcFile.GetExtension();
    string dstSuffix = dstFile.GetExtension();
    if (srcSuffix.empty() || dstSuffix.empty()) {
        srcSuffix.clear();
        dstSuffix.clear();
    }
    auto ret = false;
    if (srcSuffix == dstSuffix) {
        ret = ResourceUtil::CopyFileInner(src, dst);
    } else {
        uint32_t startIndex = outPath_.size() + CACHES_DIR.size() + 1;
        string dstPath = outPath_ + SEPARATOR_FILE + RESOURCES_DIR + dst.substr(startIndex);
        ret = ResourceUtil::CopyFileInner(dst, dstPath);
    }
    return ret;
}

bool CompressionParser::CopyAndTranscode(const string &src, string &dst, const bool extAppend)
{
    auto t0 = std::chrono::steady_clock::now();
    if (!mediaSwitch_) {
        auto res = ResourceUtil::CopyFileInner(src, dst);
        CollectTime(totalCounts_, totalTime_, t0);
        return res;
    }

    auto index = dst.find_last_of(SEPARATOR_FILE);
    if (index == string::npos) {
        PrintError(GetError(ERR_CODE_INVALID_RESOURCE_PATH).FormatCause(dst.c_str(), "missing separator"));
        return false;
    }
    uint32_t startIndex = outPath_.size() + RESOURCES_DIR.size() + 1;
    string endStr = dst.substr(startIndex, index - startIndex);
    string output = outPath_ + SEPARATOR_FILE + CACHES_DIR + endStr;
    string originDst = dst;
    if (!ResourceUtil::CreateDirs(output)) {
        return false;
    }
    for (const auto &compressFilter : compressFilters_) {
        if (!CheckAndTranscode(src, dst, output, compressFilter, extAppend)) {
            continue;
        }
        break;
    }
    auto t2 = std::chrono::steady_clock::now();
    auto ret = CopyForTrans(src, originDst, dst);
    CollectTime(totalCounts_, totalTime_, t2);
    return ret;
}

bool CompressionParser::CheckAndScaleIcon(const std::string &src, const std::string &originDst, std::string &scaleDst)
{
    scaleDst = src;
    if (filePath_.empty() || outPath_.empty()) {
        cout << "Info: compression path or out path is empty, unable to scale icon." << endl;
        return true;
    }
    auto index = originDst.find_last_of(SEPARATOR_FILE);
    if (index == string::npos) {
        PrintError(GetError(ERR_CODE_INVALID_RESOURCE_PATH).FormatCause(originDst.c_str(), "missing separator"));
        return false;
    }
    uint32_t startIndex = outPath_.size() + RESOURCES_DIR.size() + 1;
    string qualifierDir = originDst.substr(startIndex, index - startIndex);
    string outputCache = outPath_ + SEPARATOR_FILE + CACHES_DIR + qualifierDir;
    if (!ResourceUtil::CreateDirs(outputCache)) {
        return false;
    }
    string fileName = originDst.substr(index + 1);
    string outputFile = outputCache + SEPARATOR_FILE + fileName;
    auto ret = ScaleImage(src, outputFile);
    if (ret == TranscodeError::SUCCESS) {
        // if scale success, change src file to scale image
        scaleDst = outputFile;
    }
    return true;
}

bool CompressionParser::ScaleIconEnable()
{
    return !filePath_.empty() && !outPath_.empty() && handle_ != nullptr;
}
}
}
}
