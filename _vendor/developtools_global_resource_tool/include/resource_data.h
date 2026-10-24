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

#ifndef OHOS_RESTOOL_RESOURCE_DATA_H
#define OHOS_RESTOOL_RESOURCE_DATA_H

#include <cstdint>
#include <map>
#include <set>
#include <stdint.h>
#include <string>
#include <vector>

namespace OHOS {
namespace Global {
namespace Restool {
const static std::string TOOL_NAME = "restool";
const static std::string RESOURCES_DIR = "resources";
const static std::string CACHES_DIR = ".caches";
const static std::string CONFIG_JSON = "config.json";
const static std::string MODULE_JSON = "module.json";
const static std::string RAW_FILE_DIR = "rawfile";
const static std::string RES_FILE_DIR = "resfile";
const static std::string ID_DEFINED_FILE = "id_defined.json";
const static std::string RESOURCE_INDEX_FILE = "resources.index";
const static std::string JSON_EXTENSION = ".json";
#ifdef __WIN32
const static std::string SEPARATOR_FILE = "\\";
#else
const static std::string SEPARATOR_FILE = "/";
#endif
const static std::string SEPARATOR = "/";
const static std::string WIN_SEPARATOR = "\\";
const static std::string NEW_LINE_PATH = "\nat ";
const static std::string SOLUTIONS = "Solutions:";
const static std::string SOLUTIONS_ARROW = "> ";
const static std::string LONG_PATH_HEAD = "\\\\?\\";
const static int32_t VERSION_MAX_LEN = 128;
static const std::string RESTOOL_NAME = "Restool";
static const std::string RESTOOLV2_NAME = "RestoolV2";
static const std::string RESTOOL_VERSION = { " 6.1.0.003" };
const static int32_t TAG_LEN = 4;
constexpr static int DEFAULT_POOL_SIZE = 8;
static std::set<std::string> g_resourceSet;
static std::set<std::string> g_hapResourceSet;
const static int8_t INVALID_ID = -1;
const static int MIN_SUPPORT_NEW_MODULE_API_VERSION = 20;
const static int MIN_SUPPORT_TS_HEADER_API_VERSION = 23;
const static int API_VERSION_DIVISOR = 1000;
const static int HEX_BASE = 16;

enum class IgnoreType {
    IGNORE_FILE,
    IGNORE_DIR,
    IGNORE_ALL
};

enum class KeyType {
    LANGUAGE = 0,
    REGION = 1,
    RESOLUTION = 2,
    ORIENTATION = 3,
    DEVICETYPE = 4,
    SCRIPT = 5,
    NIGHTMODE = 6,
    MCC = 7,
    MNC = 8,
    // RESERVER 9
    INPUTDEVICE = 10,
    KEY_TYPE_MAX,
    OTHER,
};

enum class ResType {
    ELEMENT = 0,
    RAW = 6,
    INTEGER = 8,
    STRING = 9,
    STRARRAY = 10,
    INTARRAY = 11,
    BOOLEAN = 12,
    COLOR = 14,
    ID = 15,
    THEME = 16,
    PLURAL = 17,
    FLOAT = 18,
    MEDIA = 19,
    PROF = 20,
    PATTERN = 22,
    SYMBOL = 23,
    RES = 24,
    INVALID_RES_TYPE = -1,
};

enum class PackType {
    NORMAL = 1,
    OVERLAP
};

enum class OrientationType {
    VERTICAL = 0,
    HORIZONTAL = 1,
};

enum class DeviceType {
    PHONE = 0,
    TABLET = 1,
    CAR = 2,
    // RESERVER 3
    TV = 4,
    WEARABLE = 6,
    TWOINONE = 7,
};

enum class ResolutionType {
    SDPI = 120,
    MDPI = 160,
    LDPI = 240,
    XLDPI = 320,
    XXLDPI = 480,
    XXXLDPI = 640,
};

enum class NightMode {
    DARK = 0,
    LIGHT = 1,
};

enum class InputDevice {
    INPUTDEVICE_NOT_SET = -1,
    INPUTDEVICE_POINTINGDEVICE = 0,
};

enum class ResourceIdCluster {
    RES_ID_APP = 0,
    RES_ID_SYS,
    RES_ID_TYPE_MAX,
};

enum Option {
    END = -1,
    IDS = 1,
    DEFINED_IDS = 2,
    DEPENDENTRY = 3,
    ICON_CHECK = 4,
    TARGET_CONFIG = 5,
    DEFINED_SYSIDS = 6,
    COMPRESSED_CONFIG = 7,
    THREAD = 8,
    IGNORED_FILE = 9,
    IGNORED_PATH = 10,
    STARTID = 'e',
    FORCEWRITE = 'f',
    HELP = 'h',
    INPUTPATH = 'i',
    JSON = 'j',
    FILELIST = 'l',
    MODULES = 'm',
    OUTPUTPATH = 'o',
    PACKAGENAME = 'p',
    RESHEADER = 'r',
    VERSION = 'v',
    APPEND = 'x',
    COMBINE = 'z',
    UNKNOWN = '?',
    NO_ARGUMENT = ':',
};

const std::map<std::string, OrientationType> g_orientaionMap = {
    { "vertical", OrientationType::VERTICAL },
    { "horizontal", OrientationType::HORIZONTAL },
};

const std::map<std::string, DeviceType> g_deviceMap = {
    { "phone", DeviceType::PHONE },
    { "tablet", DeviceType::TABLET },
    { "car", DeviceType::CAR },
    { "tv", DeviceType::TV },
    { "wearable", DeviceType::WEARABLE },
    { "2in1", DeviceType::TWOINONE },
};

const std::map<std::string, ResolutionType> g_resolutionMap = {
    { "sdpi", ResolutionType::SDPI },
    { "mdpi",  ResolutionType::MDPI },
    { "ldpi",  ResolutionType::LDPI },
    { "xldpi", ResolutionType::XLDPI },
    { "xxldpi", ResolutionType::XXLDPI },
    { "xxxldpi", ResolutionType::XXXLDPI },
};

const std::map<std::string, NightMode> g_nightModeMap = {
    { "dark", NightMode::DARK },
    { "light", NightMode::LIGHT },
};

const std::map<std::string, InputDevice> g_inputDeviceMap = {
    { "pointingdevice", InputDevice::INPUTDEVICE_POINTINGDEVICE },
};

struct KeyParam {
    static const uint32_t KEY_PARAM_LEN = 8;
    KeyType keyType;
    uint32_t value;
    bool operator == (const KeyParam &other)
    {
        return keyType == other.keyType && value == other.value;
    }
};

struct IdData {
    uint32_t id;
    uint32_t dataOffset;
};

struct ResourceId {
    int64_t id;
    int64_t seq;
    std::string type;
    std::string name;
};

struct CompressFilter {
    std::vector<std::string> path;
    std::vector<std::string> excludePath;
    std::string rules;
    std::string excludeRules;
    std::string method;
};

const std::map<std::string, ResType> g_copyFileMap = {
    { RAW_FILE_DIR, ResType::RAW },
    { RES_FILE_DIR, ResType::RES },
};

const std::map<std::string, ResType> g_fileClusterMap = {
    { "element", ResType::ELEMENT },
    { "media", ResType::MEDIA },
    { "profile", ResType::PROF },
};

const std::map<std::string, ResType> g_contentClusterMap = {
    { "id", ResType::ID },
    { "integer", ResType::INTEGER },
    { "string", ResType::STRING },
    { "strarray", ResType::STRARRAY },
    { "intarray", ResType::INTARRAY },
    { "color", ResType::COLOR },
    { "plural", ResType::PLURAL },
    { "boolean", ResType::BOOLEAN },
    { "pattern", ResType::PATTERN },
    { "theme", ResType::THEME },
    { "float", ResType::FLOAT },
    { "symbol", ResType::SYMBOL }
};

const std::map<KeyType, std::string> g_keyTypeToStrMap = {
    {KeyType::MCC, "mcc"},
    {KeyType::MNC, "mnc"},
    {KeyType::LANGUAGE, "language"},
    {KeyType::SCRIPT, "script"},
    {KeyType::REGION, "region"},
    {KeyType::ORIENTATION, "orientation"},
    {KeyType::RESOLUTION, "density"},
    {KeyType::DEVICETYPE, "device"},
    {KeyType::NIGHTMODE, "colorMode"},
    {KeyType::INPUTDEVICE, "inputDevice"},
};

const std::map<int32_t, ResType> g_resTypeMap = {
    { static_cast<int32_t>(ResType::ELEMENT), ResType::ELEMENT},
    { static_cast<int32_t>(ResType::RAW), ResType::RAW},
    { static_cast<int32_t>(ResType::INTEGER), ResType::INTEGER},
    { static_cast<int32_t>(ResType::STRING), ResType::STRING},
    { static_cast<int32_t>(ResType::STRARRAY), ResType::STRARRAY},
    { static_cast<int32_t>(ResType::INTARRAY), ResType::INTARRAY},
    { static_cast<int32_t>(ResType::BOOLEAN), ResType::BOOLEAN},
    { static_cast<int32_t>(ResType::COLOR), ResType::COLOR},
    { static_cast<int32_t>(ResType::ID), ResType::ID},
    { static_cast<int32_t>(ResType::THEME), ResType::THEME},
    { static_cast<int32_t>(ResType::PLURAL), ResType::PLURAL},
    { static_cast<int32_t>(ResType::FLOAT), ResType::FLOAT},
    { static_cast<int32_t>(ResType::MEDIA), ResType::MEDIA},
    { static_cast<int32_t>(ResType::PROF), ResType::PROF},
    { static_cast<int32_t>(ResType::PATTERN), ResType::PATTERN},
    { static_cast<int32_t>(ResType::SYMBOL), ResType::SYMBOL},
    { static_cast<int32_t>(ResType::RES), ResType::RES},
    { static_cast<int32_t>(ResType::INVALID_RES_TYPE), ResType::INVALID_RES_TYPE},
};

const std::map<std::string, std::vector<uint32_t>> g_normalIconMap = {
    { "sdpi-phone", {41, 144} },
    { "sdpi-tablet", {51, 192} },
    { "mdpi-phone", {54, 192} },
    { "mdpi-tablet", {68, 256} },
    { "ldpi-phone", {81, 288} },
    { "ldpi-tablet", {102, 384} },
    { "xldpi-phone", {108, 384} },
    { "xldpi-tablet", {136, 512} },
    { "xxldpi-phone", {162, 576} },
    { "xxldpi-tablet", {204, 768} },
    { "xxxldpi-phone", {216, 768} },
    { "xxxldpi-tablet", {272, 1024} },
};

const std::map<std::string, uint32_t> g_keyNodeIndexs = {
    { "icon", 0 },
    { "startWindowIcon", 1 },
};

struct DirectoryInfo {
    std::string limitKey;
    std::string fileCluster;
    std::string dirPath;
    std::vector<KeyParam> keyParams;
    ResType dirType;
};

struct FileInfo : DirectoryInfo {
    std::string filePath;
    std::string filename;
    ResType fileType;
};

struct TargetConfig {
    std::vector<KeyParam> mccmnc;
    std::vector<KeyParam> locale;
    std::vector<KeyParam> orientation;
    std::vector<KeyParam> device;
    std::vector<KeyParam> colormode;
    std::vector<KeyParam> density;
};

struct Mccmnc {
    KeyParam mcc;
    KeyParam mnc;
    bool operator == (const Mccmnc &other)
    {
        if (mcc.value != other.mcc.value) {
            return false;
        }
        if (mnc.keyType != KeyType::OTHER && other.mnc.keyType != KeyType::OTHER &&
            mnc.value != other.mnc.value) {
            return false;
        }
        return true;
    }
};

struct Locale {
    KeyParam language;
    KeyParam script;
    KeyParam region;
    bool operator == (const Locale &other)
    {
        if (language.value != other.language.value) {
            return false;
        }
        if (script.keyType != KeyType::OTHER && other.script.keyType != KeyType::OTHER &&
            script.value != other.script.value) {
            return false;
        }
        if (region.keyType != KeyType::OTHER && other.region.keyType != KeyType::OTHER &&
            region.value != other.region.value) {
            return false;
        }
        return true;
    }
};

}
}
}
#endif
