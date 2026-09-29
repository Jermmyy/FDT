#pragma once

struct WatchlistPanelState {
	char newListName[64] = {};
	char newTicker[16] = {};
	bool showNewListInput = false;
};

void RenderWatchlistPanel(WatchlistPanelState& state);