// UWP STORAGE MANAGER
// Copyright (c) 2023 Bashar Astifan.
// Email: bashar@astifan.online
// Telegram: @basharastifan
// GitHub: https://github.com/basharast/UWP2Win32

// Functions:
// GetDataFromLocalSettings(Platform::String^ key)
// AddDataToLocalSettings(Platform::String^ key, Platform::String^ data, bool replace)
// 
// AddFolderToFutureList(StorageFolder^ folder)
// AddFileToFutureList(StorageFile^ file)
// 
// AddToAccessibleDirectories(StorageFolder^ folder)
// AddToAccessibleFiles(StorageFile^ file)
// UpdateDirectoriesByFutureList()
// UpdateFilesByFutureList()
// FillAccessLists()
// 
// GetFolderByKey(Platform::String^ key)
// GetFileByKey(Platform::String^ key)
// AppendFolderByToken(Platform::String^ token)
// AppendFileByToken(Platform::String^ token)

#if defined(_M_ARM) || defined(BUILD14393)
#include "StorageConfig.h"
#include "StorageLog.h"
#include "StorageExtensions.h"
#include "StorageAsync.h"
#include "StorageAccess.h"
#include "StorageItemW.h"

using namespace Platform;
using namespace Windows::Storage;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Storage::AccessCache;
using namespace Windows::ApplicationModel;

// Main lookup list
std::list<StorageItemW> FutureAccessItems;

// Get value from app local settings
Platform::String^ GetDataFromLocalSettings(Platform::String^ key) {
	ApplicationDataContainer^ localSettings{ ApplicationData::Current->LocalSettings };
	IPropertySet^ values{ localSettings->Values };
	if (key != nullptr) {
		Platform::Object^ tokenRetrive = values->Lookup(key);
		if (tokenRetrive != nullptr) {
			Platform::String^ ConvertedToken = (Platform::String^)tokenRetrive;
			return ConvertedToken;
		}
	}
	return nullptr;
}

std::string GetDataFromLocalSettings(std::string key) {
	return convert(GetDataFromLocalSettings(convert(key)));
}

// Add or replace value in app local settings
bool AddDataToLocalSettings(Platform::String^ key, Platform::String^ data, bool replace) {
	ApplicationDataContainer^ localSettings{ ApplicationData::Current->LocalSettings };
	IPropertySet^ values{ localSettings->Values };

	Platform::String^ testResult = GetDataFromLocalSettings(key);
	if (testResult == nullptr) {
		values->Insert(key, PropertyValue::CreateString(data));
		return true;
	}
	else if (replace) {
		values->Remove(key);
		values->Insert(key, PropertyValue::CreateString(data));
		return true;
	}

	return false;
}

bool AddDataToLocalSettings(std::string key, std::string data, bool replace) {
	return AddDataToLocalSettings(convert(key), convert(data),replace);
}

// Add item to history list (FutureAccessItems)
void AddToAccessibleItems(IStorageItem^ item) {
	bool isFolderAddedBefore = false;
	for each (auto folderItem in FutureAccessItems) {
		if (folderItem.Equal(item)) {
			isFolderAddedBefore = true;
			break;
		}
	}
	
	if (!isFolderAddedBefore) {
		FutureAccessItems.push_back(StorageItemW(item));
	}
}


// Add folder to future list (to avoid request picker again)
void AddItemToFutureList(IStorageItem^ item) {
	try {
		if (item != nullptr) {
			Platform::String^ folderToken = AccessCache::StorageApplicationPermissions::FutureAccessList->Add(item);
			AddToAccessibleItems(item);
		}
	}
	catch (Platform::COMException^ e) {
	}
}

