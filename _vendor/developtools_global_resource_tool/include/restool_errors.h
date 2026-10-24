/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#ifndef OHOS_RESTOOL_ERRORS_H
#define OHOS_RESTOOL_ERRORS_H

#include <iostream>
#include <memory>
#include <securec.h>
#include <stdint.h>

#include "resource_data.h"

namespace OHOS {
namespace Global {
namespace Restool {
constexpr uint32_t RESTOOL_SUCCESS = 0;
constexpr uint32_t RESTOOL_ERROR = -1;
constexpr uint16_t BUFFER_SIZE = 4096;
constexpr uint16_t BUFFER_SIZE_SMALL = 128;
const std::string ERROR_MORE_INFO_FILE = "restool_faq.json";
// 11200xxx unknown error
constexpr uint32_t ERR_CODE_UNDEFINED_ERROR = 11200000;

// 11201xxx dependency error
const std::string ERR_TYPE_DEPENDENCY = "Dependency Error";
constexpr uint32_t ERR_CODE_LOAD_LIBRARY_FAIL = 11201001;
// 11202xxx script error

// 11203xxx config error
const std::string ERR_TYPE_CONFIG = "Config Error";
constexpr uint32_t ERR_CODE_OPEN_JSON_FAIL = 11203001;
constexpr uint32_t ERR_CODE_JSON_FORMAT_ERROR = 11203002;
constexpr uint32_t ERR_CODE_JSON_NODE_MISMATCH = 11203003;
constexpr uint32_t ERR_CODE_JSON_NODE_MISSING = 11203004;
constexpr uint32_t ERR_CODE_JSON_NODE_EMPTY = 11203005;
constexpr uint32_t ERR_CODE_JSON_NOT_ONE_MEMBER = 11203006;
constexpr uint32_t ERR_CODE_JSON_INVALID_NODE_NAME = 11203007;

// 11204xxx file resource error
const std::string ERR_TYPE_FILE_RESOURCE = "File Resource Error";
constexpr uint32_t ERR_CODE_CREATE_FILE_ERROR = 11204001;
constexpr uint32_t ERR_CODE_REMOVE_FILE_ERROR = 11204003;
constexpr uint32_t ERR_CODE_COPY_FILE_ERROR = 11204004;
constexpr uint32_t ERR_CODE_OPEN_FILE_ERROR = 11204005;
constexpr uint32_t ERR_CODE_READ_FILE_ERROR = 11204006;

// 11210xxx parse command error
const std::string ERR_TYPE_COMMAND_PARSE = "Command Parse Error";
constexpr uint32_t ERR_CODE_UNKNOWN_COMMAND_ERROR = 11210000;
constexpr uint32_t ERR_CODE_UNKNOWN_OPTION = 11210001;
constexpr uint32_t ERR_CODE_MISSING_ARGUMENT = 11210002;
constexpr uint32_t ERR_CODE_INVALID_ARGUMENT = 11210003;
constexpr uint32_t ERR_CODE_INVALID_INPUT = 11210004;
constexpr uint32_t ERR_CODE_DUPLICATE_INPUT = 11210005;
constexpr uint32_t ERR_CODE_DOUBLE_PACKAGE_NAME = 11210006;
constexpr uint32_t ERR_CODE_INVALID_OUTPUT = 11210007;
constexpr uint32_t ERR_CODE_DOUBLE_OUTPUT = 11210008;
constexpr uint32_t ERR_CODE_DUPLICATE_RES_HEADER = 11210009;
constexpr uint32_t ERR_CODE_DOUBLE_MODULES = 11210010;
constexpr uint32_t ERR_CODE_DUPLICATE_MODULE_NAME = 11210011;
constexpr uint32_t ERR_CODE_DOUBLE_CONFIG_JSON = 11210012;
constexpr uint32_t ERR_CODE_INVALID_START_ID = 11210013;
constexpr uint32_t ERR_CODE_DUPLICATE_APPEND_PATH = 11210014;
constexpr uint32_t ERR_CODE_DOUBLE_TARGET_CONFIG = 11210015;
constexpr uint32_t ERR_CODE_INVALID_TARGET_CONFIG = 11210016;
constexpr uint32_t ERR_CODE_INVALID_SYSTEM_ID_DEFINED = 11210017;
constexpr uint32_t ERR_CODE_DUPLICATE_SYSTEM_ID_DEFINED = 11210018;
constexpr uint32_t ERR_CODE_DOUBLE_COMPRESSION_PATH = 11210019;
constexpr uint32_t ERR_CODE_NON_ASCII = 11210020;
constexpr uint32_t ERR_CODE_EXCLUSIVE_OPTION = 11210021;
constexpr uint32_t ERR_CODE_PACKAGE_NAME_EMPTY = 11210022;
constexpr uint32_t ERR_CODE_RES_HEADER_PATH_EMPTY = 11210023;
constexpr uint32_t ERR_CODE_DUMP_MISSING_INPUT = 11210024;
constexpr uint32_t ERR_CODE_DUMP_INVALID_INPUT = 11210025;
constexpr uint32_t ERR_CODE_INVALID_THREAD_COUNT = 11210026;
constexpr uint32_t ERR_CODE_INVALID_IGNORE_FILE = 11210027;

// 11211xxx resource pack error
const std::string ERR_TYPE_RESOURCE_PACK = "Resource Pack Error";
constexpr uint32_t ERR_CODE_OUTPUT_EXIST = 11211001;
constexpr uint32_t ERR_CODE_CONFIG_JSON_MISSING = 11211002;
constexpr uint32_t ERR_CODE_INVALID_MODULE_TYPE = 11211003;
constexpr uint32_t ERR_CODE_EXCLUSIVE_START_ID = 11211004;
constexpr uint32_t ERR_CODE_ID_DEFINED_INVALID_TYPE = 11211007;
constexpr uint32_t ERR_CODE_ID_DEFINED_INVALID_ID = 11211008;
constexpr uint32_t ERR_CODE_ID_DEFINED_ORDER_MISMATCH = 11211010;
constexpr uint32_t ERR_CODE_ID_DEFINED_SAME_ID = 11211012;
constexpr uint32_t ERR_CODE_MODULE_NAME_NOT_FOUND = 11211014;

constexpr uint32_t ERR_CODE_INVALID_RESOURCE_PATH = 11211101;
constexpr uint32_t ERR_CODE_INVALID_LIMIT_KEY = 11211103;
constexpr uint32_t ERR_CODE_INVALID_RESOURCE_DIR = 11211104;
constexpr uint32_t ERR_CODE_INVALID_TRANSLATE_PRIORITY = 11211106;
constexpr uint32_t ERR_CODE_INVALID_ELEMENT_TYPE = 11211107;
constexpr uint32_t ERR_CODE_INVALID_COLOR_VALUE = 11211108;
constexpr uint32_t ERR_CODE_INVALID_RESOURCE_REF = 11211109;
constexpr uint32_t ERR_CODE_PARENT_EMPTY = 11211110;
constexpr uint32_t ERR_CODE_ARRAY_TOO_LARGE = 11211111;
constexpr uint32_t ERR_CODE_INVALID_QUANTITY = 11211112;
constexpr uint32_t ERR_CODE_DUPLICATE_QUANTITY = 11211113;
constexpr uint32_t ERR_CODE_QUANTITY_NO_OTHER = 11211114;
constexpr uint32_t ERR_CODE_INVALID_SYMBOL = 11211115;
constexpr uint32_t ERR_CODE_INVALID_RESOURCE_NAME = 11211116;
constexpr uint32_t ERR_CODE_RESOURCE_DUPLICATE = 11211117;
constexpr uint32_t ERR_CODE_RESOURCE_ID_EXCEED = 11211118;
constexpr uint32_t ERR_CODE_RESOURCE_ID_NOT_DEFINED = 11211119;
constexpr uint32_t ERR_CODE_REF_NOT_DEFINED = 11211120;
constexpr uint32_t ERR_CODE_INVALID_RESOURCE_INDEX = 11211124;

// 11212xxx resource dump error
const std::string ERR_TYPE_RESOURCE_DUMP = "Resource Dump Error";
constexpr uint32_t ERR_CODE_PARSE_HAP_ERROR = 11212001;

enum class Language {
    CN,
    EN
};

struct MoreInfo {
    std::string cn;
    std::string en;
};

struct ExtSolution {
    std::string fileName;
    std::string solution;
};

struct FaqInfo {
    std::string cn;
    std::string en;
    std::vector<ExtSolution> extSolutions;
};

class ErrorInfo {
public:
    uint32_t code_;
    std::string description_;
    std::string cause_;
    std::string position_;
    std::vector<std::string> solutions_;
    MoreInfo moreInfo_;

