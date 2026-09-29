#include "WatchlistPanel.hpp"
#include "../core/WatchlistManager.hpp"
#include <imgui.h>
#include <string>

void RenderWatchlistPanel(WatchlistPanelState& state) {
	ImGui::SetNextWindowSize(ImVec2(340, 500), ImGuiCond_FirstUseEver);

	if (!ImGui::Begin("Watchlists", nullptr, ImGuiWindowFlags_NoCollapse)) {
		ImGui::End();
		return;
	}

	auto& wm = WatchlistManager::Get();
	auto lists = wm.GetListsCopy();
	int activeIdx = wm.GetActiveIndex();

	// List selector
	const char* previewName = lists.empty() ? "No lists" : lists[activeIdx].name.c_str();

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 140.0f);
	if (ImGui::BeginCombo("##list_select", previewName)) {
		for (int i = 0; i < (int)lists.size(); i++) {
			bool selected = (i == activeIdx);
			if (ImGui::Selectable(lists[i].name.c_str(), selected))
				wm.SetActiveList(i);
			if (selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	ImGui::SameLine();
	if (ImGui::Button("+ New"))
		state.showNewListInput = !state.showNewListInput;

	// Remove active list
	if (!lists.empty()) {
		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
		if (ImGui::Button("Remove List"))
			wm.RemoveList(activeIdx);
		ImGui::PopStyleColor();
	}

    // New list input
    if (state.showNewListInput) {
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 60.0f);
        ImGui::InputText("##new_list_name", state.newListName, sizeof(state.newListName));
        ImGui::SameLine();
        if (ImGui::Button("Create") && state.newListName[0] != '\0') {
            wm.AddList(state.newListName);
            memset(state.newListName, 0, sizeof(state.newListName));
            state.showNewListInput = false;
        }
    }

    ImGui::Separator();

    // -----------------------------------------------------------------------
    // Ticker list
    // -----------------------------------------------------------------------

    if (lists.empty()) {
        ImGui::TextDisabled("No lists yet. Create one above.");
        ImGui::End();
        return;
    }

    // Refresh after potential mutation
    lists = wm.GetListsCopy();
    activeIdx = wm.GetActiveIndex();

    const auto& activeList = lists[activeIdx];

    // Reserve space at bottom for the add input
    float footerHeight = ImGui::GetFrameHeightWithSpacing() + 4.0f;
    ImGui::BeginChild("##ticker_list", ImVec2(0, -footerHeight), false);

    if (activeList.entities.empty()) {
        ImGui::TextDisabled("No tickers in this list.");
    }

    for (const auto& entity : activeList.entities) {
        // Tracked toggle
        bool tracked = entity.tracked;
        if (ImGui::Checkbox(("##track_" + entity.ticker).c_str(), &tracked))
            wm.SetTracked(activeIdx, entity.ticker, tracked);

        ImGui::SameLine();
        ImGui::TextUnformatted(entity.ticker.c_str());

        // Remove button, right-aligned
        float removeWidth = ImGui::CalcTextSize("x").x + ImGui::GetStyle().FramePadding.x * 2.0f;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - removeWidth);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
        if (ImGui::SmallButton(("x##" + entity.ticker).c_str()))
            wm.RemoveTicker(activeIdx, entity.ticker);
        ImGui::PopStyleColor(2);
    }

    ImGui::EndChild();

    ImGui::Separator();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 60.0f);

    bool commit = ImGui::InputText("##new_ticker", state.newTicker, sizeof(state.newTicker),
        ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CharsUppercase);

    ImGui::SameLine();
    commit |= ImGui::Button("Add", ImVec2(-1, 0));

    if (commit && state.newTicker[0] != '\0') {
        wm.AddTicker(activeIdx, state.newTicker);
        memset(state.newTicker, 0, sizeof(state.newTicker));
        ImGui::SetKeyboardFocusHere(-1);
    }

    ImGui::End();
}