// Get item by key
// This function can be used when you store token in LocalSettings as custom key
IStorageItem^ GetItemByKey(Platform::String^ key) {
	IStorageItem^ item;
	Platform::String^ itemToken = GetDataFromLocalSettings(key);
	if (itemToken != nullptr && AccessCache::StorageApplicationPermissions::FutureAccessList->ContainsItem(itemToken)) {
		ExecuteTask(item, AccessCache::StorageApplicationPermissions::FutureAccessList->GetItemAsync(itemToken));
	}

	return item;
}

// Append folder by token to (FutureAccessFolders)
void AppendItemByToken(Platform::String^ token) {
	try {
		if (token != nullptr && AccessCache::StorageApplicationPermissions::FutureAccessList->ContainsItem(token)) {
			IStorageItem^ storageItem;
			ExecuteTask(storageItem, AccessCache::StorageApplicationPermissions::FutureAccessList->GetItemAsync(token));
			AddToAccessibleItems(storageItem);
		}
	}
	catch (Platform::COMException^ e) {
	}
}

// Update the history list by the future list (to restore all the picked items)
void UpdateItemsByFutureList() {
	auto AccessList = AccessCache::StorageApplicationPermissions::FutureAccessList->Entries;
	for each (auto ListItem in AccessList) {
		Platform::String^ itemToken = ListItem.Token;
		AppendItemByToken(itemToken);
	}
}

bool fillListsCalled = false;
bool fillListInProgress = false;
void FillLookupList() {
	if (fillListsCalled) {
		// Should be called only once
		// but let's wait in case we got too calls at once
		CoreWindow^ corewindow = CoreWindow::GetForCurrentThread();
		while (fillListInProgress)
		{
			try {
				if (corewindow) {
					corewindow->Dispatcher->ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);
				}
				else {
					corewindow = CoreWindow::GetForCurrentThread();
				}
			}
			catch (...) {

			}
		}
		return;
	}
	fillListsCalled = true;
	fillListInProgress = true;
	// Clean access list from any deleted/moved items
	for each (auto listItem in AccessCache::StorageApplicationPermissions::FutureAccessList->Entries) {
		try {
			IStorageItem^ test;
			ExecuteTask(test, AccessCache::StorageApplicationPermissions::FutureAccessList->GetItemAsync(listItem.Token));
			if (test == nullptr) {
				// Access denied or file moved/deleted
				AccessCache::StorageApplicationPermissions::FutureAccessList->Remove(listItem.Token);
			}
		}
		catch (Platform::COMException^ e) {
			// Access denied or file moved/deleted
			AccessCache::StorageApplicationPermissions::FutureAccessList->Remove(listItem.Token);
		}
	}

	// Get files/folders selected by the user
	UpdateItemsByFutureList();

	// Append known folders
#if APPEND_APP_LOCALDATA_LOCATION
	AddToAccessibleItems(ApplicationData::Current->LocalFolder);
	AddToAccessibleItems(ApplicationData::Current->TemporaryFolder);
#endif

#if APPEND_APP_INSTALLATION_LOCATION
	AddToAccessibleItems(Package::Current->InstalledLocation);
#endif

#if APPEND_DOCUMENTS_LOCATION
	// >>>>DOCUMENTS (requires 'documentsLibrary' capability)
	AddToAccessibleItems(KnownFolders::DocumentsLibrary);
#endif

#if APPEND_VIDEOS_LOCATION
	// >>>>VIDEOS (requires 'videosLibrary' capability)
	AddToAccessibleItems(KnownFolders::VideosLibrary);
#endif

#if APPEND_PICTURES_LOCATION
	// >>>>VIDEOS (requires 'picturesLibrary' capability)
	AddToAccessibleItems(KnownFolders::PicturesLibrary);
#endif

#if APPEND_MUSIC_LOCATION
	// >>>>MUSIC (requires 'musicLibrary' capability)
	AddToAccessibleItems(KnownFolders::MusicLibrary);
#endif

	// No need to append `RemovableDevices`
	// they will be allowed for access once you added the capability

	fillListInProgress = false;
}

#else

