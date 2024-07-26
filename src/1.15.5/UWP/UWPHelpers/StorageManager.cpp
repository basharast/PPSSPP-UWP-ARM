// UWP STORAGE MANAGER
// Copyright (c) 2023 Bashar Astifan.
// Email: bashar@astifan.online
// Telegram: @basharastifan
// GitHub: https://github.com/basharast/UWP2Win32

// Functions:
// GetWorkingFolder()
// SetWorkingFolder(std::string location)
// GetInstallationFolder()
// GetLocalFolder()
// GetTempFolder()
// GetPicturesFolder()
// GetVideosFolder()
// GetDocumentsFolder()
// GetMusicFolder()
// GetPreviewPath(std::string path)
//
// CreateFileUWP(std::string path, int accessMode, int shareMode, int openMode)
// CreateFileUWP(std::wstring path, int accessMode, int shareMode, int openMode)
// GetFileStream(std::string path, const char* mode)
// IsValidUWP(std::string path)
// IsExistsUWP(std::string path)
// IsDirectoryUWP(std::string path)
// 
// GetFolderContents(std::string path, T& files)
// GetFolderContents(std::wstring path, T& files)
// GetFileInfoUWP(std::string path, T& info)
//
// GetSizeUWP(std::string path)
// DeleteUWP(std::string path)
// CreateDirectoryUWP(std::string path, bool replaceExisting)
// RenameUWP(std::string path, std::string name)
// CopyUWP(std::string path, std::string name)
// MoveUWP(std::string path, std::string name)
//
// OpenFile(std::string path)
// OpenFolder(std::string path)
// IsFirstStart()
//
// GetLogFile()
// SaveLogs()
// CleanupLogs()

#if defined(_M_ARM) || defined(BUILD14393)
#include "pch.h"
#include <collection.h>
#include <io.h>
#include <fcntl.h>

#include "Common/Log.h"

#include "StorageConfig.h"
#include "StorageManager.h"
#include "StorageExtensions.h"
#include "StorageHandler.h"
#include "StorageAsync.h"
#include "StorageAccess.h"
#include "StorageItemW.h"
#include "StorageLog.h"


using namespace Platform;
using namespace Windows::Storage;
using namespace Windows::Foundation;
using namespace Windows::ApplicationModel;

extern std::list<StorageItemW> FutureAccessItems;

#pragma region Locations
std::string GetWorkingFolder() {
	if (AppWorkingFolder.empty()) {
		return GetLocalFolder();
	}
	else {
		return AppWorkingFolder;
	}
}
void SetWorkingFolder(std::string location) {
	AppWorkingFolder = location;
}
std::string GetInstallationFolder() {
	return convert(Package::Current->InstalledLocation->Path);
}
StorageFolder^ GetLocalStorageFolder() {
	return ApplicationData::Current->LocalFolder;
}
std::string GetLocalFolder() {
	return convert(GetLocalStorageFolder()->Path);
}
std::string GetTempFolder() {
	return convert(ApplicationData::Current->TemporaryFolder->Path);
}
std::string GetTempFile(std::string name) {
	StorageFile^ tmpFile;
	ExecuteTask(tmpFile, ApplicationData::Current->TemporaryFolder->CreateFileAsync(convert(name), CreationCollisionOption::GenerateUniqueName));
	if (tmpFile != nullptr) {
		return convert(tmpFile->Path);
	}
	else {
		return "";
	}
}
std::string GetPicturesFolder() {
	// Requires 'picturesLibrary' capability
	return convert(KnownFolders::PicturesLibrary->Path);
}
std::string GetVideosFolder() {
	// Requires 'videosLibrary' capability
	return convert(KnownFolders::VideosLibrary->Path);
}
std::string GetDocumentsFolder() {
	// Requires 'documentsLibrary' capability
	return convert(KnownFolders::DocumentsLibrary->Path);
}
std::string GetMusicFolder() {
	// Requires 'musicLibrary' capability
	return convert(KnownFolders::MusicLibrary->Path);
}
std::string GetPreviewPath(std::string path) {
	std::string pathView = path;
	windowsPath(pathView);
	std::string currentMemoryStick = GetWorkingFolder();
	windowsPath(currentMemoryStick);

	// Ensure memStick sub path replaced by 'ms:'
	replace(pathView, currentMemoryStick + "\\", "ms:\\");
	std::string appData = GetLocalFolder();
	replace(appData, "\\LocalState", "");
	replace(pathView, appData, "AppData");
	return pathView;
}
bool isLocalState(std::string path) {
	return iequals(GetPreviewPath(path), "LocalState");
}
#pragma endregion

#pragma region Internal
PathUWP PathResolver(PathUWP path) {
	auto root = path.GetDirectory();
	auto newPath = path.ToString();
	if (path.IsRoot() || iequals(root, "/") || iequals(root, "\\")) {
		// System requesting file from app data
		replace(newPath, "/", (GetLocalFolder() + (path.size() > 1 ? "/" : "")));
	}
	path = PathUWP(newPath);
	return path;
}
PathUWP PathResolver(std::string path) {
	return PathResolver(PathUWP(path));
}

std::string ResolvePathUWP(std::string path) {
	return PathResolver(path).ToString();
}

// Return closer parent
StorageItemW GetStorageItemParent(PathUWP path) {
	path = PathResolver(path);
	StorageItemW parent;

	for (auto& fItem : FutureAccessItems) {
		if (isChild(fItem.GetPath(), path.ToString())) {
			if (fItem.IsDirectory()) {
				VERBOSE_LOG(FILESYS, "Parent folder found (%s)", fItem.GetName().c_str());
				parent = fItem;
				break;
			}
		}
	}

	return parent;
}

