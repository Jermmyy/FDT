#pragma once
#include <string>
#include <vector>
#include <memory>
#include "../CommandRegistry.hpp"

// Forward Declaration
class FinnhubProvider;

namespace WatchlistCommands {
	CommandEntry Parse(const std::string& token, std::shared_ptr<FinnhubProvider> provider);
}