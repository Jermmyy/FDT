#include "WatchlistManager.hpp"
#include <fstream>
#include <algorithm>
#include <nlohmann/json.hpp>

// [SECTION] Singleton
// [SECTION] Load/Save List
// [SECTION] List operations
// [SECTION] Ticker operations
// [SECTION] Query

// [INTERNAL]


using json = nlohmann::json;

// [SECTION] Singleton
WatchlistManager& WatchlistManager::Get() {
	static WatchlistManager instance;
	return instance;
}

// [SECTION] Load / Save
void WatchlistManager::Load(const std::string& path) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_savePath = path;

	std::ifstream f(path);
	if (!f.is_open()) return;

	try {
		json j = json::parse(f);
		m_activeIndex = j.value("active_index", 0);

		for (const auto& jList : j["lists"]) {
			WatchList list;
			list.name = jList.value("name", "Unnamed");
			list.list_tracked = jList.value("list_tracked", false);

			for (const auto& jEntity : jList["entities"]) {
				WatchedEntity e;
				e.ticker = jEntity.value("ticker", "");
				e.tracked = jEntity.value("tracked", false);
				if (!e.ticker.empty())
					list.entities.push_back(e);
			}
			m_lists.push_back(std::move(list));
		}
	}
	catch (...) {
		// Corrupt or missing file = Start fresh
	}
}

void WatchlistManager::Save() {
	// Called internally, assumes lock is already held
	if (m_savePath.empty()) return;

	json j;
	j["active_index"] = m_activeIndex;
	j["lists"] = json::array();

	for (const auto& list : m_lists) {
		json jList;
		jList["name"] = list.name;
		jList["list_tracked"] = list.list_tracked;
		jList["entities"] = json::array();

		for (const auto& e : list.entities) {
			jList["entities"].push_back({
				{ "ticker",  e.ticker  },
				{ "tracked", e.tracked }
				});
		}
		j["lists"].push_back(jList);
	}

	std::ofstream f(m_savePath);
	if (f.is_open())
		f << j.dump(4);
}

// [SECTION] List operations
void WatchlistManager::AddList(const std::string& name) {
	std::lock_guard<std::mutex> lock(m_mutex);
	WatchList list;
	list.name = name;
	m_lists.push_back(std::move(list));
	Save();
}

void WatchlistManager::RemoveList(int index) {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (!ValidIndex(index)) return;
	m_lists.erase(m_lists.begin() + index);
	m_activeIndex = std::clamp(m_activeIndex, 0, (int)m_lists.size() - 1);
	Save();
}

void WatchlistManager::SetActiveList(int index) {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (!ValidIndex(index)) return;
	m_activeIndex = index;
	Save();
}

void WatchlistManager::SetListTracked(int listIndex, bool tracked) {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (!ValidIndex(listIndex)) return;
	m_lists[listIndex].list_tracked = tracked;
	Save();
}

// [SECTION] Ticker operations
void WatchlistManager::AddTicker(int listIndex, const std::string& ticker) {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (!ValidIndex(listIndex)) return;

	auto& entities = m_lists[listIndex].entities;
	bool exists = std::any_of(entities.begin(), entities.end(),
		[&](const WatchedEntity& e) { return e.ticker == ticker; });

	if (!exists) {
		entities.push_back({ ticker, false });
		Save();
	}
}

void WatchlistManager::RemoveTicker(int listIndex, const std::string& ticker) {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (!ValidIndex(listIndex)) return;

	auto& entities = m_lists[listIndex].entities;
	entities.erase(std::remove_if(entities.begin(), entities.end(),
		[&](const WatchedEntity& e) { return e.ticker == ticker; }),
		entities.end());
	Save();
}

void WatchlistManager::SetTracked(int listIndex, const std::string& ticker, bool tracked) {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (!ValidIndex(listIndex)) return;

	for (auto& e : m_lists[listIndex].entities) {
		if (e.ticker == ticker) {
			e.tracked = tracked;
			Save();
			return;
		}
	}
}

// [SECTION] Query
bool WatchlistManager::IsTracked(const std::string& ticker) const {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (!ValidIndex(m_activeIndex)) return false;

	for (const auto& e : m_lists[m_activeIndex].entities)
		if (e.ticker == ticker) return e.tracked;
	return false;
}

std::vector<WatchList> WatchlistManager::GetListsCopy() const {
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_lists;
}

int WatchlistManager::GetActiveIndex() const {
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_activeIndex;
}

// Helper ListDisplayPanel
int WatchlistManager::FindListIndexByName(const std::string& name) {
	std::lock_guard<std::mutex> lock(m_mutex);
	for (int i = 0; i < (int)m_lists.size(); ++i) {
		if (m_lists[i].name == name) return i;
	}
	return -1;
}

void WatchlistManager::AddTickerByName(const std::string& listName, const std::string& ticker) {
	int idx = FindListIndexByName(listName);

	if (idx != -1) {
		AddTicker(idx, ticker);
	}
	else {
		printf("[WL] Error: List '%s' does not exist. Use 'WL CREATE '%s' first.\n", listName.c_str(), listName.c_str());
	}
}

void WatchlistManager::RemoveTickerByName(const std::string& listName, const std::string& ticker) {
	int idx = FindListIndexByName(listName);

	if (idx != -1) {
		RemoveTicker(idx, ticker);
	}
	else {
		printf("[WL] Error: List '%s' does not exist. Use 'WL CREATE '%s' first.\n", listName.c_str(), listName.c_str());
	}
}

// [INTERNAL]
bool WatchlistManager::ValidIndex(int index) const {
	return index >= 0 && index < (int)m_lists.size();
}