StorageItemW GetStorageItem(PathUWP path, bool createIfNotExists = false, bool forceFolderType = false) {
	// Fill call will be ignored internally after the first call
	FillLookupList();

	path = PathResolver(path);
	StorageItemW item;
	/*try {
		StorageFile^ testFile;
		std::string pathStr = path.ToString();
		windowsPath(pathStr);
		ExecuteTask(testFile, StorageFile::GetFileFromPathAsync(convert(pathStr)));
		if (testFile != nullptr) {
			item = StorageItemW(testFile);
		}
	}
	catch (...) {

	}

	try {
		if (!item.IsValid()) {
			StorageFolder^ testFolder;
			std::string pathStr = path.ToString();
			windowsPath(pathStr);
			ExecuteTask(testFolder, StorageFolder::GetFolderFromPathAsync(convert(pathStr)));
			if (testFolder != nullptr) {
				item = StorageItemW(testFolder);
			}
		}
	}
	catch (...) {

	}*/
	//if (!item.IsValid()) 
	{
		// Look for match in FutureAccessItems
		for (auto& fItem : FutureAccessItems) {
			if (fItem.Equal(path)) {
				item = fItem;
				break;
			}
		}

		if (!item.IsValid()) {
			// Look for match inside FutureAccessFolders
			for (auto& fItem : FutureAccessItems) {
				if (fItem.IsDirectory()) {
					IStorageItem^ storageItem;
					if (fItem.Contains(path, storageItem)) {
						item = StorageItemW(storageItem);
						break;
					}
				}
			}
		}

		if (!item.IsValid() && createIfNotExists) {
			VERBOSE_LOG(FILESYS, "File not in our lists, creating new one");

			// Create and return new folder
			auto parent = GetStorageItemParent(path);
			if (parent.IsValid()) {
				if (!forceFolderType) {
					// File creation must be called in this case
					// Create folder usually will be called from 'CreateDirectory'
					item = StorageItemW(parent.CreateFile(path));
				}
				else {
					item = StorageItemW(parent.CreateFolder(path));
				}
			}
		}
	}
	return item;
}

StorageItemW GetStorageItem(std::string path, bool createIfNotExists = false, bool forceFolderType = false) {
	return GetStorageItem(PathUWP(path), createIfNotExists, forceFolderType);
}

std::list<StorageItemW> GetStorageItemsByParent(PathUWP path) {
	path = PathResolver(path);
	std::list<StorageItemW> items;

	// Look for match in FutureAccessItems
	for (auto& fItem : FutureAccessItems) {
		if (isParent(path.ToString(), fItem.GetPath(), fItem.GetName())) {
			items.push_back(fItem);
		}
	}

	return items;
}

std::list<StorageItemW> GetStorageItemsByParent(std::string path) {
	return GetStorageItemsByParent(PathUWP(path));
}

bool IsContainsAccessibleItems(PathUWP path) {
	path = PathResolver(path);

	for (auto& fItem : FutureAccessItems) {
		if (isParent(path.ToString(), fItem.GetPath(), fItem.GetName())) {
			return true;
		}
	}

	return false;
}

bool IsContainsAccessibleItems(std::string path) {
	return IsContainsAccessibleItems(PathUWP(path));
}
 
bool IsRootForAccessibleItems(PathUWP path, std::list<std::string>& subRoot, bool breakOnFirstMatch = false) {
	path = PathResolver(path);

	for (auto& fItem : FutureAccessItems) {
		if (isChild(path.ToString(), fItem.GetPath())) {
			if (breakOnFirstMatch) {
				// Just checking, we don't need to loop for each item
				return true;
			}
			auto sub = getSubRoot(path.ToString(), fItem.GetPath());

			// This check can be better, but that's how I can do it in C++
			if (!ends_with(sub, ":")) {
				bool alreadyAdded = false;
				for each (auto sItem in subRoot) {
					if (iequals(sItem, sub)) {
						alreadyAdded = true;
						break;
					}
				}
				if (!alreadyAdded) {
					subRoot.push_back(sub);
				}
			}
		}
	}
	return !subRoot.empty();
}

bool IsRootForAccessibleItems(std::string path, std::list<std::string>& subRoot, bool breakOnFirstMatch = false) {
	return IsRootForAccessibleItems(PathUWP(path), subRoot, breakOnFirstMatch);
}
bool IsRootForAccessibleItems(std::string path) {
	std::list<std::string> tmp;
	return IsRootForAccessibleItems(path, tmp, true);
}
#pragma endregion

#pragma region Functions
bool CreateIfNotExists(int openMode) {
	switch (openMode)
	{
	case OPEN_ALWAYS:
	case CREATE_ALWAYS:
	case CREATE_NEW:
		return true;
	default:
		return false;
	}
}

