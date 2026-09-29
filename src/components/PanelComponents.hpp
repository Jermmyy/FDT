#pragma once
#include <string>
#include <functional>
#include <entt/entt.hpp>
#include <imgui.h>

struct WindowComp
{
    std::string title;
    bool isOpen = true;
    bool closable = true; // This is purely to make the "x" disappear
    ImGuiWindowFlags flags = ImGuiWindowFlags_None;
};

struct PanelMeta {
    std::string command;
    std::string arg;
};

struct TickerComp {
    std::string symbol;
};

struct LinkGroupComp {
    int groupId = 0;
};

struct PanelRenderFn {
    std::function<void(entt::registry&, entt::entity)> fn;
};

struct PanelDestroyFn {
    std::function<void(entt::registry&, entt::entity)> fn;
};