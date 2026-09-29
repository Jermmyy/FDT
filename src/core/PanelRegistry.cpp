#include "PanelRegistry.hpp"
#include "../components/PanelComponents.hpp"
#include "CommandRegistry.hpp"
#include <imgui.h>
#include <fstream>
#include <string>

#include <tracy/Tracy.hpp>

PanelRegistry& PanelRegistry::Get() {
    static PanelRegistry instance;
    return instance;
}

entt::registry& PanelRegistry::GetRegistry() {
    return m_registry;
}

void PanelRegistry::DrawAll() {
    ZoneScopedN("PanelRegistry::DrawAll");

    auto view = m_registry.view<WindowComp, PanelRenderFn>();

    view.each([this](entt::entity entity, WindowComp& win, PanelRenderFn& renderer) {
        ZoneScopedN("Panel Renderer");

        if (!win.isOpen) {
            if (auto* destroyer = m_registry.try_get<PanelDestroyFn>(entity)) {
                destroyer->fn(m_registry, entity);
            }
            m_registry.destroy(entity);
            return;
        }

        ImGuiWindowFlags flags = win.flags | ImGuiWindowFlags_NoCollapse;

        if (ImGui::Begin(win.title.c_str(), nullptr, flags)) {
            renderer.fn(m_registry, entity);
        }
        ImGui::End();
        });
}

void PanelRegistry::SaveLayout(const std::string& path) {
    std::ofstream f(path);
    if (!f.is_open()) return;

    auto view = m_registry.view<PanelMeta>();
    for (auto [entity, meta] : view.each()) {
        f << meta.command;
        if (!meta.arg.empty()) {
            f << " " << meta.arg;
        }
        f << "\n";
    }
}

void PanelRegistry::LoadLayout(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return;

    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty()) {
            CommandRegistry::Get().Execute(line);
        }
    }
}

nlohmann::json& PanelRegistry::GetUserData(const std::string& panelKey) {
    return m_userData[panelKey];
}

void PanelRegistry::SaveUserData(const std::string& path) {
    std::ofstream f(path);
    f << m_userData.dump(4);
}

void PanelRegistry::LoadUserData(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return;
    try { m_userData = nlohmann::json::parse(f); }
    catch (...) { m_userData = {}; }
}