HANDLE CreateFileUWP(std::string path, int accessMode, int shareMode, int openMode) {
	HANDLE handle = INVALID_HANDLE_VALUE;

	if (IsValidUWP(path)) {
		bool createIfNotExists = CreateIfNotExists(openMode);
		DEBUG_LOG(FILESYS, "Getting handle (%s)", createIfNotExists ? "CreateIfNotExists" : "DontCreateIfNotExists");
		auto storageItem = GetStorageItem(path, createIfNotExists);

		if (storageItem.IsValid()) {
			DEBUG_LOG(FILESYS, "Getting handle (%s)", path.c_str());
			HRESULT hr = storageItem.GetHandle(&handle, accessMode, shareMode);
			if (hr == E_FAIL) {
				handle = INVALID_HANDLE_VALUE;
			}
		}
		else {
			handle = INVALID_HANDLE_VALUE;
			DEBUG_LOG(FILESYS, "Couldn't find or access (%s)", path.c_str());
		}
	}
	return handle;
}
extern HANDLE(WINAPI* CreateFile2Ptr)(LPCWSTR, DWORD, DWORD, DWORD, LPCREATEFILE2_EXTENDED_PARAMETERS);
extern BOOL(WINAPI* DeleteFileWPtr)(LPCWSTR);

HANDLE CreateFileUWP(std::wstring path, int accessMode, int shareMode, int openMode) {
	auto pathString = convert(path);
	return CreateFileUWP(pathString, accessMode, shareMode, openMode);
}

std::map<std::string, bool> accessState;
bool CheckDriveAccess(std::string driveName, bool checkIfContainsFutureAccessItems) {
	bool state = false;

	auto keyIter = accessState.find(driveName);
	if (keyIter != accessState.end()) {
		state = keyIter->second;
	}
	else {
		try {
			auto dwDesiredAccess = GENERIC_READ | GENERIC_WRITE;
			auto dwShareMode = FILE_SHARE_READ | FILE_SHARE_WRITE;
			auto dwCreationDisposition = CREATE_ALWAYS;

			auto testFile = std::string(driveName);
			testFile.append("\\.PPSSPPAccessCheck");
#if defined(_M_ARM) || defined(BUILD14393)
			HANDLE h = CreateFile2(convertToLPCWSTR(testFile), dwDesiredAccess, dwShareMode, dwCreationDisposition, nullptr);
#else
			HANDLE h = CreateFile2FromAppW(convertToLPCWSTR(testFile), dwDesiredAccess, dwShareMode, dwCreationDisposition, nullptr);
#endif
			if (h != INVALID_HANDLE_VALUE) {
				state = true;
				CloseHandle(h);
#if defined(_M_ARM) || defined(BUILD14393)
				DeleteFileW(convertToLPCWSTR(testFile));
#else
				DeleteFileFromAppW(convertToLPCWSTR(testFile));
#endif
			}
			accessState.insert(std::make_pair(driveName, state));
		}
		catch (...) {
		}
	}

	if (!state && checkIfContainsFutureAccessItems) {
		// Consider the drive accessible in case it contain files/folder selected before to avoid empty results
		state = IsRootForAccessibleItems(driveName) || IsContainsAccessibleItems(driveName);
	}
	return state;
}
bool IsValidUWP(std::string path, bool allowForAppData) {
	auto p = PathResolver(path);

	//Check valid path
	if (p.Type() == PathTypeUWP::UNDEFINED || !p.IsAbsolute()) {
		// Nothing to do here
		VERBOSE_LOG(FILESYS, "File is not valid (%s)", p.ToString().c_str());
		return false;
	}


	bool state = false;

	if (!allowForAppData) {
		auto resolvedPathStr = p.ToString();
		if (ends_with(resolvedPathStr, "LocalState") || ends_with(resolvedPathStr, "TempState") || ends_with(resolvedPathStr, "LocalCache")) {
			state = true;
		}
		else
			if (isChild(GetLocalFolder(), resolvedPathStr)) {
				state = true;
			}
			else if (isChild(GetInstallationFolder(), resolvedPathStr)) {
				state = true;
			}
			else if (isChild(GetTempFolder(), resolvedPathStr)) {
				state = true;
			}

		if (!state)
		{
			auto p = PathUWP(path);
			std::string driveName = p.GetRootVolume().ToString();
			state = CheckDriveAccess(driveName, false);
		}
	}
	return !state;
}

bool IsExistsUWP(std::string path) {
	if (IsValidUWP(path)) {
		auto storageItem = GetStorageItem(path);
		if (storageItem.IsValid()) {
			return true;
		}

		// If folder is not accessible but contains accessible items
		// consider it exists
		if (IsContainsAccessibleItems(path)) {
			return true;
		}

		// If folder is not accessible but is part of accessible items
		// consider it exists
		std::list<std::string> tmp;
		if (IsRootForAccessibleItems(path, tmp, true)) {
			return true;
		}
	}
	// ERROR_LOG(FILESYS, "Couldn't find or access (%s)", path.c_str());
	return false;
}

bool IsDirectoryUWP(std::string path) {
	if (IsValidUWP(path)) {
		auto storageItem = GetStorageItem(path);
		if (storageItem.IsValid()) {
			if (storageItem.IsDirectory()) {
				return true;
			}
		}
	}
	return false;
}

FILE* GetFileStream(std::string path, const char* mode) {
	FILE* file{};
	if (IsValidUWP(path)) {
		auto storageItem = GetStorageItem(path);
		if (storageItem.IsValid()) {
			file = storageItem.GetStream(mode);
		}
		else {
			// Forward the request to parent folder
			auto p = PathUWP(path);
			auto itemName = p.GetFilename();
			auto rootPath = p.GetDirectory();
			if (IsValidUWP(rootPath)) {
				storageItem = GetStorageItem(rootPath);
				if (storageItem.IsValid()) {
					file = storageItem.GetFileStream(itemName, mode);
				}
				else {
					ERROR_LOG(FILESYS, "Couldn't find or access (%s)", rootPath.c_str());
					ERROR_LOG(FILESYS, "Couldn't find or access (%s)", path.c_str());
				}
			}
		}
	}

	return file;
}

