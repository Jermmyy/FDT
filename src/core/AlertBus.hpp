#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <ctime>
#include <imgui.h>

enum class AlertType {
	System,
	SEC,
	FED,
	News,
	Error
};

struct FeedAlert {
	std::string title;
	std::string message;
	double timestamp;
	AlertType type;
	time_t wallTime;
};

class AlertBus {
public:
	static AlertBus& Get() {
		static AlertBus instance;
		return instance;
	}

	void Post(AlertType type, const std::string& title, const std::string& message) {
		std::lock_guard<std::mutex> lock(m_mutex);

		m_alerts.push_back({
			title,
			message,
			ImGui::GetTime(),
			type,
			time(nullptr)
		});

		if (m_alerts.size() > 50) {
			m_alerts.erase(m_alerts.begin());
		}
	}

	std::vector<FeedAlert> GetAlertsCopy() {
		std::lock_guard<std::mutex> lock(m_mutex);
		return m_alerts;
	}

private:
	AlertBus() = default;
	~AlertBus() = default;

	AlertBus(const AlertBus&) = delete;
	AlertBus& operator=(const AlertBus&) = delete;

	std::vector<FeedAlert> m_alerts;
	std::mutex m_mutex;
};