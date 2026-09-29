#include <imgui.h>
#include "NetworkWire.hpp"

// Helper function to draw raw memory as a hex view
static void DrawHexDump(const char* data, size_t size) {
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);

    const int bytesPerRow = 16;
    for (size_t i = 0; i < size; i += bytesPerRow) {
        ImGui::Text("%04zX: ", i);
        ImGui::SameLine();

        for (size_t j = 0; j < bytesPerRow; ++j) {
            if (i + j < size)
                ImGui::Text("%02X ", (unsigned char)data[i + j]);
            else
                ImGui::Text("   ");
            ImGui::SameLine();
        }

        ImGui::SameLine(0.0f, 20.0f);

        std::string ascii;
        for (size_t j = 0; j < bytesPerRow && i + j < size; ++j) {
            char c = data[i + j];
            ascii += (c >= 32 && c <= 126) ? c : '.';
        }
        ImGui::TextUnformatted(ascii.c_str());
    }

    ImGui::PopFont();
}

static ImVec4 GetPacketColor(WirePacketType type) {
    switch (type) {
    case WirePacketType::Info:      return ImVec4(0.7f, 0.7f, 0.7f, 1.0f); // Gray
    case WirePacketType::HeaderOut: return ImVec4(0.4f, 0.8f, 1.0f, 1.0f); // Light Blue
    case WirePacketType::HeaderIn:  return ImVec4(0.4f, 1.0f, 0.4f, 1.0f); // Light Green
    case WirePacketType::DataOut:   return ImVec4(0.2f, 0.5f, 1.0f, 1.0f); // Blue
    case WirePacketType::DataIn:    return ImVec4(0.2f, 0.8f, 0.2f, 1.0f); // Green
    case WirePacketType::SslOut:
    case WirePacketType::SslIn:     return ImVec4(0.8f, 0.4f, 1.0f, 1.0f); // Purple
    default:                        return ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // White
    }
}

static const char* GetPacketTypeName(WirePacketType type) {
    switch (type) {
    case WirePacketType::Info:      return "INFO";
    case WirePacketType::HeaderOut: return "HDR OUT";
    case WirePacketType::HeaderIn:  return "HDR IN";
    case WirePacketType::DataOut:   return "DATA OUT";
    case WirePacketType::DataIn:    return "DATA IN";
    case WirePacketType::SslOut:    return "SSL OUT";
    case WirePacketType::SslIn:     return "SSL IN";
    default:                        return "UNKNOWN";
    }
}

void DrawWireSharkPanel(bool* p_open) {
    if (!ImGui::Begin("Raw Network Inspector", p_open)) {
        ImGui::End();
        return;
    }

    if (ImGui::Button("Clear Traffic")) {
        NetworkWire::Get().Clear();
    }

    ImGui::Separator();

    static int selectedPacketIndex = -1;
    auto packets = NetworkWire::Get().GetPacketsCopy();

    ImGui::BeginChild("PacketList", ImVec2(0, ImGui::GetContentRegionAvail().y * 0.5f), true);
    if (ImGui::BeginTable("WireTable", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("Time(ms)", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 60.0f);
        ImGui::TableSetupColumn("Preview");
        ImGui::TableHeadersRow();

        for (int i = 0; i < packets.size(); i++) {
            const auto& p = packets[i];

            ImGui::PushID(i);

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%.2f", p.timestampMs);

            ImGui::TableSetColumnIndex(1);
            ImGui::TextColored(GetPacketColor(p.type), "%s", GetPacketTypeName(p.type));

            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%zu B", p.size);

            ImGui::TableSetColumnIndex(3);

            std::string preview = p.data.substr(0, 50);
            preview.erase(std::remove(preview.begin(), preview.end(), '\n'), preview.end());
            preview.erase(std::remove(preview.begin(), preview.end(), '\r'), preview.end());

            if (ImGui::Selectable(preview.empty() ? "(binary/empty)" : preview.c_str(), selectedPacketIndex == i, ImGuiSelectableFlags_SpanAllColumns)) {
                selectedPacketIndex = i;
            }

            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();

    // Bottom Pane: Hex Dump / Raw Data
    ImGui::BeginChild("PacketDetails", ImVec2(0, 0), true);
    if (selectedPacketIndex >= 0 && selectedPacketIndex < packets.size()) {
        const auto& p = packets[selectedPacketIndex];
        ImGui::Text("Type: %s | Size: %zu bytes | Time: %.2f ms", GetPacketTypeName(p.type), p.size, p.timestampMs);
        ImGui::Separator();

        // If it's a Header or Info, it's usually safe to print as raw text
        if (p.type == WirePacketType::HeaderIn || p.type == WirePacketType::HeaderOut || p.type == WirePacketType::Info) {
            ImGui::TextWrapped("%s", p.data.c_str());
        }
        // If it's Data or SSL, HexDump it to avoid ImGui choking on null terminators or weird bytes
        else {
            DrawHexDump(p.data.c_str(), p.data.size());
        }
    }
    else {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Select a packet to view details...");
    }
    ImGui::EndChild();

    ImGui::End();
}