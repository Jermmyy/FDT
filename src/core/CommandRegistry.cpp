#include "CommandRegistry.hpp"
#include <algorithm>

CommandRegistry& CommandRegistry::Get()
{
    static CommandRegistry instance;
    return instance;
}

void CommandRegistry::Register(const std::string& group, const std::string& command, const std::string& description, std::function<void()> action)
{
    std::string grp = group, cmd = command;
    std::transform(grp.begin(), grp.end(), grp.begin(), ::toupper);
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);

    m_staticActions[{grp, cmd}] = action;
    m_entries.push_back({ grp, cmd, description });
}

void CommandRegistry::RegisterDynamic(const std::string& group, const std::string& description, std::function<void(const std::string& arg)> action)
{
    std::string grp = group;
    std::transform(grp.begin(), grp.end(), grp.begin(), ::toupper);

    m_dynamicGroups[grp] = action;

    // Use an empty string for the command field so it displays just the group name
    m_entries.push_back({ grp, "", description });
}

CommandRegistry::ParsedInput CommandRegistry::Parse(const std::string& raw)
{
    ParsedInput p;
    p.hasSpace = false;
    auto spacePos = raw.find(' ');
    if (spacePos == std::string::npos)
    {
        p.group = raw;
    }
    else
    {
        p.group = raw.substr(0, spacePos);
        p.panel = raw.substr(spacePos + 1);
        p.hasSpace = true;
        while (!p.panel.empty() && p.panel.front() == ' ')
            p.panel.erase(p.panel.begin());
    }
    std::transform(p.group.begin(), p.group.end(), p.group.begin(), ::toupper);
    std::transform(p.panel.begin(), p.panel.end(), p.panel.begin(), ::toupper);
    return p;
}

bool CommandRegistry::Execute(const std::string& raw)
{
    auto p = Parse(raw);
    if (p.group.empty()) return false;

    // 1. If a specific sub-command/argument is provided (e.g., CHAT INTC, FOCUS AAPL)
    if (!p.panel.empty())
    {
        // Check static actions
        auto it = m_staticActions.find({ p.group, p.panel });
        if (it != m_staticActions.end())
        {
            it->second();
            return true;
        }

        // Check dynamic groups with an argument
        auto dynIt = m_dynamicGroups.find(p.group);
        if (dynIt != m_dynamicGroups.end())
        {
            dynIt->second(p.panel);
            return true;
        }
    }
    // 2. If ONLY the group name is provided with no argument (e.g., FILINGS or CHAT)
    else
    {
        // First, check if there are static actions for this group and execute ALL of them
        bool executedAny = false;
        for (const auto& [key, action] : m_staticActions)
        {
            if (key.first == p.group)
            {
                action();
                executedAny = true;
            }
        }
        if (executedAny) return true;

        // Second, check dynamic groups with an empty argument (like plain "CHAT")
        auto dynIt = m_dynamicGroups.find(p.group);
        if (dynIt != m_dynamicGroups.end())
        {
            dynIt->second("");
            return true;
        }
    }

    return false;
}

std::vector<const CommandEntry*> CommandRegistry::GetSuggestions(const std::string& raw) const
{
    auto p = Parse(raw);
    std::vector<const CommandEntry*> results;

    if (!p.hasSpace)
    {
        std::vector<std::string> seenGroups;
        for (const auto& e : m_entries)
        {
            if (e.group.rfind(p.group, 0) == 0 && std::find(seenGroups.begin(), seenGroups.end(), e.group) == seenGroups.end())
            {
                results.push_back(&e);
                seenGroups.push_back(e.group);
            }
        }
        for (const auto& [grp, _] : m_dynamicGroups)
        {
            if (grp.rfind(p.group, 0) == 0 && std::find(seenGroups.begin(), seenGroups.end(), grp) == seenGroups.end())
            {
                static CommandEntry placeholder;
                placeholder = { grp, grp, "Dynamic command group" };
                results.push_back(&placeholder);
                seenGroups.push_back(grp);
            }
        }
    }
    else
    {
        for (const auto& e : m_entries)
        {
            if (e.group == p.group && (p.panel.empty() || e.command.rfind(p.panel, 0) == 0))
            {
                results.push_back(&e);
            }
        }
    }
    return results;
}


const std::vector<CommandEntry>& CommandRegistry::Entries() const
{
    return m_entries;
}

std::vector<std::string> CommandRegistry::GetGroups() const
{
    std::vector<std::string> groups;

    // 1. Gather from static entries
    for (const auto& e : m_entries)
    {
        if (std::find(groups.begin(), groups.end(), e.group) == groups.end())
            groups.push_back(e.group);
    }

    // 2. Gather from dynamic groups (like FOCUS)
    for (const auto& [grp, _] : m_dynamicGroups)
    {
        if (std::find(groups.begin(), groups.end(), grp) == groups.end())
            groups.push_back(grp);
    }

    std::sort(groups.begin(), groups.end());
    return groups;
}

