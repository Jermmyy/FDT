#pragma once
#include <string>
#include <vector>
#include <unordered_map>

struct ChatMessage {
	std::string sender;
	std::string text;
	std::string timestamp;
};

struct ChatPanelState {
	std::string activeChannel = "main";
	std::vector<std::string> channels = { "main", "off-topic" };
	std::unordered_map < std::string, std::vector<ChatMessage>> roomMessages;
	char inputBuffer[256] = { 0 };
};