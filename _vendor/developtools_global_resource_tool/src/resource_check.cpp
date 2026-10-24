/*
 * Copyright (c) 2023-2024 Huawei Device Co., Ltd.
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
#include "resource_check.h"
#include "file_manager.h"
#include <png.h>

namespace OHOS {
namespace Global {
namespace Restool {
using namespace std;

namespace {
    constexpr int PNG_BYTRS_TO_CHECK = 8;
}

ResourceCheck::ResourceCheck(const std::map<std::string, std::set<uint32_t>> &jsonCheckIds,
    const shared_ptr<ResourceAppend> &resourceAppend) : jsonCheckIds_(jsonCheckIds), resourceAppend_(resourceAppend)
{
}

void ResourceCheck::CheckConfigJson()
{
    auto &fileManager = FileManager::GetInstance();
    auto &allResource = fileManager.GetResources();
    for (auto it = jsonCheckIds_.begin(); it != jsonCheckIds_.end(); it++) {
        for (const auto &id : it->second) {
            auto res = allResource.find(id);
            if (res == allResource.end()) {
                continue;
            }
            for (auto resourceItem : res->second) {
                CheckNodeInResourceItem(it->first, resourceItem);
            }
        }
    }
}

void ResourceCheck::CheckConfigJsonForCombine()
{
    auto &allResource = resourceAppend_->GetItems();
    for (auto it = jsonCheckIds_.begin(); it != jsonCheckIds_.end(); it++) {
        for (const auto &id : it->second) {
            auto res = allResource.find(id);
            if (res == allResource.end()) {
                continue;
            }
            for (auto resourceItemPtr : res->second) {
                CheckNodeInResourceItem(it->first, *resourceItemPtr);
            }
        }
    }
}

void ResourceCheck::CheckNodeInResourceItem(const string &key, const ResourceItem &resourceItem)
{
    string filePath = resourceItem.GetFilePath();
    uint32_t width;
    uint32_t height;
    if (!GetPngWidthAndHeight(filePath, &width, &height)) {
        return;
    }
    if (width != height) {
        cerr << "Warning: the png width and height not equal" << NEW_LINE_PATH << filePath << endl;
        return;
    }
    auto result = g_keyNodeIndexs.find(key);
    if (result == g_keyNodeIndexs.end()) {
        return;
    }
    uint32_t normalSize = ResourceUtil::GetNormalSize(resourceItem.GetKeyParam(), result->second);
    if (normalSize != 0 && width > normalSize) {
        string warningMsg = "Warning: The width or height of the png file referenced by the " + key + \
            " exceeds the limit (" + to_string(normalSize) + " pixels)" + NEW_LINE_PATH + filePath;
        cout << warningMsg << endl;
    }
}

bool ResourceCheck::IsValidPngImage(FILE *&in) const
{
    char checkheader[PNG_BYTRS_TO_CHECK];
    if (fread(checkheader, 1, PNG_BYTRS_TO_CHECK, in) != PNG_BYTRS_TO_CHECK) {
        return false;
    }
    if (png_sig_cmp(reinterpret_cast<png_const_bytep>(checkheader), 0, PNG_BYTRS_TO_CHECK) != 0) {
        return false;
    }

    rewind(in);
    return true;
}

bool ResourceCheck::GetPngWidthAndHeight(const string &filePath, uint32_t *width, uint32_t *height)
{
    FILE *in = fopen(filePath.c_str(), "rb");
    if (in == nullptr) {
        cout << "Warning: " << filePath << " can not open" << endl;
        return false;
    }
    if (!IsValidPngImage(in)) {
        cout << "Warning: " << filePath << " is not png format" << endl;
        CloseFile(in);
        return false;
    }
    png_structp pngHandle = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (pngHandle == nullptr) {
        CloseFile(in);
        return false;
    }
    png_infop infoHandle = png_create_info_struct(pngHandle);
    if (infoHandle == nullptr) {
        CloseFile(in);
        png_destroy_read_struct(&pngHandle, nullptr, nullptr);
        return false;
    }
    png_init_io(pngHandle, in);
    png_read_info(pngHandle, infoHandle);
    unsigned int w;
    unsigned int h;
    png_get_IHDR(pngHandle, infoHandle, &w, &h, nullptr, nullptr, nullptr, nullptr, nullptr);
    *width = w;
    *height = h;
    CloseFile(in);
    png_destroy_read_struct(&pngHandle, &infoHandle, 0);
    return true;
}

void ResourceCheck::CloseFile(FILE *fp)
{
    if (fp != nullptr) {
        fclose(fp);
    }
}

}
}
}