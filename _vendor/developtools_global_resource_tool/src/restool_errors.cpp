/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#include <iostream>
#include <sstream>
#ifdef __WIN32
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "resource_util.h"
#include "restool_errors.h"

namespace OHOS {
namespace Global {
namespace Restool {
const std::map<uint32_t, ErrorInfo> ERRORS_MAP = {
    // 11200000
    { ERR_CODE_UNDEFINED_ERROR,
      { ERR_CODE_UNDEFINED_ERROR, "Undefined Error", "Unknown error: %s", "", { "Please try again." }, {} } },

    // 11201xxx
    { ERR_CODE_LOAD_LIBRARY_FAIL,
      { ERR_CODE_LOAD_LIBRARY_FAIL,
        ERR_TYPE_DEPENDENCY,
        "Failed to load the library '%s', %s",
        "",
        { "Make sure the library path is correct and has access permissions.",
          "Install the missing third-party dependency libraries displayed in the error information.",
          "Adding the path 'openharmony\\previewer\\common\\bin' in your SDK path to environment variables." },
        {} } },

    // 11203xxx
    { ERR_CODE_OPEN_JSON_FAIL,
      { ERR_CODE_OPEN_JSON_FAIL,
        ERR_TYPE_CONFIG,
        "Failed to open the JSON file '%s', %s.",
        "",
        { "Make sure the JSON file path is correct and has access permissions." },
        {} } },
    { ERR_CODE_JSON_FORMAT_ERROR,
      { ERR_CODE_JSON_FORMAT_ERROR,
        ERR_TYPE_CONFIG,
        "Failed to parse the JSON file: incorrect format.",
        "",
        { "Check the JSON file and delete unnecessary commas (,).",
          "Check the JSON file to make sure the root bracket is {}." },
        {} } },
    { ERR_CODE_JSON_NODE_MISMATCH,
      { ERR_CODE_JSON_NODE_MISMATCH,
        ERR_TYPE_CONFIG,
        "The value type of node '%s' does not match. Expected type: %s.",
        "",
        {},
        {} } },
    { ERR_CODE_JSON_NODE_MISSING,
      { ERR_CODE_JSON_NODE_MISSING, ERR_TYPE_CONFIG, "The required node '%s' is missing.", "", {}, {} } },
    { ERR_CODE_JSON_NODE_EMPTY,
      { ERR_CODE_JSON_NODE_EMPTY, ERR_TYPE_CONFIG, "The array or object node '%s' cannot be empty.", "", {}, {} } },
    { ERR_CODE_JSON_NOT_ONE_MEMBER,
      { ERR_CODE_JSON_NOT_ONE_MEMBER,
        ERR_TYPE_CONFIG,
        "The node '%s' in the JSON file can only have one member.",
        "",
        {},
        {} } },
    { ERR_CODE_JSON_INVALID_NODE_NAME,
      { ERR_CODE_JSON_INVALID_NODE_NAME, ERR_TYPE_CONFIG, "Invalid node name '%s'. Valid values: %s.", "", {}, {} } },

    // 11204xxx
    { ERR_CODE_CREATE_FILE_ERROR,
      { ERR_CODE_CREATE_FILE_ERROR,
        ERR_TYPE_FILE_RESOURCE,
        "Failed to create the directory or file '%s', %s.",
        "",
        { "Make sure the file path is correct and has access permissions." },
        {} } },
    { ERR_CODE_REMOVE_FILE_ERROR,
      { ERR_CODE_REMOVE_FILE_ERROR,
        ERR_TYPE_FILE_RESOURCE,
        "Failed to delete the directory or file '%s', %s.",
        "",
        { "Make sure the file path is correct and has access permissions." },
        {} } },
    { ERR_CODE_COPY_FILE_ERROR,
      { ERR_CODE_COPY_FILE_ERROR,
        ERR_TYPE_FILE_RESOURCE,
        "Failed to copy the file from '%s' to '%s', %s.",
        "",
        { "Make sure the src and dest file path is correct and has access permissions." },
        {} } },
    { ERR_CODE_OPEN_FILE_ERROR,
      { ERR_CODE_OPEN_FILE_ERROR,
        ERR_TYPE_FILE_RESOURCE,
        "Failed to open the file '%s', %s.",
        "",
        { "Make sure the file path is correct and has access permissions." },
        {} } },
    { ERR_CODE_READ_FILE_ERROR,
      { ERR_CODE_READ_FILE_ERROR,
        ERR_TYPE_FILE_RESOURCE,
        "Failed to read the file '%s', %s.",
        "",
        { "Make sure the file content is correct." },
        {} } },

    // 11210xxx
    { ERR_CODE_UNKNOWN_COMMAND_ERROR,
      { ERR_CODE_UNKNOWN_COMMAND_ERROR,
        ERR_TYPE_COMMAND_PARSE,
        "Unknown command error: %s",
        "",
        { "For details, see the help with option -h/--help." },
        {} } },
    { ERR_CODE_UNKNOWN_OPTION,
      { ERR_CODE_UNKNOWN_OPTION,
        ERR_TYPE_COMMAND_PARSE,
        "Unknown option '%s'.",
        "",
        { "For details, see the help with option -h/--help." },
        {} } },
    { ERR_CODE_MISSING_ARGUMENT,
      { ERR_CODE_MISSING_ARGUMENT,
        ERR_TYPE_COMMAND_PARSE,
        "Option '%s' requires an argument.",
        "",
        { "For details, see the help with option -h/--help." },
        {} } },
    { ERR_CODE_INVALID_ARGUMENT,
      { ERR_CODE_INVALID_ARGUMENT,
        ERR_TYPE_COMMAND_PARSE,
        "Invalid argument value '%s'.",
        "",
        { "For details, see the help with option -h/--help." },
        {} } },
    { ERR_CODE_INVALID_INPUT,
      { ERR_CODE_INVALID_INPUT,
        ERR_TYPE_COMMAND_PARSE,
        "Invalid input path '%s'.",
        "",
        { "Make sure the input path of option -i/--inputPath is correct." },
        {} } },
    { ERR_CODE_DUPLICATE_INPUT,
      { ERR_CODE_DUPLICATE_INPUT,
        ERR_TYPE_COMMAND_PARSE,
        "Duplicated input path '%s'.",
        "",
        { "Make sure the input path of option -i/--inputPath is unique." },
        {} } },
    { ERR_CODE_DOUBLE_PACKAGE_NAME,
      { ERR_CODE_DOUBLE_PACKAGE_NAME,
        ERR_TYPE_COMMAND_PARSE,
        "The package name '%s' and '%s' conflict.",
        "",
        { "Make sure the option -p/--packageName only specified once." },
        {} } },
    { ERR_CODE_INVALID_OUTPUT,
      { ERR_CODE_INVALID_OUTPUT,
        ERR_TYPE_COMMAND_PARSE,
        "Invalid output path '%s'.",
        "",
        { "Make sure the output path of option -o/--outputPath is correct." },
        {} } },
    { ERR_CODE_DOUBLE_OUTPUT,
      { ERR_CODE_DOUBLE_OUTPUT,
        ERR_TYPE_COMMAND_PARSE,
        "The output paths '%s' and '%s' conflict.",
        "",
        { "Make sure the option -o/--outputPath only specified once." },
        {} } },
    { ERR_CODE_DUPLICATE_RES_HEADER,
      { ERR_CODE_DUPLICATE_RES_HEADER,
        ERR_TYPE_COMMAND_PARSE,
        "Duplicated resource header path '%s'.",
        "",
        { "Make sure the path of option -r/--resHeader is unique." },
        {} } },
    { ERR_CODE_DOUBLE_MODULES,
      { ERR_CODE_DOUBLE_MODULES,
        ERR_TYPE_COMMAND_PARSE,
        "The module names '%s' and '%s' conflict.",
        "",
        { "Make sure the option -m/--modules only specified once." },
        {} } },
    { ERR_CODE_DUPLICATE_MODULE_NAME,
      { ERR_CODE_DUPLICATE_MODULE_NAME,
        ERR_TYPE_COMMAND_PARSE,
        "Duplicated module name '%s'.",
        "",
        { "Make sure the module names specified with option -m/--modules is unique." },
        {} } },
    { ERR_CODE_DOUBLE_CONFIG_JSON,
      { ERR_CODE_DOUBLE_CONFIG_JSON,
        ERR_TYPE_COMMAND_PARSE,
        "The paths '%s' and '%s' of the module.json(in Stage Model) or config.json(in FA Model) file conflict.",
        "",
        { "Make sure the option -j/--json only specified once." },
        {} } },
    { ERR_CODE_INVALID_START_ID,
      { ERR_CODE_INVALID_START_ID,
        ERR_TYPE_COMMAND_PARSE,
        "Invalid start ID '%s'. It is out of range.",
        "",
        { "Make sure the start id in the scope [0x01000000, 0x06FFFFFF) or [0x08000000, 0xFFFFFFFF)." },
        {} } },
    { ERR_CODE_DUPLICATE_APPEND_PATH,
      { ERR_CODE_DUPLICATE_APPEND_PATH,
        ERR_TYPE_COMMAND_PARSE,
        "Duplicated append path '%s'.",
        "",
        { "Make sure the path of option -x/--append is unique." },
        {} } },
    { ERR_CODE_DOUBLE_TARGET_CONFIG,
      { ERR_CODE_DOUBLE_TARGET_CONFIG,
        ERR_TYPE_COMMAND_PARSE,
        "The target configurations '%s' and '%s' conflict.",
        "",
        { "Make sure the option --target-config only specified once." },
        {} } },
    { ERR_CODE_INVALID_TARGET_CONFIG,
      { ERR_CODE_INVALID_TARGET_CONFIG,
        ERR_TYPE_COMMAND_PARSE,
        "Invalid target configuration argument '%s'. The argument format for option '%s' should be like "
        "'MccMnc[mcc460_mnc001];Locale[zh_CN,en_US];Device[phone];ColorMode[dark];Density[ldpi,xldpi]'.",
        "",
        { "For more information, see the developer documentation." },
        {} } },
    { ERR_CODE_INVALID_SYSTEM_ID_DEFINED,
      { ERR_CODE_INVALID_SYSTEM_ID_DEFINED,
        ERR_TYPE_COMMAND_PARSE,
        "Invalid system id_defined.json path '%s'.",
        "",
        { "Make sure the system id_defined.json path is correct." },
        {} } },
    { ERR_CODE_DUPLICATE_SYSTEM_ID_DEFINED,
      { ERR_CODE_DUPLICATE_SYSTEM_ID_DEFINED,
        ERR_TYPE_COMMAND_PARSE,
        "Duplicated system id_defined.json path '%s'.",
        "",
        { "Make sure the system id_defined.json path is unique." },
        {} } },
    { ERR_CODE_DOUBLE_COMPRESSION_PATH,
      { ERR_CODE_DOUBLE_COMPRESSION_PATH,
        ERR_TYPE_COMMAND_PARSE,
        "The compression JSON paths '%s' and '%s' conflict.",
        "",
        { "Make sure the option --compressed-config only specified once." },
        {} } },
    { ERR_CODE_NON_ASCII,
      { ERR_CODE_NON_ASCII,
        ERR_TYPE_COMMAND_PARSE,
        "The argument value '%s' is not an ASCII value.",
        "",
        { "Make sure all the arguments value characters is ASCII." },
        {} } },
    { ERR_CODE_EXCLUSIVE_OPTION,
      { ERR_CODE_EXCLUSIVE_OPTION,
        ERR_TYPE_COMMAND_PARSE,
        "Options '%s' and '%s' %s.",
        "",
        {},
        {} } },
    { ERR_CODE_PACKAGE_NAME_EMPTY,
      { ERR_CODE_PACKAGE_NAME_EMPTY,
        ERR_TYPE_COMMAND_PARSE,
        "The package name is empty. It should be specified with option -p/--packageName.",
        "",
        { "Specifies the package name with option -p/--packageName." },
        {} } },
    { ERR_CODE_RES_HEADER_PATH_EMPTY,
      { ERR_CODE_RES_HEADER_PATH_EMPTY,
        ERR_TYPE_COMMAND_PARSE,
        "The resource header path (for example, ./ResourceTable.js, ./ResourceTable.h) is empty. It should be specified"
        " with option -r/--resHeader.",
        "",
        { "Specifies header path with option -r/--resHeader." },
        {} } },
    { ERR_CODE_DUMP_MISSING_INPUT,
      { ERR_CODE_DUMP_MISSING_INPUT,
        ERR_TYPE_COMMAND_PARSE,
        "The HAP path of the resource dump command is missing.",
        "",
        { "Specifies a HAP path with dump command." },
        {} } },
    { ERR_CODE_DUMP_INVALID_INPUT,
      { ERR_CODE_DUMP_INVALID_INPUT,
        ERR_TYPE_COMMAND_PARSE,
        "Invalid HAP path '%s' in the resource dump command.",
        "",
        { "Make sure the HAP path of dump command is correct." },
        {} } },
    { ERR_CODE_INVALID_THREAD_COUNT,
      { ERR_CODE_INVALID_THREAD_COUNT,
        ERR_TYPE_COMMAND_PARSE,
        "Invalid thread count '%s'. It should be an integer greater than 0.",
        "",
        {},
        {} } },
    { ERR_CODE_INVALID_IGNORE_FILE,
      { ERR_CODE_INVALID_IGNORE_FILE,
        ERR_TYPE_COMMAND_PARSE,
        "Invalid ignore pattern '%s', %s",
        "",
        { "Make sure the argument of the option '%s' contains valid regular expressions." },
        {} } },

    // 11211xxx
    { ERR_CODE_OUTPUT_EXIST,
      { ERR_CODE_OUTPUT_EXIST,
        ERR_TYPE_RESOURCE_PACK,
        "The output path exists. Specify option -f/--forceWrite to overwrite.",
        "",
        { "Remove the exist output dir or use option -f to force write." },
        {} } },
    { ERR_CODE_CONFIG_JSON_MISSING,
      { ERR_CODE_CONFIG_JSON_MISSING,
        ERR_TYPE_RESOURCE_PACK,
        "There are multiple input paths, but the path of the module.json (in the stage model) or config.json "
        "(in the FA model) file is not specified with option -j/--json.",
        "",
        { "Specifies option -j/--json." },
        {} } },
    { ERR_CODE_INVALID_MODULE_TYPE,
      { ERR_CODE_INVALID_MODULE_TYPE,
        ERR_TYPE_RESOURCE_PACK,
        "Invalid module type '%s'. Valid values: [\"entry\", \"har\", \"shared\", \"feature\"].",
        "",
        {},
        {} } },
    { ERR_CODE_EXCLUSIVE_START_ID,
      { ERR_CODE_EXCLUSIVE_START_ID,
        ERR_TYPE_RESOURCE_PACK,
        "The start ID '%lu' specified by option -e/--startId conflict with the IDs in the id_defined.json file.",
        "",
        { "Retain only one of the id_defined.json and the start id." },
        {} } },
    { ERR_CODE_ID_DEFINED_INVALID_TYPE,
      { ERR_CODE_ID_DEFINED_INVALID_TYPE,
        ERR_TYPE_RESOURCE_PACK,
        "Invalid resource type '%s' in the id_defined.json file. Valid values: %s.",
        "",
        {},
        {} } },
    { ERR_CODE_ID_DEFINED_INVALID_ID,
      { ERR_CODE_ID_DEFINED_INVALID_ID,
        ERR_TYPE_RESOURCE_PACK,
        "Invalid id value '%s' in the id_defined.json file. It should be a hexadecimal string, match the pattern "
        "^0[xX][0-9a-fA-F]{8}, and be in the scope [0x01000000,0x06FFFFFF] or [0x08000000,0xFFFFFFFF].",
        "",
        {},
        {} } },
    { ERR_CODE_ID_DEFINED_ORDER_MISMATCH,
      { ERR_CODE_ID_DEFINED_ORDER_MISMATCH,
        ERR_TYPE_RESOURCE_PACK,
        "The order value '%lu' in the id_defined.json file does not match the record element sequence '%lu'. "
        "Expected value: %lu.",
        "",
        { "Make sure the order value is same as the sequence." },
        {} } },
    { ERR_CODE_ID_DEFINED_SAME_ID,
      { ERR_CODE_ID_DEFINED_SAME_ID,
        ERR_TYPE_RESOURCE_PACK,
        "The names '%s' and '%s' in the id_defined.json file define the same ID.",
        "",
        {},
        {} } },
    { ERR_CODE_MODULE_NAME_NOT_FOUND,
      { ERR_CODE_MODULE_NAME_NOT_FOUND,
        ERR_TYPE_RESOURCE_PACK,
        "The module name '%s' is not found in %s, which is specified by -m/--modules.",
        "",
        { "Make sure the module name in module.json(in Stage Model) or config.json(in FA Model) is correct." },
        {} } },
    { ERR_CODE_INVALID_RESOURCE_PATH,
      { ERR_CODE_INVALID_RESOURCE_PATH,
        ERR_TYPE_RESOURCE_PACK,
        "Failed to scan resources: invalid path '%s', %s.",
        "",
        {},
        {} } },
    { ERR_CODE_INVALID_LIMIT_KEY,
      { ERR_CODE_INVALID_LIMIT_KEY,
        ERR_TYPE_RESOURCE_PACK,
        "Invalid qualifier key '%s'. It should match the pattern of the qualifiers directory, for example zh_CN or "
        "en_US.",
        "",
        { "For more information, see the developer documentation." },
        {} } },
    { ERR_CODE_INVALID_RESOURCE_DIR,
      { ERR_CODE_INVALID_RESOURCE_DIR,
        ERR_TYPE_RESOURCE_PACK,
        "Invalid resource directory name '%s'. Valid values: %s.",
        "",
        {},
        {} } },
    { ERR_CODE_INVALID_TRANSLATE_PRIORITY,
      { ERR_CODE_INVALID_TRANSLATE_PRIORITY,
        ERR_TYPE_RESOURCE_PACK,
        "Invalid translate priority value '%s'. Valid values: %s.",
        "",
        {},
        {} } },
    { ERR_CODE_INVALID_ELEMENT_TYPE,
      { ERR_CODE_INVALID_ELEMENT_TYPE,
        ERR_TYPE_RESOURCE_PACK,
        "Unsupported element resource type '%s'. Valid values: %s.",
        "",
        {},
        {} } },
    { ERR_CODE_INVALID_COLOR_VALUE,
      { ERR_CODE_INVALID_COLOR_VALUE,
        ERR_TYPE_RESOURCE_PACK,
        "Invalid color value '%s' of the resource '%s'. It can only reference '$color:xxx' or be '#rgb','#argb',"
        "'#rrggbb', or '#aarrggbb'.",
        "",
        {},
        {} } },
    { ERR_CODE_INVALID_RESOURCE_REF,
      { ERR_CODE_INVALID_RESOURCE_REF,
        ERR_TYPE_RESOURCE_PACK,
        "Invalid resource reference '%s'. Supported reference: '%sxxx'.",
        "",
        {},
        {} } },
    { ERR_CODE_PARENT_EMPTY,
      { ERR_CODE_PARENT_EMPTY,
        ERR_TYPE_RESOURCE_PACK,
        "The parent value of resource '%s' is empty. It should be a valid resource name.",
        "",
        {},
        {} } },
    { ERR_CODE_ARRAY_TOO_LARGE,
      { ERR_CODE_ARRAY_TOO_LARGE,
        ERR_TYPE_RESOURCE_PACK,
        "The array resource '%s' is too large. The total length of the value of the array elements cannot exceed "
        "65535.",
        "",
        { "Separate the large array into multiple arrays." },
        {} } },
    { ERR_CODE_INVALID_QUANTITY,
      { ERR_CODE_INVALID_QUANTITY,
        ERR_TYPE_RESOURCE_PACK,
        "Invalid quantity '%s' of the plural resource '%s'. Valid values: %s.",
        "",
        {},
        {} } },
    { ERR_CODE_DUPLICATE_QUANTITY,
      { ERR_CODE_DUPLICATE_QUANTITY,
        ERR_TYPE_RESOURCE_PACK,
        "Duplicated quantity '%s' of the plural resource '%s'.",
        "",
        { "Make sure the quantity of the plural resource is unique." },
        {} } },
    { ERR_CODE_QUANTITY_NO_OTHER,
      { ERR_CODE_QUANTITY_NO_OTHER,
        ERR_TYPE_RESOURCE_PACK,
        "The plural resource '%s' should contain the 'other' quantity.",
        "",
        {},
        {} } },
    { ERR_CODE_INVALID_SYMBOL,
      { ERR_CODE_INVALID_SYMBOL,
        ERR_TYPE_RESOURCE_PACK,
        "Invalid value '%s' of the symbol resource '%s'. It should be in the scope [0xF0000,0xFFFFF] or "
        "[0x100000,0x10FFFF]."
        "",
        {},
        {} } },
    { ERR_CODE_INVALID_RESOURCE_NAME,
      { ERR_CODE_INVALID_RESOURCE_NAME,
        ERR_TYPE_RESOURCE_PACK,
        "Invalid resource name '%s'. It should match the pattern [a-zA-Z0-9_].",
        "",
        { "Modify the name to match the pattern [a-zA-Z0-9_]." },
        {} } },
    { ERR_CODE_RESOURCE_DUPLICATE,
      { ERR_CODE_RESOURCE_DUPLICATE,
        ERR_TYPE_RESOURCE_PACK,
        "Resource '%s' conflict. It is first declared at '%s' and declared again at '%s'.",
        "",
        { "Make sure the resource name of the same type is unique." },
        {} } },
    { ERR_CODE_RESOURCE_ID_EXCEED,
      { ERR_CODE_RESOURCE_ID_EXCEED,
        ERR_TYPE_RESOURCE_PACK,
        "The resource ID '%lu' exceeds the maximum ID '%lu'.",
        "",
        { "Delete useless resources and recompile.", "Specify a smaller start id." },
        {} } },
    { ERR_CODE_RESOURCE_ID_NOT_DEFINED,
      { ERR_CODE_RESOURCE_ID_NOT_DEFINED,
        ERR_TYPE_RESOURCE_PACK,
        "The id of resource '%s' of the '%s' type is not defined.",
        "",
        { "Delete useless resources and recompile." },
        {} } },
    { ERR_CODE_REF_NOT_DEFINED,
      { ERR_CODE_REF_NOT_DEFINED,
        ERR_TYPE_RESOURCE_PACK,
        "The resource reference '%s' is not defined.",
        "",
        { "Check whether this resource is defined anywhere." },
        {} } },
    { ERR_CODE_INVALID_RESOURCE_INDEX,
      { ERR_CODE_INVALID_RESOURCE_INDEX,
        ERR_TYPE_RESOURCE_PACK,
        "Failed to parse the resources.index file, %s.",
        "",
        { "Verify that the format of the resource.index file is correct." },
        {} } },

    // 11212xxx
    { ERR_CODE_PARSE_HAP_ERROR,
      { ERR_CODE_PARSE_HAP_ERROR, ERR_TYPE_RESOURCE_DUMP, "Failed to parse the HAP, %s.", "", {}, {} } },
};

#ifdef __WIN32
constexpr int WIN_LOCALE_CN = 2052;
#endif
const std::string LOCALE_CMD_WIN = "wmic os get locale";
const std::string LOCALE_CMD_LINUX = "locale";
const std::string LOCALE_CMD_MAC = "defaults read -globalDomain AppleLocale";
const std::string LOCALE_CN = "zh_CN";

std::map<uint32_t, FaqInfo> faqInfos;
FaqInfo defaultMoreInfo = {};
Language osLanguage = Language::EN;

bool IsValidCmd(const std::string &cmd)
{
    if (cmd == LOCALE_CMD_WIN || cmd == LOCALE_CMD_LINUX || cmd == LOCALE_CMD_MAC) {
        return true;
    }
    return false;
}

#ifdef __WIN32
std::string ExecuteCommand(const std::string &cmd)
{
    if (!IsValidCmd(cmd)) {
        return "";
    }
    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;
    HANDLE hReadPipe;
    HANDLE hWritePipe;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &saAttr, 0)) {
        return "CreatePipe failed";
    }
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdError = hWritePipe;
    si.hStdOutput = hWritePipe;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.dwFlags |= STARTF_USESTDHANDLES;
    ZeroMemory(&pi, sizeof(pi));

