#pragma once
#include "RSSClient.hpp"
#include <string>
#include <vector>
#include <unordered_set>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>

struct TrackedUser {
	std::string username;
	std::string feedUrl; // e.g. https://nitter.net/username/rss
};

class RSSPoller {
public:
	static RSSPoller& Get() {
		static RSSPoller instance;
		return instance;
	}

	void Load(const std::string& path);

	// User management
	void AddUser(const std::string& username);
	void RemoveUser(const std::string& username);
	std::vector<TrackedUser> GetUsersCopy() const;

	// Feed
	std::vector<RSSPost> GetPostsCopy() const;
	void ClearFeed();

	// Lifecycle : call Start() once at startup, Stop() on shutdown
	void Start(int intervalSeconds = 10000);
	void Stop();

private:
	RSSPoller() = default;
	~RSSPoller() = default;

	RSSPoller(const RSSPoller&) = delete;
	RSSPoller& operator=(const RSSPoller&) = delete;

	std::string ResolveMirror(const std::string& username);

	void Pollloop(int intervalSeconds);
	void PollAll(bool init = false);
	void Save();

	mutable std::mutex m_mutex;
	std::vector<TrackedUser> m_users;
	std::vector<RSSPost> m_posts; // chronological feed, newest first
	std::unordered_set<std::string> m_seenGuids; // dedup

	std::thread m_thread;
	std::atomic<bool> m_running{ false };
	std::string m_savePath;

	RSSClient m_client;

	static constexpr int k_maxPosts = 200;

	static constexpr const char* k_mirrors[] = {
		"https://nitter.net",
		"https://nitter.privacydev.net",
		"https://nitter.poast.org",
		"https://nitter.catsarch.com",
	};
};