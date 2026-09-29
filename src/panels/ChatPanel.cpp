#include "ChatPanel.hpp"
#include "../networking/NetworkClient.hpp"
#include "../components/ChatComponents.hpp"
#include "../components/PanelComponents.hpp"
#include "../core/PanelRegistry.hpp"
#include <imgui.h>
#include <algorithm>
#include <tracy/Tracy.hpp>

void RenderChatPanel(entt::registry& reg, entt::entity entity) {
    ZoneScopedN("RenderChatPanel");
    auto& state = reg.get<ChatPanelState>(entity);

    std::vector<NetworkMessage> incoming;
    if (NetworkClient::Get().PollIncoming(incoming)) {
        for (const auto& msg : incoming) {
            std::string chan = msg.channel.empty() ? "main" : msg.channel;

            // Auto-create channel tab if a message arrives for an unseen room
            if (std::find(state.channels.begin(), state.channels.end(), chan) == state.channels.end()) {
                state.channels.push_back(chan);
            }
            state.roomMessages[chan].push_back({ msg.sender, msg.text, msg.timestamp });
        }
    }


    ImGui::BeginChild("ChannelSidebar", ImVec2(120, 0), true);
    ImGui::TextDisabled("Channels");
    ImGui::Separator();

    for (const auto& ch : state.channels) {
        bool isSelected = (state.activeChannel == ch);
        std::string label = (ch == "main" || ch == "off-topic") ? "#" + ch : ch;
        if (ImGui::Selectable(label.c_str(), isSelected))
            state.activeChannel = ch;
    }
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginGroup();
    {
        float footerHeight = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();

        ImGui::BeginChild("ChatScrollRegion", ImVec2(0, -footerHeight), false, ImGuiWindowFlags_HorizontalScrollbar);

        ZoneScopedN("Render chat messages");

        auto& messages = state.roomMessages[state.activeChannel];
        for (const auto& msg : messages) {
            ImGui::TextDisabled("[%s]", msg.timestamp.c_str());
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s:", msg.sender.c_str());
            ImGui::SameLine();
            ImGui::TextUnformatted(msg.text.c_str());
        }

        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);

        ImGui::EndChild();
        ImGui::Separator();

        bool reclaim_focus = false;
        auto processSend = [&state]() {
            std::string text(state.inputBuffer);
            if (!text.empty()) {
                // 2. Dispatch message payload to background network thread
                NetworkClient::Get().SendMessagePayload("You", text, state.activeChannel);

                memset(state.inputBuffer, 0, sizeof(state.inputBuffer));
            }
        };
        if (ImGui::InputText("##ChatInput", state.inputBuffer, sizeof(state.inputBuffer),
            ImGuiInputTextFlags_EnterReturnsTrue, nullptr, nullptr)) {
            processSend();
            reclaim_focus = true;
        }

        ImGui::SameLine();
        if (ImGui::Button("Send")) {
            processSend();
            reclaim_focus = true;
        }

        if (reclaim_focus)
            ImGui::SetKeyboardFocusHere(-1);
    }
    ImGui::EndGroup();
}

namespace UI {
    void OpenChatPanel(const std::string& targetChannel) {
        auto& registry = PanelRegistry::Get().GetRegistry();

        // 1. Check if already open
        auto view = registry.view<ChatPanelState, WindowComp>();
        entt::entity chatEntity = entt::null;
        for (auto entity : view) { chatEntity = entity; break; }

        // 2. Already open
        if (chatEntity != entt::null) {
            auto& state = registry.get<ChatPanelState>(chatEntity);

            // Case A: plain "CHAT" while open -> close it
            if (targetChannel.empty()) {
                registry.get<WindowComp>(chatEntity).isOpen = false;
                return;
            }

            // Case B: "CHAT <channel>" while open -> switch to or add that channel
            auto it = std::find(state.channels.begin(), state.channels.end(), targetChannel);
            if (it == state.channels.end()) {
                state.channels.push_back(targetChannel);
                state.roomMessages[targetChannel].push_back({ "System", "Joined room: " + targetChannel, "12:00:00" });
            }
            state.activeChannel = targetChannel;
            return;
        }

        // 3. Closed — spawn fresh, read saved state from userdata
        auto& ud = PanelRegistry::Get().GetUserData("CHAT");

        ChatPanelState initialState;

        // Restore saved channels
        if (ud.contains("channels"))
            initialState.channels = ud["channels"].get<std::vector<std::string>>();

        // Make sure "main" always exists
        if (std::find(initialState.channels.begin(), initialState.channels.end(), "main") == initialState.channels.end())
            initialState.channels.insert(initialState.channels.begin(), "main");

        // Restore saved active channel
        if (ud.contains("activeChannel"))
            initialState.activeChannel = ud["activeChannel"].get<std::string>();
        else
            initialState.activeChannel = "main";

        // Handle command arg
        if (!targetChannel.empty()) {
            auto it = std::find(initialState.channels.begin(), initialState.channels.end(), targetChannel);
            if (it == initialState.channels.end()) {
                initialState.channels.push_back(targetChannel);
                initialState.roomMessages[targetChannel].push_back({ "System", "Joined room: " + targetChannel, "12:00:00" });
            }
            initialState.activeChannel = targetChannel;
        }

        initialState.roomMessages["main"].push_back({ "System", "Connected to #main", "12:00:00" });

        chatEntity = registry.create();
        registry.emplace<WindowComp>(chatEntity, "Chat", true);
        registry.emplace<PanelMeta>(chatEntity, PanelMeta{ "CHAT", targetChannel });
        registry.emplace<ChatPanelState>(chatEntity, std::move(initialState));

        registry.emplace<PanelRenderFn>(chatEntity, [](entt::registry& reg, entt::entity ent) {
            RenderChatPanel(reg, ent);
            });

        // Save channels and active channel when panel closes
        registry.emplace<PanelDestroyFn>(chatEntity, [](entt::registry& reg, entt::entity ent) {
            auto& state = reg.get<ChatPanelState>(ent);
            auto& ud = PanelRegistry::Get().GetUserData("CHAT");
            ud["channels"] = state.channels;
            ud["activeChannel"] = state.activeChannel;
            // messages are transient, not saved
            });
    }
}