FILE* GetFileStreamFromApp(std::string path, const char* mode) {

	FILE* file{};

	auto pathResolved = PathUWP(ResolvePathUWP(path));
	HANDLE handle = INVALID_HANDLE_VALUE;

	auto fileMode = GetFileMode(mode);
	if (fileMode) {
		handle = CreateFile2(pathResolved.ToWString().c_str(), fileMode->dwDesiredAccess, fileMode->dwShareMode, fileMode->dwCreationDisposition, nullptr);
	}
	if (handle != INVALID_HANDLE_VALUE) {
		file = _fdopen(_open_osfhandle((intptr_t)handle, fileMode->flags), mode);
	}

	return file;
}

#pragma region Content Helpers
ItemInfoUWP GetFakeFolderInfo(std::string folder) {
	ItemInfoUWP info;
	auto folderPath = PathUWP(folder);
	info.name = folderPath.GetFilename();
	info.fullName = folderPath.ToString();

	info.isDirectory = true;

	info.size = 1;
	info.lastAccessTime = 1000;
	info.lastWriteTime = 1000;
	info.changeTime = 1000;
	info.creationTime = 1000;

	info.attributes = FILE_ATTRIBUTE_DIRECTORY;

	return info;
}

#pragma endregion

std::list<ItemInfoUWP> GetFolderContents(std::string path, bool deepScan) {
	std::list<ItemInfoUWP> contents;

	if (IsValidUWP(path)) {
		auto storageItem = GetStorageItem(path);
		if (storageItem.IsValid()) {

			// Files
			// deepScan is slow, try to avoid it
			auto rfiles = deepScan ? storageItem.GetAllFiles() : storageItem.GetFiles();
			for each (auto file in rfiles) {
				contents.push_back(file.GetFileInfo());
			}

			// Folders
			// deepScan is slow, try to avoid it
			auto rfolders = deepScan ? storageItem.GetAllFolders() : storageItem.GetFolders();
			for each (auto folder in rfolders) {
				contents.push_back(folder.GetFolderInfo());
			}
		}
		else {
			DEBUG_LOG(FILESYS, "Cannot get contents!, checking for other options.. (%s)", path.c_str());
		}
	}

	if (contents.size() == 0) {
		// Folder maybe not accessible or not exists
			// if not accessible, maybe some items inside it were selected before
			// and they already in our accessible list
		if (IsContainsAccessibleItems(path)) {
			DEBUG_LOG(FILESYS, "Folder contains accessible items (%s)", path.c_str());

			// Check contents
			auto cItems = GetStorageItemsByParent(path);
			if (!cItems.empty()) {
				for each (auto item in cItems) {
					VERBOSE_LOG(FILESYS, "Appending accessible item (%s)", item.GetPath().c_str());
					contents.push_back(item.GetItemInfo());
				}
			}
		}
		else
		{
			// Check if this folder is root for accessible item
			// then add fake folder as sub root to avoid empty results
			std::list<std::string> subRoot;
			if (IsRootForAccessibleItems(path, subRoot)) {
				DEBUG_LOG(FILESYS, "Folder is root for accessible items (%s)", path.c_str());

				if (!subRoot.empty()) {
					for each (auto sItem in subRoot) {
						VERBOSE_LOG(FILESYS, "Appending fake folder (%s)", sItem.c_str());
						contents.push_back(GetFakeFolderInfo(sItem));
					}
				}
			}
			else {
				VERBOSE_LOG(FILESYS, "Cannot get any content!.. (%s)", path.c_str());
			}
		}
	}
	return contents;
}
std::list<ItemInfoUWP> GetFolderContents(std::wstring path, bool deepScan) {
	return GetFolderContents(convert(path), deepScan);
}

ItemInfoUWP GetItemInfoUWP(std::string path) {
	ItemInfoUWP info;
	info.size = -1;
	info.attributes = INVALID_FILE_ATTRIBUTES;

	if (IsValidUWP(path)) {
		auto storageItem = GetStorageItem(path);
		if (storageItem.IsValid()) {
			info = storageItem.GetItemInfo();
		}
		else {
			ERROR_LOG(FILESYS, "Couldn't find or access (%s)", path.c_str());
		}
	}
	return info;
}
#pragma endregion

#pragma region Basics
int64_t GetSizeUWP(std::string path) {
	int64_t size = 0;
	if (IsValidUWP(path)) {
		auto storageItem = GetStorageItem(path);
		if (storageItem.IsValid()) {
			size = storageItem.GetSize();
		}
		else {
			ERROR_LOG(FILESYS, "Couldn't find or access (%s)", path.c_str());
		}
	}
	return size;
}

bool DeleteUWP(std::string path) {
	bool state = false;
	if (IsValidUWP(path)) {
		auto storageItem = GetStorageItem(path);
		if (storageItem.IsValid()) {
			DEBUG_LOG(FILESYS, "Delete (%s)", path.c_str());
			state = storageItem.Delete();
		}
		else {
			DEBUG_LOG(FILESYS, "Couldn't find or access (%s)", path.c_str());
		}
	}

	return state;
}

bool CreateDirectoryUWP(std::string path, bool replaceExisting) {
	bool state = false;
	auto p = PathUWP(path);
	auto itemName = p.GetFilename();
	auto rootPath = p.GetDirectory();

	if (IsValidUWP(rootPath)) {
		auto storageItem = GetStorageItem(rootPath);
		if (storageItem.IsValid()) {
			DEBUG_LOG(FILESYS, "Create new folder (%s)", path.c_str());
			state = storageItem.CreateFolder(itemName, replaceExisting);
		}
		else {
			ERROR_LOG(FILESYS, "Couldn't find or access (%s)", rootPath.c_str());
		}
	}

	return state;
}

