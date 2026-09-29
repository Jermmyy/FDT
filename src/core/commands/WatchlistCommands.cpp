#include "WatchlistCommands.hpp"
#include "../core/WatchlistManager.hpp"
#include "../core/PanelRegistry.hpp"
#include "../components/PanelComponents.hpp"
#include "../api/EquityProvider.hpp"
#include <sstream>
#include <algorithm>
#include <imgui.h>

// Forward declaration of the EnTT spawner for list display panels
namespace UI {
    void DrawListDisplayPanel(entt::registry& registry, entt::entity entity);
    
    inline entt::entity SpawnListDisplayPanel(const std::string& listName, std::shared_ptr<IEquityProvider> provider) {
        auto& registry = PanelRegistry::Get().GetRegistry();
        std::string uniqueTitle = "Watchlist: " + listName + "##" + listName;

        // Toggle check: if already open, close it
        auto view = registry.view<WindowComp>();
        for (auto [entity, win] : view.each()) {
            if (win.title == uniqueTitle) {
                registry.destroy(entity);
                return entt::null;
            }
        }

        auto entity = registry.create();
        registry.emplace<WindowComp>(entity, uniqueTitle, true);
        
        // Store your list name and provider in custom components or pass them along
        // For now, we attach the render function pointing to your list display logic
        registry.emplace<PanelRenderFn>(entity, [listName, provider](entt::registry& reg, entt::entity ent) {
            // Call your underlying ImGui drawing function for the list display here
            // RenderListDisplayPanel(listName, provider.get());
            ImGui::Text("Watchlist Monitor: %s", listName.c_str());
        });

        return entity;
    }
}

namespace WatchlistCommands {

    bool Execute(const std::string& rawArgs, std::shared_ptr<IEquityProvider> provider) {
        std::stringstream ss(rawArgs);
        std::string arg1, arg2, arg3;
        ss >> arg1 >> arg2 >> arg3;

        if (arg1.empty()) return false;

        std::transform(arg1.begin(), arg1.end(), arg1.begin(), ::toupper);
        if (!arg2.empty()) std::transform(arg2.begin(), arg2.end(), arg2.begin(), ::toupper);

        auto& wm = WatchlistManager::Get();

        // --- ACTION: CREATE <NAME> ---
        if (arg1 == "CREATE" && !arg2.empty()) {
            if (wm.FindListIndexByName(arg2) == -1) {
                wm.AddList(arg2);
            }
            return true;
        }

        // --- ACTION: DELETE <NAME> ---
        if (arg1 == "DELETE" && !arg2.empty()) {
            int idx = wm.FindListIndexByName(arg2);
            if (idx != -1) wm.RemoveList(idx);
            return true;
        }

        // --- ACTION: <LIST> ADD <TICKER> ---
        if (!arg1.empty() && arg2 == "ADD" && !arg3.empty()) {
            wm.AddTickerByName(arg1, arg3);
            return true;
        }

        // --- ACTION: <LIST> DELETE/DEL <TICKER> ---
        if (!arg1.empty() && (arg2 == "DELETE" || arg2 == "DEL") && !arg3.empty()) {
            wm.RemoveTickerByName(arg1, arg3);
            return true;
        }

        // --- VIEW: DISPLAY LIST PANEL (<LISTNAME>) ---
        if (!arg1.empty() && arg2.empty()) {
            UI::SpawnListDisplayPanel(arg1, provider);
            return true;
        }

        return false;
    }

} // namespace WatchlistCommands