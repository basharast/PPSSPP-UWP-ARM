// Copyright (c) 2023- PPSSPP Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0 or later versions.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official git repository and contact information can be found at
// https://github.com/hrydgard/ppsspp and http://www.ppsspp.org/.

#pragma once 

#include <list>
#include <ctime>

#include "StoragePath.h"
#include "StorageInfo.h"
#include "StorageAccess.h"
#include "StoragePickers.h"

// Locations
std::string GetWorkingFolder(); // Where main data is, default is app data
void SetWorkingFolder(std::string location); // Change working location
std::string GetInstallationFolder();
std::string GetLocalFolder();
std::string GetTempFolder();
std::string GetTempFile(std::string name);
std::string GetPicturesFolder(); // Requires 'picturesLibrary' capability
std::string GetVideosFolder(); // Requires 'videosLibrary' capability
std::string GetDocumentsFolder(); // Requires 'documentsLibrary' capability
std::string GetMusicFolder(); // Requires 'musicLibrary' capability
std::string GetPreviewPath(std::string path);
bool isLocalState(std::string path);

// Management
HANDLE CreateFileUWP(std::string path, int accessMode = GENERIC_READ, int shareMode = FILE_SHARE_READ, int openMode = OPEN_EXISTING);
HANDLE CreateFileUWP(std::wstring path, int accessMode = GENERIC_READ, int shareMode = FILE_SHARE_READ, int openMode = OPEN_EXISTING);
FILE* GetFileStream(std::string path, const char* mode);
// `GetFileStreamFromApp` Will use Windows UWP API, use it instead of fopen..etc
FILE* GetFileStreamFromApp(std::string path, const char* mode);
bool IsValidUWP(std::string path, bool allowForAppData = false);
bool IsExistsUWP(std::string path);
bool IsDirectoryUWP(std::string path);

std::list<ItemInfoUWP> GetFolderContents(std::string path, bool deepScan = false);
std::list<ItemInfoUWP> GetFolderContents(std::wstring path, bool deepScan = false);
ItemInfoUWP GetItemInfoUWP(std::string path);

// Basics
int64_t GetSizeUWP(std::string path);
bool DeleteUWP(std::string path);
bool CreateDirectoryUWP(std::string path, bool replaceExisting = true);
bool RenameUWP(std::string path, std::string name);
// Add file name to destination path
bool CopyUWP(std::string path, std::string dest);
// Add file name to destination path
bool MoveUWP(std::string path, std::string dest);

// Helpers
bool OpenFile(std::string path);
bool OpenFolder(std::string path);
bool IsFirstStart();
std::string ResolvePathUWP(std::string path);
bool IsContainsAccessibleItems(std::string path);
bool IsRootForAccessibleItems(std::string path);
// 'checkIfContainsFutureAccessItems' for listing purposes not real access, 'driveName' like C:
bool CheckDriveAccess(std::string driveName, bool checkIfContainsFutureAccessItems);
bool GetDriveFreeSpace(PathUWP path, int64_t& space);

// Log helpers
std::string GetLogFile();
bool SaveLogs(); // With picker
void CleanupLogs();
