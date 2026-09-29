#pragma once
#include "imgui.h"
#include <implot.h>

namespace UI {
    // === Typography
    struct Typography
    {
        ImFont* Title = nullptr;
        ImFont* Header = nullptr;
        ImFont* Body = nullptr;
        ImFont* Muted = nullptr;
    };

    inline Typography Fonts;

    // Base palette
    namespace Colors
    {
        static const ImVec4 Amber = ImVec4(1.0f, 0.7f, 0.0f, 1.0f);
        static const ImVec4 DeepBlack = ImVec4(0.06f, 0.06f, 0.06f, 1.0f);
        static const ImVec4 DarkGrey = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);
        static const ImVec4 MidGrey = ImVec4(0.20f, 0.20f, 0.20f, 1.0f);
        static const ImVec4 TextMuted = ImVec4(0.50f, 0.50f, 0.50f, 1.0f);
        static const ImVec4 TextDim = ImVec4(0.30f, 0.30f, 0.30f, 1.0f);
        static const ImVec4 BorderCol = ImVec4(0.30f, 0.30f, 0.30f, 1.0f);
        static const ImVec4 BorderHi = ImVec4(0.55f, 0.55f, 0.55f, 1.0f);
        static const ImVec4 White = ImVec4(1.00f, 1.00f, 1.00f, 1.0f);

        // State colors
        static const ImVec4 ColorPositive = ImVec4(0.20f, 0.85f, 0.45f, 1.0f); // green
        static const ImVec4 ColorNegative = ImVec4(1.00f, 0.25f, 0.25f, 1.0f); // red
        static const ImVec4 ColorNeutral = ImVec4(0.65f, 0.65f, 0.65f, 1.0f); // grey

