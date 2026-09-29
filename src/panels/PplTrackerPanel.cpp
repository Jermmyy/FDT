#include "PplTrackerPanel.hpp"
#include "../rss/RSSPoller.hpp"
#include "../core/PanelRegistry.hpp"
#include "../components/PanelComponents.hpp"
#include "../components/PplTrackerComponents.hpp"
#include <imgui.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <shellapi.h>
#endif

static void OpenUrl(const std::string& url) {
#ifdef _WIN32
    ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#endif
}

namespace UI {

    static void DrawManageWindow(PplTrackerPanelState& state) {
        ImGui::SetNextWindowSize(ImVec2(360, 300), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Tracked Users", &state.showManageWindow, ImGuiWindowFlags_NoCollapse)) {
            ImGui::End();
            return;
        }

        static char nameBuf[64] = {};
        static bool resolving = false;

        ImGui::Text("Add Target");
        ImGui::SetNextItemWidth(-80.0f);
        bool enter = ImGui::InputText("##username", nameBuf, sizeof(nameBuf),
            ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();

        if (resolving) ImGui::BeginDisabled();
        if ((ImGui::Button("Add", ImVec2(70.0f, 0)) || enter) && nameBuf[0] != '\0') {
            std::string username = nameBuf;
            nameBuf[0] = '\0';
            std::thread([username]() {
                RSSPoller::Get().AddUser(username);
                }).detach();
        }
        if (resolving) ImGui::EndDisabled();

        ImGui::TextDisabled("Mirror is resolved automatically.");
        ImGui::Separator();
        ImGui::Text("Current Targets:");

        ImGui::BeginChild("##userlist", ImVec2(0, 0), false);
        auto users = RSSPoller::Get().GetUsersCopy();
        for (const auto& user : users) {
            ImGui::PushID(user.username.c_str());
            if (ImGui::SmallButton("Remove"))
                RSSPoller::Get().RemoveUser(user.username);
            ImGui::SameLine();
            ImGui::TextUnformatted(user.username.c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("  %s", user.feedUrl.c_str());
            ImGui::PopID();
        }
        ImGui::EndChild();
        ImGui::End();
    }

    void DrawPplTracker(PplTrackerPanelState& state) {
        if (ImGui::Button("Manage Targets"))
            state.showManageWindow = !state.showManageWindow;
        ImGui::SameLine();
        if (ImGui::Button("Clear Feed"))
            RSSPoller::Get().ClearFeed();

        ImGui::Separator();

        ImGui::BeginChild("##feed", ImVec2(0, 0), false);
        auto posts = RSSPoller::Get().GetPostsCopy();

        if (posts.empty()) {
            ImGui::TextDisabled("No posts yet. Add targets in Manage Targets.");
        }

        for (const auto& post : posts) {
            ImGui::PushID(post.guid.c_str());

            // Author + date
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.6f, 1.0f), "[%s]", post.author.c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("%s", post.pubDate.c_str());

            // Content
            if (!post.title.empty())
                ImGui::TextWrapped("%s", post.title.c_str());
            else
                ImGui::TextWrapped("%s", post.content.c_str());

            // Post link
            if (!post.link.empty()) {
                if (ImGui::SmallButton("Open Post"))
                    OpenUrl(post.link);
            }

            // Image links
            for (int i = 0; i < (int)post.imageUrls.size(); i++) {
                ImGui::SameLine();
                char label[32];
                snprintf(label, sizeof(label), "Image %d", i + 1);
                if (ImGui::SmallButton(label))
                    OpenUrl(post.imageUrls[i]);
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::PopID();
        }

        ImGui::EndChild();

        if (state.showManageWindow)
            DrawManageWindow(state);
    }

    void SpawnPplTrackerPanel() {
        auto& registry = PanelRegistry::Get().GetRegistry();
        std::string uniqueTitle = "Leak Feed##ppl";

        // Toggle check: close if already open
        auto view = registry.view<WindowComp>();
        for (auto [entity, win] : view.each()) {
            if (win.title == uniqueTitle) {
                registry.destroy(entity);
                return;
            }
        }

        // Create fresh entity
        auto entity = registry.create();
        registry.emplace<WindowComp>(entity, uniqueTitle, true);
        registry.emplace<PplTrackerComp>(entity);

        // Render loop callback
        registry.emplace<PanelRenderFn>(entity, [](entt::registry& reg, entt::entity ent) {
            auto& comp = reg.get<PplTrackerComp>(ent);
            DrawPplTracker(comp.state);
            });
    }
}