bool CopyUWP(std::string path, std::string dest) {
	bool state = false;

	if (IsValidUWP(path, true) && IsValidUWP(dest, true)) {
		auto srcStorageItem = GetStorageItem(path);
		if (srcStorageItem.IsValid()) {
			auto destDir = dest;
			auto srcName = srcStorageItem.GetName();
			auto dstPath = PathUWP(dest);
			auto dstName = dstPath.GetFilename();
			// Destination must be parent folder
			destDir = dstPath.GetDirectory();
			auto dstStorageItem = GetStorageItem(destDir, true, true);
			if (dstStorageItem.IsValid()) {
				DEBUG_LOG(FILESYS, "Copy (%s) to (%s)", path.c_str(), dest.c_str());
				state = srcStorageItem.Copy(dstStorageItem, dstName);
			}
			else {
				ERROR_LOG(FILESYS, "Couldn't find or access (%s)", dest.c_str());
			}
		}
		else {
			ERROR_LOG(FILESYS, "Couldn't find or access (%s)", path.c_str());
		}
	}

	return state;
}

bool MoveUWP(std::string path, std::string dest) {
	bool state = false;

	if (IsValidUWP(path, true) && IsValidUWP(dest, true)) {
		auto srcStorageItem = GetStorageItem(path);

		if (srcStorageItem.IsValid()) {
			auto destDir = dest;
			auto srcName = srcStorageItem.GetName();
			auto dstPath = PathUWP(dest);
			auto dstName = dstPath.GetFilename();
			// Destination must be parent folder
			destDir = dstPath.GetDirectory();
			auto dstStorageItem = GetStorageItem(destDir, true, true);
			if (dstStorageItem.IsValid()) {
				DEBUG_LOG(FILESYS, "Move (%s) to (%s)", path.c_str(), dest.c_str());
				state = srcStorageItem.Move(dstStorageItem, dstName);
			}
			else {
				ERROR_LOG(FILESYS, "Couldn't find or access (%s)", dest.c_str());
			}
		}
		else {
			ERROR_LOG(FILESYS, "Couldn't find or access (%s)", path.c_str());
		}
	}

	return state;
}

bool RenameUWP(std::string path, std::string name) {
	bool state = false;

	auto srcRoot = PathUWP(path).GetDirectory();
	auto dstRoot = PathUWP(name).GetDirectory();
	// Check if system using rename to move
	if (iequals(srcRoot, dstRoot)) {
		auto srcStorageItem = GetStorageItem(path);
		if (srcStorageItem.IsValid()) {
			DEBUG_LOG(FILESYS, "Rename (%s) to (%s)", path.c_str(), name.c_str());
			state = srcStorageItem.Rename(name);
		}
		else {
			DEBUG_LOG(FILESYS, "Couldn't find or access (%s)", path.c_str());
		}
	}
	else {
		DEBUG_LOG(FILESYS, " Rename used as move -> call move (%s) to (%s)", path.c_str(), name.c_str());
		state = MoveUWP(path, name);
	}

	return state;
}
#pragma endregion


#pragma region Helpers
bool OpenFile(std::string path) {
	bool state = false;

	auto storageItem = GetStorageItem(path);
	if (storageItem.IsValid()) {
		if (!storageItem.IsDirectory()) {
			ExecuteTask(state, Windows::System::Launcher::LaunchFileAsync(storageItem.GetStorageFile()), false);
		}
	}
	else {
		auto uri = ref new Windows::Foundation::Uri(convert(path));

		ExecuteTask(state, Windows::System::Launcher::LaunchUriAsync(uri), false);
	}
	return state;
}

bool OpenFolder(std::string path) {
	bool state = false;
	PathUWP itemPath(path);
	Platform::String^ wString = ref new Platform::String(itemPath.ToWString().c_str());
	StorageFolder^ storageItem;
	ExecuteTask(storageItem, StorageFolder::GetFolderFromPathAsync(wString));
	if (storageItem != nullptr) {
		ExecuteTask(state, Windows::System::Launcher::LaunchFolderAsync(storageItem), false);
	}
	else {
		// Try as it's file
		PathUWP parent = PathUWP(itemPath.GetDirectory());
		Platform::String^ wParentString = ref new Platform::String(parent.ToWString().c_str());

		ExecuteTask(storageItem, StorageFolder::GetFolderFromPathAsync(wParentString));
		if (storageItem != nullptr) {
			ExecuteTask(state, Windows::System::Launcher::LaunchFolderAsync(storageItem), false);
		}
	}
	return state;
}


bool GetDriveFreeSpace(PathUWP path, int64_t& space) {

	bool state = false;
	Platform::String^ wString = ref new Platform::String(path.ToWString().c_str());
	StorageFolder^ storageItem;
	ExecuteTask(storageItem, StorageFolder::GetFolderFromPathAsync(wString));
	if (storageItem != nullptr) {
		Platform::String^ freeSpaceKey = ref new Platform::String(L"System.FreeSpace");
		Platform::Collections::Vector<Platform::String^>^ propertiesToRetrieve = ref new Platform::Collections::Vector<Platform::String^>();
		propertiesToRetrieve->Append(freeSpaceKey);
		Windows::Foundation::Collections::IMap<Platform::String^, Platform::Object^>^ result;
		ExecuteTask(result, storageItem->Properties->RetrievePropertiesAsync(propertiesToRetrieve));
		if (result != nullptr && result->Size > 0) {
			try {
				auto value = result->Lookup(L"System.FreeSpace");
				space = (uint64_t)value;
				state = true;
			}
			catch (...) {

			}
		}
	}

	return state;
}