        static const ImVec4 SuggestionBg = ImVec4(0.06f, 0.06f, 0.06f, 1.0f);
        static const ImVec4 HighlightBg = ImVec4(0.18f, 0.12f, 0.0f, 1.0f);
    };

    struct Metrics {
        // Base Scaling
        static float Scale() { return 1.0f; }

        static float FontSize() { return ImGui::GetFontSize(); }

        static float CommandBarHeight() { return FontSize() * 2.0f * Scale(); }
        static float CommandBarWidth() { return FontSize() * 1.8f * Scale(); }

        static float Pad() { return 8.0f * Scale(); }
    };

    // Apply Theme

    int GetDockSpaceFlags();

    inline void ApplyTheme()
    {
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigWindowsMoveFromTitleBarOnly = true;
        io.ConfigDockingAlwaysTabBar = false;

        ImGuiStyle& s = ImGui::GetStyle();

        // Geometry
        s.WindowRounding = 0.0f;
        s.FrameRounding = 0.0f;
        s.ChildRounding = 0.0f;
        s.ScrollbarRounding = 0.0f;
        s.GrabRounding = 0.0f;
        s.TabRounding = 0.0f;
        s.TabBorderSize = 0.0f;
        s.FrameBorderSize = 0.0f;

        s.WindowPadding = ImVec2(8.0f, 8.0f);
        s.FramePadding = ImVec2(4.0f, 3.0f);
        s.ItemSpacing = ImVec2(4.0f, 4.0f);
        s.ScrollbarSize = 10.0f;

        // Core surface
        s.Colors[ImGuiCol_WindowBg] = Colors::DeepBlack;
        s.Colors[ImGuiCol_ChildBg] = Colors::DeepBlack;
        s.Colors[ImGuiCol_PopupBg] = Colors::DarkGrey;

        // Border
        s.Colors[ImGuiCol_Border] = Colors::BorderCol;
        s.Colors[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);

        // Text
        s.Colors[ImGuiCol_Text] = Colors::Amber;
        s.Colors[ImGuiCol_TextDisabled] = Colors::TextDim;

        // Frames
        s.Colors[ImGuiCol_FrameBg] = Colors::DarkGrey;
        s.Colors[ImGuiCol_FrameBgHovered] = Colors::MidGrey;
        s.Colors[ImGuiCol_FrameBgActive] = Colors::MidGrey;

        // Title bars
        s.Colors[ImGuiCol_TitleBg] = Colors::DeepBlack;
        s.Colors[ImGuiCol_TitleBgActive] = Colors::DarkGrey;
        s.Colors[ImGuiCol_TitleBgCollapsed] = Colors::DeepBlack;

        // Tabs
        s.Colors[ImGuiCol_Tab] = Colors::DeepBlack;
        s.Colors[ImGuiCol_TabHovered] = Colors::MidGrey;
        s.Colors[ImGuiCol_TabActive] = Colors::DarkGrey;
        s.Colors[ImGuiCol_TabUnfocused] = Colors::DeepBlack;
        s.Colors[ImGuiCol_TabUnfocusedActive] = Colors::DeepBlack;

        // Buttons
        s.Colors[ImGuiCol_Button] = Colors::DarkGrey;
        s.Colors[ImGuiCol_ButtonHovered] = Colors::MidGrey;
        s.Colors[ImGuiCol_ButtonActive] = ImVec4(0.28f, 0.28f, 0.28f, 1.0f);

        // Headers
        s.Colors[ImGuiCol_Header] = Colors::DarkGrey;
        s.Colors[ImGuiCol_HeaderHovered] = Colors::MidGrey;
        s.Colors[ImGuiCol_HeaderActive] = Colors::MidGrey;

        // Scrollbar
        s.Colors[ImGuiCol_ScrollbarBg] = Colors::DeepBlack;
        s.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.35f, 0.35f, 0.35f, 1.0f);
        s.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.50f, 0.50f, 0.50f, 1.0f);
        s.Colors[ImGuiCol_ScrollbarGrabActive] = Colors::Amber;

        // Checkmark & Slider grab
        s.Colors[ImGuiCol_CheckMark] = Colors::Amber;
        s.Colors[ImGuiCol_SliderGrab] = Colors::Amber;
        s.Colors[ImGuiCol_SliderGrabActive] = Colors::White;

        // Separator
        s.Colors[ImGuiCol_Separator] = Colors::BorderCol;
        s.Colors[ImGuiCol_SeparatorHovered] = Colors::BorderHi;
        s.Colors[ImGuiCol_SeparatorActive] = Colors::Amber;

        // Resize grip
        s.Colors[ImGuiCol_ResizeGrip] = ImVec4(0, 0, 0, 0);
        s.Colors[ImGuiCol_ResizeGripHovered] = Colors::BorderHi;
        s.Colors[ImGuiCol_ResizeGripActive] = Colors::Amber;

        // Text selection
        s.Colors[ImGuiCol_TextSelectedBg] = ImVec4(Colors::Amber.x, Colors::Amber.y, Colors::Amber.z, 0.25f);

        // Docking
        s.Colors[ImGuiCol_DockingPreview] = ImVec4(Colors::Amber.x, Colors::Amber.y, Colors::Amber.z, 0.30f);
        s.Colors[ImGuiCol_DockingEmptyBg] = Colors::DeepBlack;

        // Implot
        ImPlotStyle& ps = ImPlot::GetStyle();
        ps.Colors[ImPlotCol_PlotBg] = Colors::DeepBlack;
        ps.Colors[ImPlotCol_FrameBg] = ImVec4(0.05f, 0.05f, 0.05f, 1.0f);
        ps.Colors[ImPlotCol_AxisText] = Colors::Amber;
        ps.Colors[ImPlotCol_AxisGrid] = ImVec4(Colors::BorderCol.x, Colors::BorderCol.y, Colors::BorderCol.z, 0.4f);
        ps.Colors[ImPlotCol_AxisTick] = ImVec4(Colors::Amber.x, Colors::Amber.y, Colors::Amber.z, 0.5f);
        ps.Colors[ImPlotCol_PlotBorder] = Colors::BorderCol;
        ps.PlotPadding = ImVec2(8.0f, 8.0f);
    }
}