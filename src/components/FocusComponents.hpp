#pragma once

#include <memory>
#include <string>

class FocusClient;

struct FocusClientComp {
	std::shared_ptr<FocusClient> client;
};

struct FocusRenderStateComp {
	double lastPrice = 0.0;
	float flashTimer = 0.0f;
	int flashDir = 0;
};