bool IsFirstStart() {
	auto firstrun = GetDataFromLocalSettings("first_run");
	AddDataToLocalSettings("first_run", "done", true);
	return firstrun.empty();
}
#pragma endregion

#pragma region Logs
// Get log file name
std::string currentLogFile;
std::string getLogFileName() {
	//Initial new name each session/launch
	if (currentLogFile.empty() || currentLogFile.size() == 0) {
		std::time_t now = std::time(0);
		char mbstr[100];
		std::strftime(mbstr, 100, "ppsspp %d-%m-%Y (%T).txt", std::localtime(&now));
		std::string formatedDate(mbstr);
		std::replace(formatedDate.begin(), formatedDate.end(), ':', '-');
		currentLogFile = formatedDate;
	}

	return currentLogFile;
}

// Get current log file location
StorageFolder^ GetLogsStorageFolder() {
	// Ensure 'LOGS' folder is created
	auto workingFolder = GetStorageItem(GetWorkingFolder());
	StorageFolder^ logsFolder;
	if (workingFolder.IsValid()) {
		auto workingStorageFolder = workingFolder.GetStorageFolder();
		ExecuteTask(logsFolder, workingStorageFolder->CreateFolderAsync("PSP", CreationCollisionOption::OpenIfExists));
	}
	return logsFolder;
}
std::string GetLogFile() {
	std::string logFile;
	PathUWP logFilePath = PathUWP(GetWorkingFolder() + "\\PSP\\ppsspp.txt");
	HANDLE h = CreateFile2(logFilePath.ToWString().c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, CREATE_ALWAYS, nullptr);
	if (h != INVALID_HANDLE_VALUE) {
		logFile = logFilePath.ToString();
		CloseHandle(h);
	}
	else {
		auto logDir  = GetWorkingFolder() + "\\PSP";
		StorageFolder^ sFolder = GetLogsStorageFolder();
		if (sFolder != nullptr) {
			StorageItemW wItem = StorageItemW(sFolder);
			if (wItem.IsValid()) {
				if (wItem.CreateFile("ppsspp.txt", true)) {
					logFile = logFilePath.ToString();
				}
			}
		}
	}

	if (logFile.empty()) {
		// Force local state
		logFilePath = PathUWP(GetLocalFolder() + "\\PSP\\ppsspp.txt");
		h = CreateFile2(logFilePath.ToWString().c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, CREATE_ALWAYS, nullptr);
		if (h != INVALID_HANDLE_VALUE) {
			logFile = logFilePath.ToString();
			CloseHandle(h);
		}
	}

	return logFile;
}

// Save logs to folder selected by the user
bool SaveLogs() {
	try {
		auto folderPicker = ref new Windows::Storage::Pickers::FolderPicker();
		folderPicker->SuggestedStartLocation = Windows::Storage::Pickers::PickerLocationId::Desktop;
		folderPicker->FileTypeFilter->Append("*");

		StorageFolder^ saveFolder;
		ExecuteTask(saveFolder, folderPicker->PickSingleFolderAsync());

		if (saveFolder != nullptr) {
			StorageFolder^ logsFolder = GetLogsStorageFolder();

			if (logsFolder != nullptr) {
				StorageFolderW logsCache(logsFolder);
				logsCache.Copy(saveFolder);
			}
		}
	}
	catch (...) {
		return false;
	}
	return true;
}

void CleanupLogs() {
	StorageFolder^ logsFolder = GetLogsStorageFolder();
	if (logsFolder != nullptr) {
		StorageFolderW logsCache(logsFolder);
		std::list<StorageFileW> files = logsCache.GetFiles();
		if (!files.empty()) {
			for each (auto fItem in files) {
				if (fItem.GetSize() == 0) {
					fItem.Delete();
				}
			}
		}
	}
}
#pragma endregion

#else

#include "pch.h"
#include <io.h>
#include <fcntl.h>
#include <collection.h>


#include "Common/Log.h"
#include "Core/Config.h"
#include "Common/File/Path.h"
#include "Common/StringUtils.h"
#include "UWPUtil.h"

#include "StorageManager.h"
#include "StorageAsync.h"
#include "StorageAccess.h"


using namespace Platform;
using namespace Windows::Storage;
using namespace Windows::Foundation;
using namespace Windows::ApplicationModel;


