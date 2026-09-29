#include "HelpPanel.hpp"
#include "../core/CommandRegistry.hpp"
#include "../core/PanelRegistry.hpp"
#include "../components/PanelComponents.hpp"
#include "../components/HelpComponents.hpp"
#include <imgui.h>
#include <map>
#include <vector>

namespace UI {
    void DrawHelpPanel() {
        ImGui::TextDisabled("Registered Commands (Usage: CATEGORY COMMAND)");
        ImGui::Separator();

        const std::vector<CommandEntry>& entries = CommandRegistry::Get().Entries();

        std::map<std::string, std::vector<const CommandEntry*>> grouped;
        for (const auto& e : entries) {
            grouped[e.group].push_back(&e);
        }

        if (ImGui::BeginChild("HelpList")) {
            for (auto const& [groupName, cmds] : grouped) {
                if (ImGui::CollapsingHeader(groupName.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                    if (ImGui::BeginTable("HelpTable", 2, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg)) {
                        ImGui::TableSetupColumn("Command", ImGuiTableColumnFlags_WidthFixed, 100.0f);
                        ImGui::TableSetupColumn("Description");

                        for (const auto* e : cmds) {
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            // Show the specific command token
                            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", e->command.c_str());

                            ImGui::TableSetColumnIndex(1);
                            // Show the registered description
                            ImGui::TextWrapped("%s", e->description.c_str());
                        }
                        ImGui::EndTable();
                    }
                }
                ImGui::Spacing();
            }
            ImGui::EndChild();
        }
    }

    void SpawnHelpPanel() {
        auto& registry = PanelRegistry::Get().GetRegistry();
        std::string uniqueTitle = "Help";

        // Toggle check: close if already open
        auto view = registry.view<WindowComp>();
        for (auto [entity, win] : view.each()) {
            if (win.title == uniqueTitle) {
                registry.destroy(entity);
                return;
            }
        }

        // Create fresh entity
        auto entity = registry.create();
        registry.emplace<WindowComp>(entity, uniqueTitle, true);
        registry.emplace<HelpPanelComp>(entity);
        registry.emplace<PanelMeta>(entity, PanelMeta{ "HELP", "SYS" });

        // Render loop callback
        registry.emplace<PanelRenderFn>(entity, [](entt::registry& reg, entt::entity ent) {
            DrawHelpPanel();
        });
    }
}