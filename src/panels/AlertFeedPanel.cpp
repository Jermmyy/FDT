#include "AlertFeedPanel.hpp"
#include "../core/AlertBus.hpp"
#include "../core/PanelRegistry.hpp"
#include "../components/PanelComponents.hpp"
#include <imgui.h>
#include <algorithm>
#include <ctime>


void RenderAlertFeedPanel(entt::registry& reg, entt::entity entity) {
    auto& state = reg.get<AlertFeedState>(entity); //[cite: 1]

    const auto& alerts = AlertBus::Get().GetAlertsCopy(); //[cite: 1]
    double currentTime = ImGui::GetTime(); //[cite: 1]

    ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar); //[cite: 1]

    for (int i = (int)alerts.size() - 1; i >= 0; --i) { //[cite: 1]
        const auto& alert = alerts[i]; //[cite: 1]

        ImVec4 typeColor;
        switch (alert.type) { //[cite: 1]
        case AlertType::SEC:     typeColor = ImVec4(1.0f, 0.45f, 0.0f, 1.0f); break; //[cite: 1]
        case AlertType::FED:     typeColor = ImVec4(0.1f, 1.0f, 0.1f, 1.0f); break;  //[cite: 1]
        case AlertType::Error:   typeColor = ImVec4(1.0f, 0.2f, 0.2f, 1.0f); break;  //[cite: 1]
        case AlertType::System:  typeColor = ImVec4(0.6f, 0.6f, 0.6f, 1.0f); break;  //[cite: 1]
        default:                 typeColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); break;  //[cite: 1]
        }

        float timeDiff = (float)(currentTime - alert.timestamp); //[cite: 1]
        float flashAlpha = std::max(0.0f, 1.0f - (timeDiff / 1.5f)); //[cite: 1]

        if (flashAlpha > 0.0f) { //[cite: 1]
            ImVec2 pMin = ImGui::GetCursorScreenPos(); //[cite: 1]
            float lineH = ImGui::GetTextLineHeightWithSpacing(); //[cite: 1]
            ImVec2 pMax = ImVec2(pMin.x + ImGui::GetContentRegionAvail().x, pMin.y + lineH); //[cite: 1]
            ImGui::GetWindowDrawList()->AddRectFilled(pMin, pMax, ImColor(0.2f, 0.4f, 0.8f, flashAlpha * 0.2f)); //[cite: 1]
        }

        struct tm tstruct;
        localtime_s(&tstruct, &alert.wallTime); //[cite: 1]
        char timeBuf[16];
        strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", &tstruct); //[cite: 1]

        ImGui::TextDisabled("%s", timeBuf); //[cite: 1]
        ImGui::SameLine(70.0f); //[cite: 1]

        float windowWidth = ImGui::GetContentRegionMax().x; //[cite: 1]
        float padding = 10.0f; //[cite: 1]

        std::string displayTitle = alert.title; //[cite: 1]
        float maxTitleWidth = windowWidth * 0.30f; //[cite: 1]

        if (ImGui::CalcTextSize(displayTitle.c_str()).x > maxTitleWidth) { //[cite: 1]
            while (!displayTitle.empty() && ImGui::CalcTextSize((displayTitle + "...").c_str()).x > maxTitleWidth) { //[cite: 1]
                displayTitle.pop_back(); //[cite: 1]
            }
            displayTitle += "..."; //[cite: 1]
        }

        char tagBuf[128];
        snprintf(tagBuf, sizeof(tagBuf), "[%s]", displayTitle.c_str()); //[cite: 1]
        float finalTagWidth = ImGui::CalcTextSize(tagBuf).x; //[cite: 1]

        float rightBoundary = windowWidth - finalTagWidth - padding; //[cite: 1]
        float availableMsgWidth = rightBoundary - ImGui::GetCursorPosX() - padding; //[cite: 1]

        std::string displayMsg = alert.message; //[cite: 1]
        if (ImGui::CalcTextSize(displayMsg.c_str()).x > availableMsgWidth) { //[cite: 1]
            while (!displayMsg.empty() && ImGui::CalcTextSize((displayMsg + "...").c_str()).x > availableMsgWidth) { //[cite: 1]
                displayMsg.pop_back(); //[cite: 1]
            }
            displayMsg += "..."; //[cite: 1]
        }

        ImGui::TextUnformatted(displayMsg.c_str()); //[cite: 1]
        ImGui::SameLine(rightBoundary + padding); //[cite: 1]
        ImGui::TextColored(typeColor, "%s", tagBuf); //[cite: 1]

        ImGui::Separator(); //[cite: 1]
    }

    ImGui::EndChild(); //[cite: 1]
}

void SpawnAlertFeedPanel() {
    auto& registry = PanelRegistry::Get().GetRegistry();
    auto entity = registry.create();

    // 1. Core window frame settings managed by PanelRegistry
    registry.emplace<WindowComp>(entity, "Event Feed", true);

    // 2. Entity state component
    registry.emplace<AlertFeedState>(entity);

    // 3. Layout persistence metadata (matches your CommandRegistry command key)
    registry.emplace<PanelMeta>(entity, PanelMeta{ "EVENTS", "" });

    // 4. Stateless render wrapper
    registry.emplace<PanelRenderFn>(entity, [](entt::registry& reg, entt::entity ent) {
        RenderAlertFeedPanel(reg, ent);
        });
}