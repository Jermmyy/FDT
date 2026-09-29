#include "ListDisplayPanel.hpp"
#include "../core/WatchlistManager.hpp"
#include "../api/EquityProvider.hpp"
#include <imgui.h>

void RenderListDisplayPanel(void* instance, IEquityProvider* provider) {
	auto* state = static_cast<ListDisplayPanelState*>(instance);
	if (!state || !provider) return;

	auto& wm = WatchlistManager::Get();

	auto lists = wm.GetListsCopy();
	const WatchList* targetList = nullptr;
	for (const auto& l : lists) {
		if (l.name == state->listName) {
			targetList = &l;
			break;
		}
	}

	std::string windowId = "List: " + state->listName + "###" + state->listName;
	if (!ImGui::Begin(windowId.c_str())) {
		ImGui::End();
		return;
	}

	if (!targetList) {
		ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "List '%s' not found.", state->listName.c_str());
		ImGui::End();
		return;
	}

	static ImGuiTableFlags tableFlags = ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable |
		ImGuiTableFlags_Sortable | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |ImGuiTableFlags_ScrollY;

	if (ImGui::BeginTable("##list_table", 4, tableFlags)) {
		ImGui::TableSetupColumn("Ticker", ImGuiTableColumnFlags_WidthFixed, 70.0f);
		ImGui::TableSetupColumn("Price", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Change", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("% Chg", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableHeadersRow();

		for (const auto& entity : targetList->entities) {
			EquityQuote quote = provider->GetLatest(entity.ticker);

			if (!quote.valid) {
				provider->Subscribe(entity.ticker);
			}

			ImGui::TableNextRow();

			ImGui::TableNextColumn();
			ImGui::TextUnformatted(entity.ticker.c_str());

			ImVec4 color = ImVec4(1, 1, 1, 1);
			if (quote.change > 0)      color = ImVec4(0.2f, 0.8f, 0.2f, 1.0f); // Green
			else if (quote.change < 0) color = ImVec4(0.9f, 0.1f, 0.1f, 1.0f); // Red

			ImGui::TableNextColumn();
			if (quote.valid) ImGui::TextColored(color, "%.2f", quote.price);
			else ImGui::TextDisabled("Loading...");

			ImGui::TableNextColumn();
			if (quote.valid) ImGui::TextColored(color, "%s%.2f", (quote.change >= 0 ? "+" : ""), quote.change);

			ImGui::TableNextColumn();
			if (quote.valid) ImGui::TextColored(color, "%.2f%%", quote.changePct);
		}
		ImGui::EndTable();
	}

	ImGui::End();
}