#include "StorageAsync.h"
#include "StorageAccess.h"
#include "UWPUtil.h"
#include <Common/File/Path.h>

using namespace Platform;
using namespace Windows::Storage;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Storage::AccessCache;
using namespace Windows::ApplicationModel;

std::list<std::string> alist;
void AppendToAccessList(Platform::String^ path)
{
	Path p(FromPlatformString(path));
	alist.push_back(p.ToString());
}

// Get value from app local settings
Platform::String^ GetDataFromLocalSettings(Platform::String^ key) {
	ApplicationDataContainer^ localSettings{ ApplicationData::Current->LocalSettings };
	IPropertySet^ values{ localSettings->Values };
	if (key != nullptr) {
		Platform::Object^ tokenRetrive = values->Lookup(key);
		if (tokenRetrive != nullptr) {
			Platform::String^ ConvertedToken = (Platform::String^)tokenRetrive;
			return ConvertedToken;
		}
	}
	return nullptr;
}

std::string GetDataFromLocalSettings(std::string key) {
	return FromPlatformString(GetDataFromLocalSettings(ToPlatformString(key)));
}

// Add or replace value in app local settings
bool AddDataToLocalSettings(Platform::String^ key, Platform::String^ data, bool replace) {
	ApplicationDataContainer^ localSettings{ ApplicationData::Current->LocalSettings };
	IPropertySet^ values{ localSettings->Values };

	Platform::String^ testResult = GetDataFromLocalSettings(key);
	if (testResult == nullptr) {
		values->Insert(key, PropertyValue::CreateString(data));
		return true;
	}
	else if (replace) {
		values->Remove(key);
		values->Insert(key, PropertyValue::CreateString(data));
		return true;
	}

	return false;
}

bool AddDataToLocalSettings(std::string key, std::string data, bool replace) {
	return AddDataToLocalSettings(ToPlatformString(key), ToPlatformString(data), replace);
}

// Add folder to future list (to avoid request picker again)
void AddItemToFutureList(IStorageItem^ item) {
	try {
		if (item != nullptr) {
			Platform::String^ folderToken = AccessCache::StorageApplicationPermissions::FutureAccessList->Add(item);
			AppendToAccessList(item->Path);
		}
	}
	catch (Platform::COMException^ e) {
	}
}

// Get item by key
// This function can be used when you store token in LocalSettings as custom key
IStorageItem^ GetItemByKey(Platform::String^ key) {
	IStorageItem^ item;
	Platform::String^ itemToken = GetDataFromLocalSettings(key);
	if (itemToken != nullptr && AccessCache::StorageApplicationPermissions::FutureAccessList->ContainsItem(itemToken)) {
		ExecuteTask(item, AccessCache::StorageApplicationPermissions::FutureAccessList->GetItemAsync(itemToken));
	}

	return item;
}

std::list<std::string> GetFutureAccessList() {
	if (alist.empty()) {
		auto AccessList = AccessCache::StorageApplicationPermissions::FutureAccessList->Entries;
		for (auto it = 0; it != AccessList->Size; ++it) {
			auto item = AccessList->GetAt(it);
			try {
				auto token = item.Token;
				if (token != nullptr && AccessCache::StorageApplicationPermissions::FutureAccessList->ContainsItem(token)) {
					IStorageItem^ storageItem;
					ExecuteTask(storageItem, AccessCache::StorageApplicationPermissions::FutureAccessList->GetItemAsync(token));
					if (storageItem != nullptr) {
						AppendToAccessList(storageItem->Path);
					}
					else {
						AccessCache::StorageApplicationPermissions::FutureAccessList->Remove(token);
					}
				}
			}
			catch (Platform::COMException^ e) {
			}
		}

		AppendToAccessList(ApplicationData::Current->LocalFolder->Path);
		AppendToAccessList(ApplicationData::Current->TemporaryFolder->Path);
	}
	return alist;
}

#endif
