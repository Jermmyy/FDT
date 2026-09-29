#pragma once
#include <string>

class IEquityProvider;

struct ListDisplayPanelState {
	std::string listName;
};

void RenderListDisplayPanel(void* instance, IEquityProvider* provider);