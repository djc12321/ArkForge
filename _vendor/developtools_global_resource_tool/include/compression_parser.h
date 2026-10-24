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

#ifndef OHOS_RESTOOL_COMPRESSION_PARSER_H
#define OHOS_RESTOOL_COMPRESSION_PARSER_H

#include <chrono>
#include <cJSON.h>
#ifdef __WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#include "resource_util.h"

namespace OHOS {
namespace Global {
namespace Restool {

enum class TranscodeError {
    SUCCESS = 0,
    INVALID_PARAMETERS,
    IMAGE_ERROR,
    ANIMATED_IMAGE_SKIP,
    MALLOC_FAILED,
    ENCODE_ASTC_FAILED,
    SUPER_COMPRESS_FAILED,
    NOT_MATCH_BASE = 100,
    IMAGE_SIZE_NOT_MATCH = 100,
    IMAGE_RESOLUTION_NOT_MATCH,
    EXCLUDE_MATCH,
    NOT_MATCH_BUTT = 199,
    LOAD_COMPRESS_FAILED
};

struct TranscodeResult {
    size_t originSize;
    int32_t size;
    int32_t width;
    int32_t height;
};

struct ImageSize {
    size_t width;
    size_t height;
};

typedef TranscodeError (*ITranscodeImages) (const std::string &imagePath, const bool extAppend,
    std::string &outputPath, TranscodeResult &result);
typedef bool (*ISetTranscodeOptions) (const std::string &optionJson, const std::string &optionJsonExclude);
typedef TranscodeError (*IScaleImage) (const std::string &imagePath, std::string &outputPath, ImageSize size);

class CompressionParser {
public:
    static std::shared_ptr<CompressionParser> GetCompressionParser(const std::string &filePath);
    static std::shared_ptr<CompressionParser> GetCompressionParser();
    CompressionParser();
    explicit CompressionParser(const std::string &filePath);
    virtual ~CompressionParser();
    uint32_t Init();
    bool CopyAndTranscode(const std::string &src, std::string &dst, const bool extAppend = false);
    bool GetMediaSwitch();
    std::string PrintTransMessage();
    bool GetDefaultCompress();
    void SetOutPath(const std::string &path);
    bool ScaleIconEnable();
    bool CheckAndScaleIcon(const std::string &src, const std::string &originDst, std::string &scaleDst);
private:
    bool ParseContext(const cJSON *contextNode);
    bool ParseCompression(const cJSON *compressionNode);
    bool ParseFilters(const cJSON *filtersNode);
    bool LoadImageTranscoder();
    bool SetTranscodeOptions(const std::string &optionJson, const std::string &optionJsonExclude);
    TranscodeError TranscodeImages(const std::string &imagePath, const bool extAppend,
        std::string &outputPath, TranscodeResult &result);
    TranscodeError ScaleImage(const std::string &imagePath, std::string &outputPath);
    std::vector<std::string> ParsePath(const cJSON *pathNode);
    std::string ParseRules(const cJSON *rulesNode);
    std::string ParseJsonStr(const cJSON *node);
    bool CheckPath(const std::string &src, const std::vector<std::string> &paths);
    bool IsInPath(const std::string &src, const std::shared_ptr<CompressFilter> &compressFilter);
    bool IsInExcludePath(const std::string &src, const std::shared_ptr<CompressFilter> &compressFilter);
    void CollectTime(uint32_t &count, unsigned long long &time,
        std::chrono::time_point<std::chrono::steady_clock> &start);
    void CollectTimeAndSize(TranscodeError res, std::chrono::time_point<std::chrono::steady_clock> &start,
        TranscodeResult &result);
    std::string GetMethod(const std::shared_ptr<CompressFilter> &compressFilter);
    std::string GetRules(const std::shared_ptr<CompressFilter> &compressFilter);
    std::string GetExcludeRules(const std::shared_ptr<CompressFilter> &compressFilter);
    std::string GetFileRules(const std::string &rules, const std::string &method);
    bool CheckAndTranscode(const std::string &src, std::string &dst, std::string &output,
        const std::shared_ptr<CompressFilter> &compressFilter, const bool extAppend);
    bool CopyForTrans(const std::string &src, const std::string &originDst, const std::string &dst);
    bool IsDefaultCompress();
    std::string filePath_;
    std::string extensionPath_;
    std::vector<std::shared_ptr<CompressFilter>> compressFilters_;
    bool mediaSwitch_;
    cJSON *root_;
    bool defaultCompress_;
    std::string outPath_;
    unsigned long long totalTime_ = 0;
    uint32_t totalCounts_ = 0;
    unsigned long long compressTime_ = 0;
    uint32_t compressCounts_ = 0;
    unsigned long long successTime_ = 0;
    uint32_t successCounts_ = 0;
    unsigned long long originalSize_ = 0;
    unsigned long long successSize_ = 0;
#ifdef __WIN32
    HMODULE handle_ = nullptr;
#else
    void *handle_ = nullptr;
#endif
};
}
}
}
#endif