#pragma once
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <map>

struct CommandEntry
{
	std::string group;
	std::string command;
	std::string description;
};

class CommandRegistry
{
public:
	static CommandRegistry& Get();

	void Register(const std::string& group, const std::string& command, const std::string& description, std::function<void()> action);
	void RegisterDynamic(const std::string& group, const std::string& description, std::function<void(const std::string& arg)> action);
	struct ParsedInput
	{
		std::string group;
		std::string panel;
		bool hasSpace;
	};

	static ParsedInput Parse(const std::string& raw);
	bool Execute(const std::string& raw);
	std::vector<const CommandEntry*> GetSuggestions(const std::string& raw) const;
	std::vector<std::string> GetGroups() const;
	const std::vector<CommandEntry>& Entries() const;

private:
	CommandRegistry() = default;

	std::map<std::pair<std::string, std::string>, std::function<void()>> m_staticActions;
	std::unordered_map<std::string, std::function<void(const std::string&)>> m_dynamicGroups;
	std::vector<CommandEntry> m_entries;
};