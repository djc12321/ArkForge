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

#include "cmd/package_parser.h"

#include <algorithm>
#include <climits>
#include <regex>
#include <sstream>

#include "file_entry.h"
#include "resconfig_parser.h"
#include "resource_pack.h"
#include "resource_util.h"
#include "select_compile_parse.h"

namespace OHOS {
namespace Global {
namespace Restool {
using namespace std;
const struct option PackageParser::CMD_OPTS[] = {
    { "inputPath", required_argument, nullptr, Option::INPUTPATH },
    { "packageName", required_argument, nullptr, Option::PACKAGENAME },
    { "outputPath", required_argument, nullptr, Option::OUTPUTPATH },
    { "resHeader", required_argument, nullptr, Option::RESHEADER },
    { "forceWrite", no_argument, nullptr, Option::FORCEWRITE },
    { "version", no_argument, nullptr, Option::VERSION},
    { "modules", required_argument, nullptr, Option::MODULES },
    { "json", required_argument, nullptr, Option::JSON },
    { "startId", required_argument, nullptr, Option::STARTID },
    { "fileList", required_argument, nullptr, Option::FILELIST },
    { "append", required_argument, nullptr, Option::APPEND },
    { "combine", required_argument, nullptr, Option::COMBINE },
    { "dependEntry", required_argument, nullptr, Option::DEPENDENTRY },
    { "help", no_argument, nullptr, Option::HELP},
    { "ids", required_argument, nullptr, Option::IDS},
    { "defined-ids", required_argument, nullptr, Option::DEFINED_IDS},
    { "icon-check", no_argument, nullptr, Option::ICON_CHECK},
    { "target-config", required_argument, nullptr, Option::TARGET_CONFIG},
    { "defined-sysids", required_argument, nullptr, Option::DEFINED_SYSIDS},
    { "compressed-config", required_argument, nullptr, Option::COMPRESSED_CONFIG},
    { "thread", required_argument, nullptr, Option::THREAD},
    { "ignored-file", required_argument, nullptr, Option::IGNORED_FILE},
    { "ignored-path", required_argument, nullptr, Option::IGNORED_PATH},
    { 0, 0, 0, 0},
};

const string PackageParser::CMD_PARAMS = ":i:p:o:r:m:j:e:l:x:fhvz";

uint32_t PackageParser::Parse(int argc, char *argv[])
{
    InitCommand();
    if (ParseCommand(argc, argv) != RESTOOL_SUCCESS) {
        return RESTOOL_ERROR;
    }
    if (CheckParam() != RESTOOL_SUCCESS) {
        return RESTOOL_ERROR;
    }
    AdaptResourcesDirForInput();
    return RESTOOL_SUCCESS;
}

const vector<string> &PackageParser::GetInputs() const
{
    return inputs_;
}

const string &PackageParser::GetPackageName() const
{
    return packageName_;
}

const string &PackageParser::GetOutput() const
{
    return output_;
}

const vector<string> &PackageParser::GetResourceHeaders() const
{
    return resourceHeaderPaths_;
}

bool PackageParser::GetForceWrite() const
{
    return forceWrite_;
}

const vector<string> &PackageParser::GetModuleNames() const
{
    return moduleNames_;
}

const string &PackageParser::GetConfig() const
{
    return configPath_;
}

const string &PackageParser::GetRestoolPath() const
{
    return restoolPath_;
}

uint32_t PackageParser::GetStartId() const
{
    return startId_;
}

const string &PackageParser::GetDependEntry() const
{
    return dependEntry_;
}

const vector<std::string> &PackageParser::GetSysIdDefinedPaths() const
{
    return sysIdDefinedPaths_;
}

uint32_t PackageParser::AddInput(const string& argValue)
{
    string inputPath = ResourceUtil::RealPath(argValue);
    if (inputPath.empty()) {
        PrintError(GetError(ERR_CODE_INVALID_INPUT).FormatCause(argValue.c_str()));
        return RESTOOL_ERROR;
    }

    auto ret = find_if(inputs_.begin(), inputs_.end(), [inputPath](auto iter) {return inputPath == iter;});
    if (ret != inputs_.end()) {
        PrintError(GetError(ERR_CODE_DUPLICATE_INPUT).FormatCause(argValue.c_str()));
        return RESTOOL_ERROR;
    }

    if (!IsAscii(inputPath)) {
        return RESTOOL_ERROR;
    }

    if (IsOverlapInput(inputPath)) {
        inputs_.emplace(inputs_.begin(), inputPath);
    } else {
        inputs_.push_back(inputPath);
    }
    return RESTOOL_SUCCESS;
}

bool PackageParser::IsOverlapInput(const string& inputPath)
{
    if (isOverlap_) {
        return false;
    }

    string indexPath = FileEntry::FilePath(inputPath).Append(RESOURCE_INDEX_FILE).GetPath();
    if (ResourceUtil::FileExist(indexPath)) {
        isOverlap_ = true;
        return true;
    }

    string jsonIndexPath = ResourceUtil::GetMainPath(inputPath).Append(RESOURCE_INDEX_FILE).GetPath();
    string txtResHeaderPath = ResourceUtil::GetMainPath(inputPath).Append("ResourceTable.txt").GetPath();
    string jsResHeaderPath = ResourceUtil::GetMainPath(inputPath).Append("ResourceTable.js").GetPath();
    string hResJsHeaderPath = ResourceUtil::GetMainPath(inputPath).Append("ResourceTable.h").GetPath();
    if (ResourceUtil::FileExist(jsonIndexPath) &&
        !ResourceUtil::FileExist(txtResHeaderPath) &&
        !ResourceUtil::FileExist(jsResHeaderPath) &&
        !ResourceUtil::FileExist(hResJsHeaderPath)) {
        isOverlap_ = true;
        return true;
    }

    return false;
}

uint32_t PackageParser::AddSysIdDefined(const std::string& argValue)
{
    string sysIdDefinedPath = ResourceUtil::RealPath(argValue);
    if (sysIdDefinedPath.empty()) {
        PrintError(GetError(ERR_CODE_INVALID_SYSTEM_ID_DEFINED).FormatCause(argValue.c_str()));
        return RESTOOL_ERROR;
    }

    auto ret = find_if(sysIdDefinedPaths_.begin(), sysIdDefinedPaths_.end(),
        [sysIdDefinedPath](auto iter) {return sysIdDefinedPath == iter;});
    if (ret != sysIdDefinedPaths_.end()) {
        PrintError(GetError(ERR_CODE_DUPLICATE_SYSTEM_ID_DEFINED).FormatCause(argValue.c_str()));
        return RESTOOL_ERROR;
    }

    if (!IsAscii(sysIdDefinedPath)) {
        return RESTOOL_ERROR;
    }
    sysIdDefinedPaths_.push_back(sysIdDefinedPath);
    return RESTOOL_SUCCESS;
}

uint32_t PackageParser::AddPackageName(const string& argValue)
{
    if (!packageName_.empty()) {
        PrintError(GetError(ERR_CODE_DOUBLE_PACKAGE_NAME).FormatCause(packageName_.c_str(), argValue.c_str()));
        return RESTOOL_ERROR;
    }

    packageName_ = argValue;
    return RESTOOL_SUCCESS;
}

uint32_t PackageParser::AddOutput(const string& argValue)
{
    if (!output_.empty()) {
        PrintError(GetError(ERR_CODE_DOUBLE_OUTPUT).FormatCause(output_.c_str(), argValue.c_str()));
        return RESTOOL_ERROR;
    }

    output_ = ResourceUtil::RealPath(argValue);
    if (output_.empty()) {
        PrintError(GetError(ERR_CODE_INVALID_OUTPUT).FormatCause(argValue.c_str()));
        return RESTOOL_ERROR;
    }
    if (!IsAscii(output_)) {
        return RESTOOL_ERROR;
    }
    return RESTOOL_SUCCESS;
}

uint32_t PackageParser::AddResourceHeader(const string &argValue)
{
    if (find(resourceHeaderPaths_.begin(), resourceHeaderPaths_.end(), argValue) != resourceHeaderPaths_.end()) {
        PrintError(GetError(ERR_CODE_DUPLICATE_RES_HEADER).FormatCause(argValue.c_str()));
        return RESTOOL_ERROR;
    }
    resourceHeaderPaths_.push_back(argValue);
    return RESTOOL_SUCCESS;
}

uint32_t PackageParser::ForceWrite()
{
    forceWrite_ = true;
    return RESTOOL_SUCCESS;
}

uint32_t PackageParser::PrintVersion()
{
    std::string restoolVersion = RESTOOLV2_NAME + RESTOOL_VERSION;
    cout << "Info: Restool version = " << restoolVersion << endl;
    exit(RESTOOL_SUCCESS);
    return RESTOOL_SUCCESS;
}

uint32_t PackageParser::AddMoudleNames(const string& argValue)
{
    if (!moduleNames_.empty()) {
        std::string existModuleNames;
        for (const auto &module : moduleNames_) { existModuleNames.append(module).append(","); }
        existModuleNames.pop_back();
        PrintError(GetError(ERR_CODE_DOUBLE_MODULES).FormatCause(existModuleNames.c_str(), argValue.c_str()));
        return RESTOOL_ERROR;
    }

    ResourceUtil::Split(argValue, moduleNames_, ",");
    for (auto it = moduleNames_.begin(); it != moduleNames_.end(); it++) {
        auto ret = find_if(moduleNames_.begin(), moduleNames_.end(), [it](auto iter) {return *it == iter;});
        if (ret != it) {
            PrintError(GetError(ERR_CODE_DUPLICATE_MODULE_NAME).FormatCause(it->c_str()));
            return RESTOOL_ERROR;
        }
    }
    return RESTOOL_SUCCESS;
}

uint32_t PackageParser::AddConfig(const string& argValue)
{
    if (!configPath_.empty()) {
        PrintError(GetError(ERR_CODE_DOUBLE_CONFIG_JSON).FormatCause(configPath_.c_str(), argValue.c_str()));
        return RESTOOL_ERROR;
    }

    configPath_ = argValue;
    return RESTOOL_SUCCESS;
}

uint32_t PackageParser::AddStartId(const string& argValue)
{
    char *end;
    errno = 0;
    long long id = static_cast<long long>(strtoll(argValue.c_str(), &end, 16)); // 16 is hexadecimal number
    if (end == argValue.c_str() || errno == ERANGE || *end != '\0' || id == LLONG_MAX || id == LLONG_MIN) {
        PrintError(GetError(ERR_CODE_INVALID_START_ID).FormatCause(argValue.c_str()));
        return RESTOOL_ERROR;
    }
    if (id <= 0) {
        PrintError(GetError(ERR_CODE_INVALID_START_ID).FormatCause(argValue.c_str()));
        return RESTOOL_ERROR;
    }
    if ((id >= 0x01000000 && id < 0x06ffffff) || (id >= 0x08000000 && id < 0xffffffff)) {
        startId_ = static_cast<uint32_t>(id);
        return RESTOOL_SUCCESS;
    }
    PrintError(GetError(ERR_CODE_INVALID_START_ID).FormatCause(argValue.c_str()));
    return RESTOOL_ERROR;
}

// -i input directory, add the resource directory
void PackageParser::AdaptResourcesDirForInput()
{
    if (!isFileList_ && !combine_) { // -l and increment compile -i, no need to add resource directory
        for (auto &path : inputs_) {
            path = FileEntry::FilePath(path).Append(RESOURCES_DIR).GetPath();
        }
    }
}

uint32_t PackageParser::CheckParam() const
{
    if (inputs_.empty() && append_.empty()) {
        PrintError(GetError(ERR_CODE_INVALID_INPUT).FormatCause(""));
        return RESTOOL_ERROR;
    }

    if (output_.empty()) {
        PrintError(GetError(ERR_CODE_INVALID_OUTPUT).FormatCause(""));
        return RESTOOL_ERROR;
    }

    if (SelectCompileParse::HasTargetConfig() && !append_.empty()) {
        PrintError(GetError(ERR_CODE_EXCLUSIVE_OPTION).FormatCause("-x", "--target-config", "cannot be used together"));
        return RESTOOL_ERROR;
    }

    if (!append_.empty()) {
        return RESTOOL_SUCCESS;
    }

    if (packageName_.empty()) {
        PrintError(ERR_CODE_PACKAGE_NAME_EMPTY);
        return RESTOOL_ERROR;
    }

    if (resourceHeaderPaths_.empty()) {
        PrintError(ERR_CODE_RES_HEADER_PATH_EMPTY);
        return RESTOOL_ERROR;
    }

    if (startId_ != 0 && !idDefinedInputPath_.empty()) {
        PrintError(GetError(ERR_CODE_EXCLUSIVE_OPTION).FormatCause("-e", "--defined-ids", "cannot be used together"));
        return RESTOOL_ERROR;
    }

    return RESTOOL_SUCCESS;
}

bool PackageParser::IsFileList() const
{
    return isFileList_;
}

uint32_t PackageParser::AddAppend(const string& argValue)
{
    string appendPath = ResourceUtil::RealPath(argValue);
    if (appendPath.empty()) {
        cout << "Warning: invalid compress '" << argValue << "'" << endl;
        appendPath = argValue;
    }
    auto ret = find_if(append_.begin(), append_.end(), [appendPath](auto iter) {return appendPath == iter;});
    if (ret != append_.end()) {
        PrintError(GetError(ERR_CODE_DUPLICATE_APPEND_PATH).FormatCause(argValue.c_str()));
        return RESTOOL_ERROR;
    }
    if (!IsAscii(appendPath)) {
        return RESTOOL_ERROR;
    }
    append_.push_back(appendPath);
    return RESTOOL_SUCCESS;
}

const vector<string> &PackageParser::GetAppend() const
{
    return append_;
}

uint32_t PackageParser::SetCombine()
{
    combine_ = true;
    return RESTOOL_SUCCESS;
}

bool PackageParser::GetCombine() const
{
    return combine_;
}

uint32_t PackageParser::AddDependEntry(const string& argValue)
{
    dependEntry_ = argValue;
    return RESTOOL_SUCCESS;
}

uint32_t PackageParser::ShowHelp() const
{
    auto &parser = CmdParser::GetInstance();
    parser.ShowUseage();
    exit(RESTOOL_SUCCESS);
    return RESTOOL_SUCCESS;
}

uint32_t PackageParser::SetIdDefinedOutput(const string& argValue)
{
    idDefinedOutput_ = argValue;
    return RESTOOL_SUCCESS;
}

const string &PackageParser::GetIdDefinedOutput() const
{
    return idDefinedOutput_;
}

uint32_t PackageParser::SetIdDefinedInputPath(const string& argValue)
{
    idDefinedInputPath_ = argValue;
    return RESTOOL_SUCCESS;
}

const string &PackageParser::GetIdDefinedInputPath() const
{
    return idDefinedInputPath_;
}

uint32_t PackageParser::IconCheck()
{
    isIconCheck_ = true;
    return RESTOOL_SUCCESS;
}

bool PackageParser::GetIconCheck() const
{
    return isIconCheck_;
}

uint32_t PackageParser::ParseTargetConfig(const string &argValue)
{
    return SelectCompileParse::ParseTargetConfig(argValue, "--target-config");
}

uint32_t PackageParser::ParseThread(const std::string &argValue)
{
    if (argValue.empty()) {
        return RESTOOL_SUCCESS;
    }
    char *end;
    errno = 0;
    int count = static_cast<int>(strtol(argValue.c_str(), &end, 10));
    if (end == argValue.c_str() || errno == ERANGE || *end != '\0' || count == INT_MIN || count == INT_MAX) {
        PrintError(GetError(ERR_CODE_INVALID_THREAD_COUNT).FormatCause(argValue.c_str()));
        return RESTOOL_ERROR;
    }
    if (count <= 0) {
        PrintError(GetError(ERR_CODE_INVALID_THREAD_COUNT).FormatCause(argValue.c_str()));
        return RESTOOL_ERROR;
    }
    threadCount_ = static_cast<size_t>(count);
    return RESTOOL_SUCCESS;
}

uint32_t PackageParser::ParseIgnoreRegex(const std::string &argValue, const std::string &option)
{
    if (!ResourceUtil::CheckIgnoreOption(option)) {
        return RESTOOL_ERROR;
    }
    std::stringstream in(argValue);
    std::string regex;
    ResourceUtil::SetIgnoreOption(option);
    while (getline(in, regex, ':')) {
        bool isSucceed = ResourceUtil::AddIgnoreRegex(regex, IgnoreType::IGNORE_ALL, option);
        if (!isSucceed) {
            return RESTOOL_ERROR;
        }
    }
    return RESTOOL_SUCCESS;
}

size_t PackageParser::GetThreadCount() const
{
    return threadCount_;
}

bool PackageParser::IsAscii(const string& argValue) const
{
#ifdef __WIN32
    auto result = find_if(argValue.begin(), argValue.end(), [](auto iter) {
        if ((iter & 0x80) != 0) {
            return true;
        }
        return false;
    });
    if (result != argValue.end()) {
        PrintError(GetError(ERR_CODE_NON_ASCII).FormatCause(argValue.c_str()));
        return false;
    }
#endif
    return true;
}

uint32_t PackageParser::AddCompressionPath(const std::string& argValue)
{
    if (!compressionPath_.empty()) {
        PrintError(GetError(ERR_CODE_DOUBLE_COMPRESSION_PATH).FormatCause(compressionPath_.c_str(), argValue.c_str()));
        return RESTOOL_ERROR;
    }
    compressionPath_ = argValue;
    return RESTOOL_SUCCESS;
}

const std::string &PackageParser::GetCompressionPath() const
{
    return compressionPath_;
}

bool PackageParser::IsOverlap() const
{
    return isOverlap_;
}

void PackageParser::InitCommand()
{
    using namespace placeholders;
    handles_.emplace(Option::INPUTPATH, bind(&PackageParser::AddInput, this, _1));
    handles_.emplace(Option::PACKAGENAME, bind(&PackageParser::AddPackageName, this, _1));
    handles_.emplace(Option::OUTPUTPATH, bind(&PackageParser::AddOutput, this, _1));
    handles_.emplace(Option::RESHEADER, bind(&PackageParser::AddResourceHeader, this, _1));
    handles_.emplace(Option::FORCEWRITE, [this](const string &) -> uint32_t { return ForceWrite(); });
    handles_.emplace(Option::VERSION, [this](const string &) -> uint32_t { return PrintVersion(); });
    handles_.emplace(Option::MODULES, bind(&PackageParser::AddMoudleNames, this, _1));
    handles_.emplace(Option::JSON, bind(&PackageParser::AddConfig, this, _1));
    handles_.emplace(Option::STARTID, bind(&PackageParser::AddStartId, this, _1));
    handles_.emplace(Option::APPEND, bind(&PackageParser::AddAppend, this, _1));
    handles_.emplace(Option::COMBINE, [this](const string &) -> uint32_t { return SetCombine(); });
    handles_.emplace(Option::DEPENDENTRY, bind(&PackageParser::AddDependEntry, this, _1));
    handles_.emplace(Option::HELP, [this](const string &) -> uint32_t { return ShowHelp(); });
    handles_.emplace(Option::IDS, bind(&PackageParser::SetIdDefinedOutput, this, _1));
    handles_.emplace(Option::DEFINED_IDS, bind(&PackageParser::SetIdDefinedInputPath, this, _1));
    handles_.emplace(Option::ICON_CHECK, [this](const string &) -> uint32_t { return IconCheck(); });
    handles_.emplace(Option::TARGET_CONFIG, bind(&PackageParser::ParseTargetConfig, this, _1));
    handles_.emplace(Option::DEFINED_SYSIDS, bind(&PackageParser::AddSysIdDefined, this, _1));
    handles_.emplace(Option::COMPRESSED_CONFIG, bind(&PackageParser::AddCompressionPath, this, _1));
    handles_.emplace(Option::THREAD, bind(&PackageParser::ParseThread, this, _1));
    handles_.emplace(Option::IGNORED_FILE, bind(&PackageParser::ParseIgnoreRegex, this, _1, "--ignored-file"));
    handles_.emplace(Option::IGNORED_PATH, bind(&PackageParser::ParseIgnoreRegex, this, _1, "--ignored-path"));
}

uint32_t PackageParser::HandleProcess(int c, const string &argValue)
{
    auto handler = handles_.find(c);
    if (handler == handles_.end()) {
        PrintError(GetError(ERR_CODE_UNKNOWN_OPTION).FormatCause(std::to_string(c).c_str()));
        return RESTOOL_ERROR;
    }
    return handler->second(argValue);
}

uint32_t PackageParser::ParseFileList(const string& fileListPath)
{
    isFileList_ = true;
    ResConfigParser resConfigParser;
    if (resConfigParser.Init(fileListPath, [this](int c, const string &argValue) -> uint32_t {
        return HandleProcess(c, argValue);
    }) != RESTOOL_SUCCESS) {
        return RESTOOL_ERROR;
    }
    return RESTOOL_SUCCESS;
}

uint32_t PackageParser::CheckError(int argc, char *argv[], int c, int optIndex)
{
    if (optIndex != -1) {
        if ((optarg == nullptr && (optind - 1 < 0 || optind - 1 >= argc)) ||
            (optarg != nullptr && (optind - 2 < 0 || optind - 2 >= argc))) { // 1 or 2 menas optind offset value
            return RESTOOL_ERROR;
        }
        string curOpt = (optarg == nullptr) ? argv[optind - 1] : argv[optind - 2];
        if (curOpt != ("--" + string(CMD_OPTS[optIndex].name))) {
            PrintError(GetError(ERR_CODE_UNKNOWN_OPTION).FormatCause(curOpt.c_str()));
            return RESTOOL_ERROR;
        }
    }
    if (c == Option::UNKNOWN) {
        if (optopt == 0 && (optind - 1 < 0 || optind - 1 >= argc)) {
            return RESTOOL_ERROR;
        }
        string optUnknown = (optopt == 0) ? argv[optind - 1] : ("-" + string(1, optopt));
        PrintError(GetError(ERR_CODE_UNKNOWN_OPTION).FormatCause(optUnknown.c_str()));
        return RESTOOL_ERROR;
    }
    if (c == Option::NO_ARGUMENT) {
        if (optind - 1 < 0 || optind - 1 >= argc) {
            return RESTOOL_ERROR;
        }
        if (IsLongOpt(argc, argv)) {
            PrintError(GetError(ERR_CODE_MISSING_ARGUMENT).FormatCause(argv[optind - 1]));
        } else {
            PrintError(GetError(ERR_CODE_UNKNOWN_OPTION).FormatCause(argv[optind - 1]));
        }
        return RESTOOL_ERROR;
    }
    return RESTOOL_SUCCESS;
}

uint32_t PackageParser::ParseCommand(int argc, char *argv[])
{
    restoolPath_ = string(argv[0]);
    while (optind <= argc) {
        int optIndex = -1;
        int c = getopt_long(argc, argv, CMD_PARAMS.c_str(), CMD_OPTS, &optIndex);
        if (CheckError(argc, argv, c, optIndex) != RESTOOL_SUCCESS) {
            return RESTOOL_ERROR;
        }
        if (c == Option::END) {
            if (argc == optind) {
                break;
            }
            string errmsg;
            for (int i = optind; i < argc; i++) {
                errmsg.append(argv[i]).append(",");
            }
            errmsg.pop_back();
            PrintError(GetError(ERR_CODE_INVALID_ARGUMENT).FormatCause(errmsg.c_str()));
            return RESTOOL_ERROR;
        }

        string argValue = (optarg != nullptr) ? optarg : "";
        if (c == Option::FILELIST) {
            return ParseFileList(argValue);
        }
        if (HandleProcess(c, argValue) != RESTOOL_SUCCESS) {
            return RESTOOL_ERROR;
        }
    }
    return RESTOOL_SUCCESS;
}

bool PackageParser::IsLongOpt(int argc, char *argv[]) const
{
    if (optind - 1 < 0 || optind - 1 >= argc) {
        return false;
    }
    for (auto iter : CMD_OPTS) {
        if (optopt == iter.val && argv[optind - 1] == ("--" + string(iter.name))) {
            return true;
        }
    }
    return false;
}

uint32_t PackageParser::ExecCommand()
{
    return ResourcePack(*this).Package();
}
}
}
}
