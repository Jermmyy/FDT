#include "RSSPoller.hpp"
#include "../core/AlertBus.hpp"
#include <nlohmann/json.hpp>
//#include <cpr/cpr.h>
#include "../logging/NetWrapper.hpp"
#include <fstream>
#include <algorithm>
#include <chrono>

using json = nlohmann::json;

// [SECTION] Mirror resolution
// [SECTION] Persistance
// [SECTION] User Management
// [SECTION] Feed
// [SECTION] Poll Loop


// [SECTION] Mirror resolution
std::string RSSPoller::ResolveMirror(const std::string& username) {
	std::printf(">>> ResolveMirror searching for: %s\n", username.c_str());
	for (const char* mirror : k_mirrors) {
		std::string url = std::string(mirror) + "/" + username + "/rss";
		std::printf(">>> Trying mirror URL: %s\n", url.c_str());
		try {
			cpr::Response r = Net::Get(
				cpr::Url{ url },
				cpr::Header{
					{ "User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36" },
					{ "Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8" }
				},
				cpr::Timeout{ 5000 }
			);
			std::printf(">>> Mirror response status: %d (Error code: %d)\n", r.status_code, (int)r.error.code);
			if (!r.error && r.status_code < 400)
				return url;
		}
		catch (const std::exception& e) {
			std::printf(">>> Mirror exception: %s\n", e.what());
		}
		catch (...) {
			std::printf(">>> Mirror unknown exception\n");
		}
	}
	std::printf(">>> ResolveMirror failed for all mirrors.\n");
	return {};
}


// [SECTION] Persistance
void RSSPoller::Load(const std::string& path) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_savePath = path;

	std::ifstream f(path, std::ios::binary);
	if (!f.is_open()) return;

	try {
		std::vector<uint8_t> buf(std::istreambuf_iterator<char>(f), {});
		json j = json::from_msgpack(buf);

		for (const auto& u : j["users"]) {
			TrackedUser user;
			user.username = u.value("username", "");
			user.feedUrl = u.value("feedUrl", "");
			if (!user.username.empty() && !user.feedUrl.empty())
				m_users.push_back(std::move(user));
		}
	} catch (...) {}
}

void RSSPoller::Save() {
	// Called internally, lock already held
	if (m_savePath.empty()) return;

	json j;
	j["users"] = json::array();
	for (const auto& u : m_users)
		j["users"].push_back({ {"username", u.username}, {"feedUrl", u.feedUrl} });

	std::vector<uint8_t> packed = json::to_msgpack(j);
	std::ofstream f(m_savePath, std::ios::binary);
	if (f.is_open())
		f.write(reinterpret_cast<const char*>(packed.data()), packed.size());
}

// [SECTION] User Management

void RSSPoller::AddUser(const std::string& username) {
	std::printf(">>> AddUser started for: %s\n", username.c_str());
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		bool exists = std::any_of(m_users.begin(), m_users.end(),
			[&](const TrackedUser& u) { return u.username == username; });
		if (exists) {
			std::printf(">>> User already exists!\n");	
			return;
		}
	}

	// Resolve mirror outside lock — this is a network call
	std::string url = ResolveMirror(username);
	if (url.empty()) {
		AlertBus::Get().Post(AlertType::Error, "RSS",
			"Could not reach any Nitter mirror for: " + username);
		return;
	}

	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_users.push_back({ username, url });
		Save();
	}

	// Immediate first poll outside lock
	std::vector<RSSPost> posts = m_client.Fetch(url);
	std::lock_guard<std::mutex> lock(m_mutex);

	// Seed seenGuids from initial fetch so PollAll() doesnt fire again
	// Alerts for posts that were already present when the user was added
	for (const auto& post : posts)
		m_seenGuids.insert(post.guid);

	m_posts.insert(m_posts.begin(),
		std::make_move_iterator(posts.begin()),
		std::make_move_iterator(posts.end()));

	if ((int)m_posts.size() > k_maxPosts)
		m_posts.resize(k_maxPosts);
}

void RSSPoller::RemoveUser(const std::string& username) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_users.erase(std::remove_if(m_users.begin(), m_users.end(),
		[&](const TrackedUser& u) { return u.username == username; }),
		m_users.end());
	Save();
}

std::vector<TrackedUser> RSSPoller::GetUsersCopy() const {
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_users;
}

// [SECTION] Feed

std::vector<RSSPost> RSSPoller::GetPostsCopy() const {
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_posts;
}

void RSSPoller::ClearFeed() {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_posts.clear();
}

// [SECTION] Lifecycle

void RSSPoller::Start(int intervalSeconds) {
	if (m_running) return;
	m_running = true;
	m_thread = std::thread(&RSSPoller::Pollloop, this, intervalSeconds);
}

void RSSPoller::Stop() {
	m_running = false;
	if (m_thread.joinable())
		m_thread.join();
}

// [SECTION] Poll Loop
void RSSPoller::Pollloop(int intevalSeconds) {
	// Poll immediately on start
	PollAll(true);

	auto nextPoll = std::chrono::steady_clock::now() + std::chrono::seconds(intevalSeconds);

	while (m_running) {
		std::this_thread::sleep_for(std::chrono::seconds(1));

		if (std::chrono::steady_clock::now() >= nextPoll) {
			PollAll(false);
			nextPoll = std::chrono::steady_clock::now() + std::chrono::seconds(intevalSeconds);
		}
	}
}

void RSSPoller::PollAll(bool init) {
	std::vector<TrackedUser> users;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		users = m_users;
	}

	for (const auto& user : users) {
		std::vector<RSSPost> posts = m_client.Fetch(user.feedUrl);
		if (posts.empty()) continue;
		
		std::vector<RSSPost> newPosts;
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			for (auto& post : posts) {
				if (m_seenGuids.count(post.guid)) continue;
				m_seenGuids.insert(post.guid);
				newPosts.push_back(post);
			}
			if (!init) {
				// Post to AlertBus for each new post
				for (const auto& post : newPosts) {
					std::string msg = post.title.empty() ? post.content.substr(0, 80) : post.title;
					for (char& c : msg) {
						if (c == '\n' || c == '\r') c = ' ';
					}
					AlertBus::Get().Post(AlertType::News,
						post.author.empty() ? "RSS" : post.author,
						msg);
				}
			}

			m_posts.insert(m_posts.begin(), 
                   std::make_move_iterator(newPosts.begin()), 
                   std::make_move_iterator(newPosts.end()));
			if ((int)m_posts.size() > k_maxPosts)
				m_posts.resize(k_maxPosts);
		}
	}
}