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

#ifndef OHOS_RESTOOL_FILE_ENTRY_H
#define OHOS_RESTOOL_FILE_ENTRY_H

#include <memory>
#include <vector>
#include <string>
#ifdef _WIN32
#include "shlwapi.h"
#include "windows.h"
#endif

namespace OHOS {
namespace Global {
namespace Restool {
class FileEntry {
public:
    class FilePath {
    public:
        FilePath(const std::string &path);
        virtual ~FilePath();
        FilePath Append(const std::string &path);
        FilePath ReplaceExtension(const std::string &extension);
        FilePath GetParent();
        const std::string &GetPath() const;
        const std::string &GetFilename() const;
        const std::string &GetExtension() const;
        const std::vector<std::string> GetSegments() const;
    private:
        void Init();
        void Format();
        std::string filePath_;
        std::string filename_;
        std::string parent_;
        std::string extension_;
    };

    FileEntry(const std::string &path);
    virtual ~FileEntry();
    bool Init();
    const std::vector<std::unique_ptr<FileEntry>> GetChilds() const;
    bool IsFile() const;
    const FilePath &GetFilePath() const;
    static bool Exist(const std::string &path);
    static bool RemoveAllDir(const std::string &path);
    static bool RemoveFile(const std::string &path);
    static bool CreateDirs(const std::string &path);
    static bool CopyFileInner(const std::string &src, const std::string &dst);
    static bool MakeWritable(const std::string &path);
    static bool IsDirectory(const std::string &path);
    static std::string RealPath(const std::string &path);
    static std::string AdaptLongPath(const std::string &path);
    static std::string Utf8ToSysDefault(const std::string &path);

private:
    bool IsIgnore(const std::string &filename) const;
    static bool RemoveAllDirInner(const FileEntry &entry);
    static bool CreateDirsInner(const std::string &path, std::string::size_type offset);
#ifdef _WIN32
    static std::wstring AdaptLongPathW(const std::string &path);
    static std::string Wstring2String(const std::wstring &wstr, const int codePage = CP_UTF8);
    static std::wstring String2Wstring(const std::string &str, const int codePage = CP_UTF8);
#endif
    FilePath filePath_;
    bool isFile_;
    static const std::string SEPARATE;
};
}
}
}
#endif