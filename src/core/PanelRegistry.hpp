#pragma once
#include <entt/entt.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <fstream>

class PanelRegistry {
public:
    static PanelRegistry& Get();
    entt::registry& GetRegistry();
    void DrawAll();

    // Layout
    void SaveLayout(const std::string& path);
    void LoadLayout(const std::string& path);

    // User Settings & Saved Variables
    nlohmann::json& GetUserData(const std::string& panelKey);
    void SaveUserData(const std::string& path);
    void LoadUserData(const std::string& path);

private:
    PanelRegistry() = default;
    entt::registry m_registry;
    nlohmann::json   m_userData;
};