    if (!CreateProcess(NULL, const_cast<LPSTR>(cmd.c_str()), NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
        return "CreateProcess failed";
    }
    // close the write end of the pipe in the parent process
    CloseHandle(hWritePipe);
    std::string result;
    DWORD bytesRead;
    CHAR buffer[BUFFER_SIZE_SMALL];
    while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead != 0) {
        buffer[bytesRead] = '\0';
        result += buffer;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(hReadPipe);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return result;
}
#else
std::vector<std::string> split(const std::string &s, const char &delimiter)
{
    std::vector<std::string> tokens;
    std::string token;
    std::stringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter)) { tokens.push_back(token); }
    return tokens;
}

std::string ExecuteCommand(const std::string &cmd)
{
    std::string result;
    if (!IsValidCmd(cmd)) {
        return result;
    }
    int pipefd[2];
    pid_t pid;
    if (pipe(pipefd) == -1) {
        perror("open pipe failed");
        return result;
    }
    pid = fork();
    if (pid == -1) {
        perror("fork failed");
        return result;
    }
    if (pid == 0) {
        // child process
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        std::vector<std::string> cmds = split(cmd, ' ');
        char *argv[cmds.size() + 1];
        size_t i = 0;
        for (i = 0; i < cmds.size(); ++i) {
            argv[i] = cmds[i].data();
        }
        argv[i] = nullptr;
        execvp(argv[0], argv);
        // if execvp returns, there was an error.
        perror("execvp failed");
        throw std::runtime_error("execvp failed");
    } else {
        // parent process
        // close unused write end
        close(pipefd[1]);
        char buffer[BUFFER_SIZE_SMALL];
        ssize_t readBytes;
        while ((readBytes = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
            buffer[readBytes] = '\0';
            result += buffer;
        }
        close(pipefd[0]);
        wait(nullptr);
    }
    return result;
}
#endif

#ifdef __WIN32
Language GetWinLanguage()
{
    std::string result = ExecuteCommand(LOCALE_CMD_WIN);
    size_t pos = 0;
    std::string locale = "Locale";
    if ((pos = result.find(locale)) != std::string::npos) {
        result.replace(pos, locale.length(), "");
    }
    if (result.empty()) {
        return Language::CN;
    }
    char *end;
    errno = 0;
    int localeCode = static_cast<int>(strtol(result.c_str(), &end, 16));
    if (end == result.c_str() || errno == ERANGE || localeCode == INT_MIN || localeCode == INT_MAX) {
        return Language::CN;
    }
    if (localeCode == WIN_LOCALE_CN) {
        return Language::CN;
    }
    return Language::EN;
}
#endif

#ifdef __LINUX__
Language GetLinuxLanguage()
{
    std::string result = ExecuteCommand(LOCALE_CMD_LINUX);
    if (result.empty()) {
        return Language::CN;
    }
    std::vector<std::string> localeLines = split(result, '\n');
    for (const std::string &line : localeLines) {
        std::vector<std::string> keyValue = split(line, '=');
        if (keyValue.size() <= 1) {
            continue;
        }
        std::string key = keyValue[0];
        std::string value = keyValue[1];
        if ((key == "LC_ALL" || key == "LC_MESSAGES" || key == "LANG" || key == "LANGUAGE")
            && value.find(LOCALE_CN) != std::string::npos) {
            return Language::CN;
        }
    }
    return Language::EN;
}
#endif

#ifdef __MAC__
Language GetMacLanguage()
{
    std::string result = ExecuteCommand(LOCALE_CMD_MAC);
    if (result.empty()) {
        return Language::CN;
    }
    if (result.find(LOCALE_CN) != std::string::npos) {
        return Language::CN;
    }
    return Language::EN;
}
#endif

Language GetOsLanguage()
{
#ifdef __WIN32
    return GetWinLanguage();
#endif

#ifdef __LINUX__
    return GetLinuxLanguage();
#endif

#ifdef __MAC__
    return GetMacLanguage();
#endif
    return Language::CN;
}

void GetExtSolutions(cJSON *node, FaqInfo &info)
{
    if (!node || !cJSON_IsArray(node)) {
        return;
    }
    for (cJSON *subNode = node->child; subNode; subNode = subNode->next) {
        ExtSolution solution{};
        cJSON *fileNameNode = cJSON_GetObjectItem(subNode, "fileName");
        if (fileNameNode && cJSON_IsString(fileNameNode)) {
            solution.fileName = fileNameNode->valuestring;
        }
        cJSON *solutionNode = cJSON_GetObjectItem(subNode, "solution");
        if (solutionNode && cJSON_IsString(solutionNode)) {
            solution.solution = solutionNode->valuestring;
        }
        info.extSolutions.push_back(solution);
    }
}

void GetFaqInfo(cJSON *node, FaqInfo &info)
{
    if (node && cJSON_IsObject(node)) {
        cJSON *cn = cJSON_GetObjectItem(node, "cn");
        if (cn && cJSON_IsString(cn)) {
            info.cn = cn->valuestring;
        }
        cJSON *en = cJSON_GetObjectItem(node, "en");
        if (en && cJSON_IsString(en)) {
            info.en = en->valuestring;
        }
        GetExtSolutions(cJSON_GetObjectItem(node, "extSolutions"), info);
    }
}

void InitFaq(const std::string &restoolPath)
{
    osLanguage = GetOsLanguage();
    cJSON *root;
    std::string moreInfoPath = FileEntry::FilePath(restoolPath).GetParent().Append(ERROR_MORE_INFO_FILE).GetPath();
    if (!ResourceUtil::OpenJsonFile(moreInfoPath, &root, false)) {
        return;
    }
    if (!root || !cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return;
    }
    cJSON *defaultNode = cJSON_GetObjectItem(root, "default");
    GetFaqInfo(defaultNode, defaultMoreInfo);
    cJSON *faqsNode = cJSON_GetObjectItem(root, "faqs");
    if (!faqsNode || !cJSON_IsArray(faqsNode) || cJSON_GetArraySize(faqsNode) == 0) {
        cJSON_Delete(root);
        return;
    }
    for (cJSON *infoNode = faqsNode->child; infoNode; infoNode = infoNode->next) {
        cJSON *codeNode = cJSON_GetObjectItem(infoNode, "code");
        if (!codeNode || !cJSON_IsNumber(codeNode)) {
            continue;
        }
        uint32_t code = static_cast<uint32_t>(codeNode->valueint);
        FaqInfo info = {};
        GetFaqInfo(infoNode, info);
        faqInfos[code] = info;
    }
    cJSON_Delete(root);
}

ErrorInfo GetError(const uint32_t &errCode)
{
    ErrorInfo error;
    auto it = ERRORS_MAP.find(errCode);
    if (it != ERRORS_MAP.end()) {
        error = it->second;
    }
    auto faq = faqInfos.find(errCode);
    if (faq != faqInfos.end()) {
        error.moreInfo_ = { faq->second.cn, faq->second.en };
    }
    return error;
}

void PrintError(const uint32_t &errCode)
{
    PrintError(GetError(errCode));
}

void PrintError(const ErrorInfo &error)
{
    std::string errMsg;
    errMsg.append("Error Code: ").append(std::to_string(error.code_)).append("\n");
    errMsg.append("Error: ").append(error.description_).append("\n");
    errMsg.append("Error Message: ").append(error.cause_);
    if (!error.position_.empty()) {
        errMsg.append(" At file: ").append(error.position_);
    }
    errMsg.append("\n");
    if (error.solutions_.empty()) {
        std::cerr << errMsg;
        return;
    }
    errMsg.append("* Try the following:").append("\n");
    auto faq = faqInfos.find(error.code_);
    if (faq != faqInfos.end()) {
        std::vector<ExtSolution> extSolutions = faq->second.extSolutions;
        for (const auto &solution : extSolutions) {
            if (solution.fileName.empty()) {
                errMsg.append("  > ").append(solution.solution).append("\n");
                continue;
            }
            if (FileEntry(error.position_).GetFilePath().GetFilename() == solution.fileName) {
                errMsg.append("  > ").append(solution.solution).append("\n");
            }
        }
    }
    for (const auto &solution : error.solutions_) {
        errMsg.append("  > ").append(solution).append("\n");
    }
    std::string moreInfo;
    if (osLanguage == Language::CN) {
        if (!error.moreInfo_.cn.empty()) {
            moreInfo = error.moreInfo_.cn;
        } else {
            moreInfo = defaultMoreInfo.cn;
        }
    } else {
        if (!error.moreInfo_.en.empty()) {
            moreInfo = error.moreInfo_.en;
        } else {
            moreInfo = defaultMoreInfo.en;
        }
    }
    if (!moreInfo.empty()) {
        errMsg.append("  > More info: ").append(moreInfo).append("\n");
    }
    errMsg = FileEntry::Utf8ToSysDefault(errMsg);
    std::cerr << errMsg;
}
} // namespace Restool
} // namespace Global
} // namespace OHOS
