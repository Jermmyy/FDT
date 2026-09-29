#pragma once
#include <string>
#include <vector>
#include <mutex>

// [SECTION] Data
// [SECTION] Manager


// [SECTION] Data

struct WatchedEntity {
	std::string ticker;
	bool tracked = false;
};

struct WatchList {
	std::string name;
	std::vector<WatchedEntity> entities;
	bool list_tracked = false; // TODO reserved for future list-level tracking toggle
};

// [SECTION] Manager
class WatchlistManager {
public:
	static WatchlistManager& Get();

	void Load(const std::string& path);

	// List operations
	void AddList(const std::string& name);
	void RemoveList(int index);
	void SetActiveList(int index);
	void SetListTracked(int listIndex, bool tracked);

	// Ticker operations
	void AddTicker(int listIndex, const std::string& ticker);
	void RemoveTicker(int listIndex, const std::string& ticker);
	void SetTracked(int listIndex, const std::string& ticker, bool tracked);
	
	// Query
	bool IsTracked(const std::string& ticker) const;
	std::vector<WatchList> GetListsCopy() const;
	int GetActiveIndex() const;

	// Helper for ListDisplayPanel
	int FindListIndexByName(const std::string& name);
	void AddTickerByName(const std::string& listName, const std::string& ticker);
	void RemoveTickerByName(const std::string& listName, const std::string& ticker);


private:
	WatchlistManager() = default;
	~WatchlistManager() = default;

	WatchlistManager(const WatchlistManager&) = delete;
	WatchlistManager& operator = (const WatchlistManager&) = delete;

	bool ValidIndex(int index) const;
	void Save();

	mutable std::mutex		m_mutex;
	std::vector<WatchList>	m_lists;
	int						m_activeIndex = 0;
	std::string				m_savePath;
};