    template <class... Args>
    ErrorInfo &FormatDescription(Args... args)
    {
        description_ = FormatString(description_, std::forward<Args>(args)...);
        return *this;
    }

    template <class... Args>
    ErrorInfo &FormatCause(Args... args)
    {
        cause_ = FormatString(cause_, std::forward<Args>(args)...);
        return *this;
    }

    template <class... Args>
    ErrorInfo &FormatSolution(size_t index, Args... args)
    {
        if (index < solutions_.size()) {
            std::string &solution = solutions_[index];
            solution = FormatString(solution, std::forward<Args>(args)...);
        }
        return *this;
    }

    ErrorInfo &SetPosition(const std::string &position)
    {
        position_ = position;
        return *this;
    }

private:
    template <class... Args>
    std::string FormatString(const std::string &fmt, Args... args)
    {
        if (fmt.empty()) {
            return fmt;
        }
        size_t paramCount = sizeof...(args);
        if (paramCount == 0) {
            return fmt;
        }
        std::unique_ptr<char[]> buf = std::make_unique<char[]>(BUFFER_SIZE);
        auto realSize = snprintf_s(buf.get(), BUFFER_SIZE, BUFFER_SIZE - 1, fmt.c_str(), args...);
        if (realSize == -1) {
            return fmt;
        }
        return std::string(buf.get(), buf.get() + realSize);
    }
};

void InitFaq(const std::string &restoolPath);
ErrorInfo GetError(const uint32_t &errCode);
void PrintError(const uint32_t &errCode);
void PrintError(const ErrorInfo &error);
}
}
}
#endif