#pragma region Locations
std::string GetWorkingFolder() {
	if (g_Config.memStickDirectory.empty()) {
		return g_Config.internalDataDirectory.ToString();
	}
	else {
		return g_Config.memStickDirectory.ToString();
	}
}
std::string GetInstallationFolder() {
	return FromPlatformString(Package::Current->InstalledLocation->Path);
}
StorageFolder^ GetLocalStorageFolder() {
	return ApplicationData::Current->LocalFolder;
}
std::string GetLocalFolder() {
	return FromPlatformString(GetLocalStorageFolder()->Path);
}
std::string GetTempFolder() {
	return FromPlatformString(ApplicationData::Current->TemporaryFolder->Path);
}
std::string GetTempFile(std::string name) {
	StorageFile^ tmpFile;
	ExecuteTask(tmpFile, ApplicationData::Current->TemporaryFolder->CreateFileAsync(ToPlatformString(name), CreationCollisionOption::GenerateUniqueName));
	if (tmpFile != nullptr) {
		return FromPlatformString(tmpFile->Path);
	}
	else {
		return "";
	}
}
std::string GetPicturesFolder() {
	// Requires 'picturesLibrary' capability
	return FromPlatformString(KnownFolders::PicturesLibrary->Path);
}
std::string GetVideosFolder() {
	// Requires 'videosLibrary' capability
	return FromPlatformString(KnownFolders::VideosLibrary->Path);
}
std::string GetDocumentsFolder() {
	// Requires 'documentsLibrary' capability
	return FromPlatformString(KnownFolders::DocumentsLibrary->Path);
}
std::string GetMusicFolder() {
	// Requires 'musicLibrary' capability
	return FromPlatformString(KnownFolders::MusicLibrary->Path);
}
std::string GetPreviewPath(std::string path) {

	std::string pathView = path;
	pathView = ReplaceAll(pathView, "/", "\\");
	std::string currentMemoryStick = ConvertWStringToUTF8(g_Config.memStickDirectory.ToWString());
	// Ensure memStick sub path replaced by 'ms:'
	pathView = ReplaceAll(pathView, currentMemoryStick + "\\", "ms:\\");
	auto appData = ReplaceAll(GetLocalFolder(), "\\LocalState", "");
	pathView = ReplaceAll(pathView, appData, "AppData");

	return pathView;
}
bool isLocalState(std::string path) {
	return !_stricmp(GetPreviewPath(path).c_str(), "LocalState");
}
#pragma endregion

#pragma region Internal
Path PathResolver(Path path) {
	auto root = path.GetDirectory();
	auto newPath = path.ToString();
	if (path.IsRoot() || !_stricmp(root.c_str(), "/") || !_stricmp(root.c_str(), "\\")) {
		// System requesting file from app data
		newPath = ReplaceAll(newPath, "/", (GetLocalFolder() + (path.size() > 1 ? "/" : "")));
	}
	return Path(newPath);
}
Path PathResolver(std::string path) {
	return PathResolver(Path(path));
}

std::string ResolvePathUWP(std::string path) {
	return PathResolver(path).ToString();
}
#pragma endregion

#pragma region Functions
std::map<std::string, bool> accessState;
bool CheckDriveAccess(std::string driveName) {
	bool state = false;

	auto keyIter = accessState.find(driveName);
	if (keyIter != accessState.end()) {
		state = keyIter->second;
	}
	else {
		try {
			auto dwDesiredAccess = GENERIC_READ | GENERIC_WRITE;
			auto dwShareMode = FILE_SHARE_READ | FILE_SHARE_WRITE;
			auto dwCreationDisposition = CREATE_ALWAYS;

			auto testFile = std::string(driveName);
			testFile.append("\\.PPSSPPAccessCheck");
#if defined(_M_ARM) || defined(BUILD14393)
			HANDLE h = CreateFile2(convertToLPCWSTR(testFile), dwDesiredAccess, dwShareMode, dwCreationDisposition, nullptr);
#else
			HANDLE h = CreateFile2FromAppW(convertToLPCWSTR(testFile), dwDesiredAccess, dwShareMode, dwCreationDisposition, nullptr);
#endif
			if (h != INVALID_HANDLE_VALUE) {
				state = true;
				CloseHandle(h);
#if defined(_M_ARM) || defined(BUILD14393)
				DeleteFileW(convertToLPCWSTR(testFile));
#else
				DeleteFileFromAppW(convertToLPCWSTR(testFile));
#endif
			}
			accessState.insert(std::make_pair(driveName, state));
		}
		catch (...) {
		}
	}
	if (!state) {
		state = IsRootForAccessibleItems(driveName);
	}
	return state;
}

FILE* GetFileStreamFromApp(std::string path, const char* mode) {

	FILE* file{};

	auto pathResolved = Path(ResolvePathUWP(path));
	HANDLE handle = INVALID_HANDLE_VALUE;

	auto fileMode = GetFileMode(mode);

	if (fileMode) {
		handle = CreateFile2FromAppW(pathResolved.ToWString().c_str(), fileMode->dwDesiredAccess, fileMode->dwShareMode, fileMode->dwCreationDisposition, nullptr);
	}
	if (handle != INVALID_HANDLE_VALUE) {
		file = _fdopen(_open_osfhandle((intptr_t)handle, fileMode->flags), mode);
	}

	return file;
}
#pragma endregion

#pragma region FakeFolders
// Parent and child full path
std::string getSubRoot(std::string parent, std::string child) {
	auto childCut = child;
	childCut = ReplaceAll(childCut, (parent + "/"), "");
	size_t len = childCut.find_first_of('/', 0);
	auto subRoot = childCut.substr(0, len);

	return parent + "/" + subRoot;
}

bool isChild(std::string parent, std::string child) {
	return child.find(parent) != std::string::npos;
}

// Parent full path, child full path, child name only
bool isParent(std::string parent, std::string child, std::string childName) {
	parent.append("/" + childName);
	return parent == child;
}

