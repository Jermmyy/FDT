#include "RegisterPanels.hpp"
#include "CommandRegistry.hpp"
//#include "commands/WatchlistCommands.hpp"

#include "../config/AppConfig.hpp"

#include "../panels/RatePanel.hpp"
#include "../panels/FedPanel.hpp"
#include "../panels/SecfilingPanel.hpp"
#include "../panels/AlertFeedPanel.hpp"
#include "../panels/WatchlistPanel.hpp"
#include "../panels/ListDisplayPanel.hpp"
#include "../panels/PplTrackerPanel.hpp"
#include "../panels/HelpPanel.hpp"
#include "../panels/FocusPanel.hpp"

#include "../api/EquityProvider.hpp"
//#include "../core/WatchlistManager.hpp"
#include "../logging/NetworkWire.hpp"

#include <memory>

namespace UI {
    void SpawnSecFilingPanel(ultralight::Renderer* renderer);
    void SpawnPplTrackerPanel();
    void SpawnHelpPanel();
    void OpenChatPanel(const std::string& targetChannel);
}

void RegisterAllPanels(ultralight::Renderer* renderer)
{
    // Instantiate your provider right here locally
    const std::string& finnhubKey = AppConfig::Instance().Get("FINNHUB_API_KEY");
    auto finnhubProvider = std::make_shared<FinnhubProvider>(finnhubKey);

    finnhubProvider->Start();

    CommandRegistry::Get().RegisterDynamic(
        "FOCUS",
        "Open equity focus panel (e.g., FOCUS AAPL)",
        [finnhubProvider](const std::string& ticker) {
            if (!ticker.empty()) {
                UI::SpawnFocusPanel(ticker, finnhubProvider);
            }
        }
    );

    CommandRegistry::Get().Register(
        "FILINGS",
        "SEC",
        "SEC Company Filing Viewer",
        [renderer]() {
            UI::SpawnSecFilingPanel(renderer);
        }
    );

    CommandRegistry::Get().Register(
        "TRACKER",
        "PPL",
        "People Tracker & Leak Feed",
        []() {
            UI::SpawnPplTrackerPanel();
        }
    );

    CommandRegistry::Get().Register(
        "HELP",
        "SYS",
        "Show Application Commands & Help",
        []() {
            UI::SpawnHelpPanel();
        }
    );

    CommandRegistry::Get().Register(
        "EVENTS",
        "FEED",
        "Show System Event & Alert Feed",
        []() {
            SpawnAlertFeedPanel();
        }
    );

    // Plain CHAT command opens the master panel
    CommandRegistry::Get().RegisterDynamic(
        "CHAT",
        "Open chat panel or ticker room (e.g., CHAT INTC)",
        [](const std::string& arg) {
            UI::OpenChatPanel(arg);
        }
    );
}