#pragma once

#include "../api/EquityProvider.hpp"
#include "../components/PanelComponents.hpp"
#include "../components/FocusComponents.hpp"
#include <entt/entt.hpp>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <chrono>

class FocusClient
{
public:
	explicit FocusClient(std::shared_ptr<IEquityProvider> provider);
	~FocusClient();

	void SetSymbol(const std::string& symbol);
	EquityQuote GetQuote() const;
	std::string GetSymbol() const;

private:
	mutable std::mutex m_mutex;
	std::shared_ptr<IEquityProvider> m_provider;
	std::string m_symbol;
};

namespace UI
{
	entt::entity SpawnFocusPanel(const std::string& symbol, std::shared_ptr<IEquityProvider> provider);
	void DrawFocusPanel(entt::registry& registry, entt::entity entity);
}