bool IsRootForAccessibleItems(Path path, std::list<std::string>& subRoot, bool breakOnFirstMatch = false) {
	path = PathResolver(path);
	auto FutureAccessItems = GetFutureAccessList();
	for (auto& fItem : FutureAccessItems) {
		if (isChild(path.ToString(), fItem)) {
			if (breakOnFirstMatch) {
				// Just checking, we don't need to loop for each item
				return true;
			}
			auto sub = getSubRoot(path.ToString(), fItem);

			// This check can be better, but that's how I can do it in C++
			if (!endsWith(sub, ":")) {
				bool alreadyAdded = false;
				for each (auto sItem in subRoot) {
					if (!strcmp(sItem.c_str(), sub.c_str())) {
						alreadyAdded = true;
						break;
					}
				}
				if (!alreadyAdded) {
					subRoot.push_back(sub);
				}
			}
		}
	}
	return !subRoot.empty();
}
bool IsRootForAccessibleItems(std::string path)
{
	std::list<std::string> tmp;
	return IsRootForAccessibleItems(Path(path), tmp, true);
}

bool GetFakeFolders(Path path, std::vector<File::FileInfo>* files, const char* filter, std::set<std::string> filters) {
	bool state = false;
	std::list<std::string> subRoot;
	if (IsRootForAccessibleItems(path, subRoot)) {
		if (!subRoot.empty()) {
			for each (auto sItem in subRoot) {
				auto folderPath = Path(sItem);
				auto attributes = FILE_ATTRIBUTE_DIRECTORY;
				File::FileInfo info;
				info.name = folderPath.GetFilename();
				info.fullName = folderPath;
				info.exists = true;
				info.size = 1;
				info.isDirectory = true;
				info.isWritable = (attributes & FILE_ATTRIBUTE_READONLY) == 0;
				info.atime = 1000;
				info.mtime = 1000;
				info.ctime = 1000;
				if (attributes & FILE_ATTRIBUTE_READONLY) {
					info.access = 0444;  // Read
				}
				else {
					info.access = 0666;  // Read/Write
				}
				if (attributes & FILE_ATTRIBUTE_DIRECTORY) {
					info.access |= 0111;  // Execute
				}
				if (!info.isDirectory) {
					std::string ext = info.fullName.GetFileExtension();
					if (!ext.empty()) {
						ext = ext.substr(1);  // Remove the dot.
						if (filter && filters.find(ext) == filters.end()) {
							continue;
						}
					}
				}
				files->push_back(info);
				state = true;
			}
		}
	}
	return state;
}

#pragma endregion

#pragma region Helpers
bool OpenFile(std::string path) {
	bool state = false;
	path = ReplaceAll(path, "/", "\\");

	StorageFile^ storageItem;
	ExecuteTask(storageItem, StorageFile::GetFileFromPathAsync(ToPlatformString(path)));
	if (storageItem != nullptr) {
		ExecuteTask(state, Windows::System::Launcher::LaunchFileAsync(storageItem), false);
	}
	else {
		auto uri = ref new Windows::Foundation::Uri(ToPlatformString(path));
		ExecuteTask(state, Windows::System::Launcher::LaunchUriAsync(uri), false);
	}
	return state;
}

bool OpenFolder(std::string path) {
	bool state = false;
	Path itemPath(path);
	Platform::String^ wString = ref new Platform::String(itemPath.ToWString().c_str());
	StorageFolder^ storageItem;
	ExecuteTask(storageItem, StorageFolder::GetFolderFromPathAsync(wString));
	if (storageItem != nullptr) {
		ExecuteTask(state, Windows::System::Launcher::LaunchFolderAsync(storageItem), false);
	}
	else {
		// Try as it's file
		Path parent = Path(itemPath.GetDirectory());
		Platform::String^ wParentString = ref new Platform::String(parent.ToWString().c_str());

		ExecuteTask(storageItem, StorageFolder::GetFolderFromPathAsync(wParentString));
		if (storageItem != nullptr) {
			ExecuteTask(state, Windows::System::Launcher::LaunchFolderAsync(storageItem), false);
		}
	}
	return state;
}


bool GetDriveFreeSpace(Path path, int64_t& space) {

	bool state = false;
	Platform::String^ wString = ref new Platform::String(path.ToWString().c_str());
	StorageFolder^ storageItem;
	ExecuteTask(storageItem, StorageFolder::GetFolderFromPathAsync(wString));
	if (storageItem != nullptr) {
		Platform::String^ freeSpaceKey = ref new Platform::String(L"System.FreeSpace");
		Platform::Collections::Vector<Platform::String^>^ propertiesToRetrieve = ref new Platform::Collections::Vector<Platform::String^>();
		propertiesToRetrieve->Append(freeSpaceKey);
		Windows::Foundation::Collections::IMap<Platform::String^, Platform::Object^>^ result;
		ExecuteTask(result, storageItem->Properties->RetrievePropertiesAsync(propertiesToRetrieve));
		if (result != nullptr && result->Size > 0) {
			try {
				auto value = result->Lookup(L"System.FreeSpace");
				space = (uint64_t)value;
				state = true;
			}
			catch (...) {

			}
		}
	}

	return state;
}

bool IsFirstStart() {
	auto firstrun = GetDataFromLocalSettings("first_run");
	AddDataToLocalSettings("first_run", "done", true);
	return firstrun.empty();
}
#pragma endregion

#pragma region Logs
std::string GetLogFile() {
	std::string logFile;
	Path logFilePath = Path(GetWorkingFolder() + "\\PSP\\ppsspp.txt");
	HANDLE h = CreateFile2FromAppW(logFilePath.ToWString().c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, CREATE_ALWAYS, nullptr);
	if (h != INVALID_HANDLE_VALUE) {
		logFile = logFilePath.ToString();
		CloseHandle(h);
	}
	return logFile;
}

#pragma endregion
#endif
