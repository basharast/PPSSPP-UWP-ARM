#include <algorithm>
#include <cstring>
#include <set>

#include "ppsspp_config.h"

#include "Common/Net/HTTPClient.h"
#include "Common/Net/URL.h"

#include "Common/File/PathBrowser.h"
#include "Common/File/FileUtil.h"
#include "Common/File/DirListing.h"
#include "Common/StringUtils.h"
#include "Common/TimeUtil.h"
#include "Common/Log.h"
#include "Common/Thread/ThreadUtil.h"

#if PPSSPP_PLATFORM(ANDROID)
#include "android/jni/app-android.h"
#endif
#include <UWP/UWPHelpers/StorageManager.h>

bool LoadRemoteFileList(const Path &url, const std::string &userAgent, bool *cancel, std::vector<File::FileInfo> &files) {
	_dbg_assert_(url.Type() == PathType::HTTP);

	http::Client http;
	Buffer result;
	int code = 500;
	std::vector<std::string> responseHeaders;

	http.SetUserAgent(userAgent);

	Url baseURL(url.ToString());
	if (!baseURL.Valid()) {
		return false;
	}

	// Start by requesting the list of files from the server.
	http::RequestParams req(baseURL.Resource(), "text/plain, text/html; q=0.9, */*; q=0.8");
	if (http.Resolve(baseURL.Host().c_str(), baseURL.Port())) {
		if (http.Connect(2, 20.0, cancel)) {
			net::RequestProgress progress(cancel);
			code = http.GET(req, &result, responseHeaders, &progress);
			http.Disconnect();
		}
	}

	if (code != 200 || (cancel && *cancel)) {
		return false;
	}

	std::string listing;
	std::vector<std::string> items;
	result.TakeAll(&listing);

	std::string contentType;
	for (const std::string &header : responseHeaders) {
		if (startsWithNoCase(header, "Content-Type:")) {
			contentType = header.substr(strlen("Content-Type:"));
			// Strip any whitespace (TODO: maybe move this to stringutil?)
			contentType.erase(0, contentType.find_first_not_of(" \t\r\n"));
			contentType.erase(contentType.find_last_not_of(" \t\r\n") + 1);
		}
	}

	// TODO: Technically, "TExt/hTml    ; chaRSet    =    Utf8" should pass, but "text/htmlese" should not.
	// But unlikely that'll be an issue.
	bool parseHtml = startsWithNoCase(contentType, "text/html");
	bool parseText = startsWithNoCase(contentType, "text/plain");

	if (parseText) {
		// Plain text format - easy.
		SplitString(listing, '\n', items);
	} else if (parseHtml) {
		// Try to extract from an automatic webserver directory listing...
		GetQuotedStrings(listing, items);
	} else {
		ERROR_LOG(IO, "Unsupported Content-Type: %s", contentType.c_str());
		return false;
	}

	for (std::string item : items) {
		// Apply some workarounds.
		if (item.empty())
			continue;
		if (item.back() == '\r')
			item.pop_back();
		if (item == baseURL.Resource())
			continue;

		File::FileInfo info;
		info.name = item;
		info.fullName = Path(baseURL.Relative(item).ToString());
		info.isDirectory = endsWith(item, "/");
		info.exists = true;
		info.size = 0;
		info.isWritable = false;

		files.push_back(info);
	}

	return !files.empty();
}

PathBrowser::~PathBrowser() {
	std::unique_lock<std::mutex> guard(pendingLock_);
	pendingCancel_ = true;
	pendingStop_ = true;
	pendingCond_.notify_all();
	guard.unlock();

	if (pendingThread_.joinable()) {
		pendingThread_.join();
	}
}

void PathBrowser::SetPath(const Path &path) {
	path_ = path;
	HandlePath();
}

void PathBrowser::HandlePath() {
	if (!path_.empty() && path_.ToString()[0] == '!') {
		if (pendingActive_)
			ResetPending();
		ready_ = true;
		return;
	}

	std::lock_guard<std::mutex> guard(pendingLock_);
	ready_ = false;
	pendingActive_ = true;
	pendingCancel_ = false;
	pendingFiles_.clear();
	pendingPath_ = path_;
	pendingCond_.notify_all();

	if (pendingThread_.joinable())
		return;

	pendingThread_ = std::thread([&] {
		SetCurrentThreadName("PathBrowser");

		AndroidJNIThreadContext jniContext;  // destructor detaches

		std::unique_lock<std::mutex> guard(pendingLock_);
		std::vector<File::FileInfo> results;
		Path lastPath("NONSENSE THAT WONT EQUAL A PATH");
		while (!pendingStop_) {

			while (lastPath == pendingPath_ && !pendingCancel_) {
				pendingCond_.wait(guard);
			}
			if (pendingStop_) {
				break;
			}
			lastPath = pendingPath_;
			bool success = false;
			if (lastPath.Type() == PathType::HTTP) {
				guard.unlock();
				results.clear();
				success = LoadRemoteFileList(lastPath, userAgent_, &pendingCancel_, results);
				guard.lock();
			} else if (lastPath.empty()) {
				results.clear();
				success = true;
			} else {
				guard.unlock();
				results.clear();
				success = File::GetFilesInDir(lastPath, &results, nullptr);
				guard.lock();
			}

			if (pendingPath_ == lastPath) {
				if (success && !pendingCancel_) {
					pendingFiles_ = results;
				}
				pendingPath_.clear();
				lastPath.clear();
				ready_ = true;
			}
		}
	});
}

void PathBrowser::ResetPending() {
	std::lock_guard<std::mutex> guard(pendingLock_);
	pendingCancel_ = true;
	pendingPath_.clear();
}

bool PathBrowser::IsListingReady() {
	return ready_;
}

std::string PathBrowser::GetFriendlyPath() const {
	std::string str = GetPath().ToVisualString();
	// Show relative to memstick root if there.
	if (startsWith(str, aliasMatch_)) {
		return aliasDisplay_ + str.substr(aliasMatch_.size());
	}

#if PPSSPP_PLATFORM(LINUX) || PPSSPP_PLATFORM(MAC)
	char *home = getenv("HOME");
	if (home != nullptr && !strncmp(str.c_str(), home, strlen(home))) {
		str = std::string("~") + str.substr(strlen(home));
	}
#endif
	return str;
}

bool PathBrowser::GetListing(std::vector<File::FileInfo> &fileInfo, const char *filter, bool *cancel) {
	std::unique_lock<std::mutex> guard(pendingLock_);
	while (!IsListingReady() && (!cancel || !*cancel)) {
		// In case cancel changes, just sleep. TODO: Replace with condition variable.
		guard.unlock();
		sleep_ms(50);
		guard.lock();
	}

	fileInfo = ApplyFilter(pendingFiles_, filter);
	return true;
}

bool PathBrowser::CanNavigateUp() {
	return path_.CanNavigateUp();
}

void PathBrowser::NavigateUp() {
	path_ = path_.NavigateUp();
}

// TODO: Support paths like "../../hello"
void PathBrowser::Navigate(const std::string &path) {
	if (path == ".")
		return;
	if (path == "..") {
		NavigateUp();
	} else {
		if (path.size() >= 2 && path[1] == ':' && path_.IsRoot())
			path_ = Path(path);
		else
			path_ = path_ / path;
	}
	HandlePath();
}
