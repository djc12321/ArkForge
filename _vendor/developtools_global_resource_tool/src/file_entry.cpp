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

#include "file_entry.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include "dirent.h"
#include "sys/stat.h"
#include "unistd.h"
#ifdef _WIN32
#include "shlwapi.h"
#include "windows.h"
#endif
#include "resource_data.h"
#include "restool_errors.h"

namespace OHOS {
namespace Global {
namespace Restool {
#ifdef _WIN32
const std::string FileEntry::SEPARATE = "\\";
#else
const std::string FileEntry::SEPARATE = "/";
#endif

using namespace std;
FileEntry::FileEntry(const string &path)
    : filePath_(path), isFile_(false)
{
}

FileEntry::~FileEntry()
{
}

bool FileEntry::Init()
{
    string filePath = filePath_.GetPath();
    if (!Exist(filePath)) {
        cerr << "Warning: file not exist: " << filePath << endl;
        return false;
    }

    isFile_ = !IsDirectory(filePath);
    return true;
}

const vector<unique_ptr<FileEntry>> FileEntry::GetChilds() const
{
    vector<unique_ptr<FileEntry>> children;
    string filePath = filePath_.GetPath();
#ifdef _WIN32
    WIN32_FIND_DATAW findData;
    string temp(filePath + "\\*.*");
    HANDLE handle = FindFirstFileW(AdaptLongPathW(temp).c_str(), &findData);
    if (handle == INVALID_HANDLE_VALUE) {
        return children;
    }

    do {
        wstring filename(findData.cFileName);
        if (filename == L"." || filename == L"..") {
            continue;
        }
        wstring parentPathW = String2Wstring(filePath);
        string childPath = Wstring2String(parentPathW + L"\\" + filename);
        unique_ptr<FileEntry> f = make_unique<FileEntry>(childPath);
        f->Init();
        children.push_back(move(f));
    } while (FindNextFileW(handle, &findData));
    FindClose(handle);
#else
    DIR *handle = opendir(filePath.c_str());
    if (handle == nullptr) {
        return children;
    }
    struct dirent *entry;
    while ((entry = readdir(handle)) != nullptr) {
        string filename(entry->d_name);
        if (IsIgnore(filename)) {
            continue;
        }

        filePath = filePath_.GetPath() + SEPARATE + filename;
        unique_ptr<FileEntry> f = make_unique<FileEntry>(filePath);
        f->Init();
        children.push_back(move(f));
    }
    closedir(handle);
#endif
    return children;
}

bool FileEntry::IsFile() const
{
    return isFile_;
}

const FileEntry::FilePath &FileEntry::GetFilePath() const
{
    return filePath_;
}

bool FileEntry::Exist(const string &path)
{
#ifdef _WIN32
    if (!PathFileExists(AdaptLongPath(path).c_str())) {
        return PathFileExistsW(AdaptLongPathW(path).c_str());
    }
#else
    struct stat s;
    if (stat(path.c_str(), &s) != 0) {
        return false;
    }
#endif
    return true;
}

bool FileEntry::RemoveAllDir(const string &path)
{
    FileEntry f(path);
    if (!f.Init()) {
        return false;
    }

    if (f.IsFile()) {
        PrintError(GetError(ERR_CODE_REMOVE_FILE_ERROR).FormatCause(path.c_str(), "not directory"));
        return false;
    }
    return RemoveAllDirInner(f);
}

bool FileEntry::RemoveFile(const string &path)
{
    FileEntry f(path);
    if (!f.Init()) {
        return false;
    }
    return RemoveAllDirInner(f);
}

bool FileEntry::CreateDirs(const string &path)
{
    return CreateDirsInner(path, 0);
}

bool FileEntry::CopyFileInner(const string &src, const string &dst)
{
#ifdef _WIN32
    if (!CopyFileW(AdaptLongPathW(src).c_str(), AdaptLongPathW(dst).c_str(), false)) {
        PrintError(GetError(ERR_CODE_COPY_FILE_ERROR).FormatCause(src.c_str(), dst.c_str(), strerror(errno)));
        return false;
    }
    if (!MakeWritable(dst)) {
        cout << "Warning: failed to make file writable: " << dst << endl;
    }
#else
    ifstream in(src, ios::binary);
    ofstream out(dst, ios::binary);
    if (!in || !out) {
        PrintError(GetError(ERR_CODE_COPY_FILE_ERROR).FormatCause(src.c_str(), dst.c_str(), strerror(errno)));
        return false;
    }
    out << in.rdbuf();
    if (!MakeWritable(dst)) {
        cout << "Warning: failed to make file writable: " << dst << endl;
    }
#endif
    return true;
}

bool FileEntry::MakeWritable(const string &path)
{
    if (path.empty()) {
        return false;
    }
#ifdef _WIN32
    DWORD attrs = GetFileAttributesW(AdaptLongPathW(path).c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    if (!(attrs & FILE_ATTRIBUTE_READONLY)) {
        return true;
    }
    return SetFileAttributesW(AdaptLongPathW(path).c_str(), attrs & ~FILE_ATTRIBUTE_READONLY) != 0;
#else
    struct stat s;
    if (stat(path.c_str(), &s) != 0) {
        return false;
    }
    if (s.st_mode & S_IWUSR) {
        return true;
    }
    return chmod(path.c_str(), s.st_mode | S_IWUSR) == 0;
#endif
}

bool FileEntry::IsDirectory(const string &path)
{
#ifdef _WIN32
    if (!PathIsDirectoryW(AdaptLongPathW(path).c_str())) {
        return false;
    }
    return true;
#else
    struct stat s;
    if (stat(path.c_str(), &s) != 0) {
        return false;
    }
    return S_ISDIR(s.st_mode);
#endif
}

string FileEntry::RealPath(const string &path)
{
#ifdef _WIN32
    char buffer[MAX_PATH];
    if (!PathCanonicalize(buffer, path.c_str())) {
        return "";
    }

    if (PathIsRelative(buffer)) {
        char current[MAX_PATH];
        if (!GetCurrentDirectory(MAX_PATH, current)) {
            return "";
        }

        char temp[MAX_PATH];
        if (!PathCombine(temp, current, buffer)) {
            return "";
        }
        if (!Exist(string(temp))) {
            return "";
        }
        return string(temp);
    }
#else
    char buffer[PATH_MAX];
    if (!realpath(path.c_str(), buffer)) {
        return "";
    }
#endif
    if (!Exist(string(buffer))) {
        return "";
    }
    return string(buffer);
}

FileEntry::FilePath::FilePath(const string &path) : filePath_(path)
{
    Format();
    Init();
}

FileEntry::FilePath::~FilePath()
{
}

FileEntry::FilePath FileEntry::FilePath::Append(const string &path)
{
    Format();
    string filePath = filePath_ + SEPARATE + path;
    return FilePath(filePath);
}

FileEntry::FilePath FileEntry::FilePath::ReplaceExtension(const string &extension)
{
    string filePath;
    if (!parent_.empty()) {
        filePath += parent_ + SEPARATE;
    }

    filePath += filename_.substr(0, filename_.length() - extension_.length()) + extension;
    return FilePath(filePath);
}

FileEntry::FilePath FileEntry::FilePath::GetParent()
{
    return FilePath(parent_);
}

const string &FileEntry::FilePath::GetPath() const
{
    return filePath_;
}

const string &FileEntry::FilePath::GetFilename() const
{
    return filename_;
}

const string &FileEntry::FilePath::GetExtension() const
{
    return extension_;
}

const vector<string> FileEntry::FilePath::GetSegments() const
{
    vector<string> segments;
    string::size_type offset = 0;
    string::size_type pos = filePath_.find_first_of(SEPARATE.front(), offset);
    while (pos != string::npos) {
        segments.push_back(filePath_.substr(offset, pos - offset));
        offset = pos + 1;
        pos = filePath_.find_first_of(SEPARATE.front(), offset);
    }

    if (offset < filePath_.length()) {
        segments.push_back(filePath_.substr(offset));
    }
    return segments;
}

// below private
bool FileEntry::IsIgnore(const string &filename) const
{
    if (filename == "." || filename == "..") {
        return true;
    }
    return false;
}

bool FileEntry::RemoveAllDirInner(const FileEntry &entry)
{
    string path = entry.GetFilePath().GetPath();
    if (!Exist(path)) {
        return true;
    }
    if (entry.IsFile()) {
#ifdef _WIN32
        if (!MakeWritable(path)) {
            cout << "Warning: failed to make file writable: " << path << endl;
        }
        bool result = DeleteFileW(AdaptLongPathW(path).c_str());
#else
        if (!MakeWritable(path)) {
            cout << "Warning: failed to make file writable: " << path << endl;
        }
        bool result = remove(path.c_str()) == 0;
#endif
        if (!result) {
            PrintError(GetError(ERR_CODE_REMOVE_FILE_ERROR).FormatCause(path.c_str(), strerror(errno)));
            return false;
        }
        return true;
    }

    for (const auto &iter : entry.GetChilds()) {
        if (!RemoveAllDirInner(*iter)) {
            return false;
        }
    }
#ifdef _WIN32
    bool result = RemoveDirectoryW(AdaptLongPathW(path).c_str());
#else
    bool result = rmdir(path.c_str()) == 0;
#endif
    if (!result) {
        PrintError(GetError(ERR_CODE_REMOVE_FILE_ERROR).FormatCause(path.c_str(), strerror(errno)));
        return false;
    }
    return true;
}

bool FileEntry::CreateDirsInner(const string &path, string::size_type offset)
{
    string::size_type pos = path.find_first_of(SEPARATE.front(), offset);
    if (pos == string::npos) {
#ifdef _WIN32
        return CreateDirectoryW(AdaptLongPathW(path).c_str(), nullptr);
#else
        return mkdir(path.c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == 0;
#endif
    }

    string subPath = path.substr(0, pos + 1);
    if (!Exist(subPath)) {
#ifdef _WIN32
        if (!CreateDirectoryW(AdaptLongPathW(subPath).c_str(), nullptr)) {
#else
        if (mkdir(subPath.c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) != 0) {
#endif
            return false;
        }
    }
    return CreateDirsInner(path, pos + 1);
}

void FileEntry::FilePath::Format()
{
    if (filePath_.back() != SEPARATE.front()) {
        return;
    }
    filePath_.pop_back();
}

void FileEntry::FilePath::Init()
{
    filename_ = filePath_;
    string::size_type pos = filePath_.find_last_of(SEPARATE.front());
    if (pos != string::npos) {
        parent_ = filePath_.substr(0, pos);
        if (pos + 1 < filePath_.length()) {
            filename_ = filePath_.substr(pos + 1);
        }
    }

    pos = filename_.find_last_of('.');
    if (pos != string::npos && pos + 1 < filename_.length()) {
        extension_ = filename_.substr(pos);
    }
}

string FileEntry::AdaptLongPath(const string &path)
{
#ifdef _WIN32
    if (path.size() >= MAX_PATH -12) { //the max file path can not exceed 260 - 12
        return LONG_PATH_HEAD + path;
    }
#endif
    return path;
}

string FileEntry::Utf8ToSysDefault(const std::string &utf8Str)
{
#ifdef _WIN32
    wstring wStr = String2Wstring(utf8Str, CP_UTF8);
    return Wstring2String(wStr, CP_ACP);
#else
    return utf8Str;
#endif
}

#ifdef _WIN32
wstring FileEntry::AdaptLongPathW(const string &path)
{
    wstring longPathW = String2Wstring(path);
    if (longPathW.size() >= MAX_PATH - 12) { // the max file path can not exceed 260 - 12
        return LR"(\\?\)" + longPathW;
    }
    return longPathW;
}

string FileEntry::Wstring2String(const wstring &wstr, const int codePage)
{
    string res;
    int len = WideCharToMultiByte(codePage, 0, wstr.c_str(), wstr.size(), nullptr, 0, nullptr, nullptr);
    if (len <= 0) {
        cout << "Warning: WideCharToMultiByte failed: " << wstr.c_str() << endl;
        return res;
    }
    char *buffer = new char[len + 1];
    if (buffer == nullptr) {
        return res;
    }
    WideCharToMultiByte(codePage, 0, wstr.c_str(), wstr.size(), buffer, len, nullptr, nullptr);
    buffer[len] = '\0';
    res.append(buffer);
    delete[] buffer;
    return res;
}

wstring FileEntry::String2Wstring(const string &str, const int codePage)
{
    wstring res;
    int len = MultiByteToWideChar(codePage, 0, str.c_str(), str.size(), nullptr, 0);
    if (len < 0) {
        cout << "Warning: MultiByteToWideChar failed: " << str.c_str() << endl;
        return res;
    }
    wchar_t *buffer = new wchar_t[len + 1];
    if (buffer == nullptr) {
        return res;
    }
    MultiByteToWideChar(codePage, 0, str.c_str(), str.size(), buffer, len);
    buffer[len] = '\0';
    res.append(buffer);
    delete[] buffer;
    return res;
}
#endif
}
}
}
