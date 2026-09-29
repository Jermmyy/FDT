#include "CommandBar.hpp"
#include "../core/CommandRegistry.hpp"
#include "UIStyle.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include <string>
#include <cstring>
#include <cmath>

extern ImFont* g_TerminalFont;

static const ImVec4 kAmber      = ImVec4(1.0f,  0.7f,  0.0f,  1.0f);
static const ImVec4 kAmberDim   = ImVec4(0.6f,  0.42f, 0.0f,  1.0f);
static const ImVec4 kAmberFaint = ImVec4(0.35f, 0.24f, 0.0f,  1.0f);
static const ImVec4 kRed        = ImVec4(0.9f,  0.3f,  0.3f,  1.0f);
static const ImVec4 kBgSuggest  = ImVec4(0.06f, 0.06f, 0.06f, 1.0f);
static const ImVec4 kBgHighlight= ImVec4(0.18f, 0.12f, 0.0f,  1.0f);

void RenderCommandBar()
{
    static char  s_buf[64]    = {};
    static int   s_len        = 0;
    static bool  s_active     = false;
    static bool  s_badCmd     = false;
    static float s_errorTimer = 0.0f;
    static float s_blink      = 0.0f;
    static int   s_selIdx     = -1;   // currently highlighted suggestion (-1 = none)

    const ImGuiIO& io = ImGui::GetIO();

    const float kBarHeight = UI::Metrics::CommandBarHeight();
    const float kRowHeight = UI::Metrics::CommandBarWidth();
    const float kFontSize = UI::Metrics::FontSize();
    const float kMaxSugRows  = 8.f;   // dropdown shows at most this many rows

    ImFont* font = g_TerminalFont ? g_TerminalFont : ImGui::GetFont();
    float   charW = font->CalcTextSizeA(kFontSize, FLT_MAX, 0.f, "A").x;

    // ── Hotkey ───────────────────────────────────────────────────
    if (!s_active && ImGui::IsKeyPressed(ImGuiKey_F1))
    {
        s_active = true;
        s_badCmd = false;
        s_len    = 0;
        s_selIdx = -1;
        memset(s_buf, 0, sizeof(s_buf));
    }

    // ── Build suggestion list (used for input handling + drawing) ─
    std::vector<const CommandEntry*> suggestions;
    if (s_active && s_len > 0)
        suggestions = CommandRegistry::Get().GetSuggestions(s_buf);

    // Clamp selection index
    if (s_selIdx >= (int)suggestions.size()) s_selIdx = (int)suggestions.size() - 1;

    // ── Input handling ───────────────────────────────────────────
    if (s_active)
    {
        if (ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            s_active = false;
            s_len    = 0;
            s_selIdx = -1;
            memset(s_buf, 0, sizeof(s_buf));
        }
        else if (ImGui::IsKeyPressed(ImGuiKey_Backspace) && s_len > 0)
        {
            s_buf[--s_len] = '\0';
            s_badCmd = false;
            s_selIdx = -1;
        }
        else if (ImGui::IsKeyPressed(ImGuiKey_Tab) && !suggestions.empty())
        {
            // Autocomplete: pick highlighted or first suggestion
            int pick = (s_selIdx >= 0) ? s_selIdx : 0;
            const CommandEntry* e = suggestions[pick];

            auto parsed = CommandRegistry::Parse(s_buf);
            std::string completed;

            if (!parsed.hasSpace)
            {
                // Completing a group — append space so user can type panel next
                completed = e->group + " ";
            }
            else
            {
                // Completing a panel within an already-typed group
                completed = e->group + " " + e->command;
            }

            s_len = (int)completed.size();
            if (s_len > (int)sizeof(s_buf) - 1) s_len = (int)sizeof(s_buf) - 1;
            memcpy(s_buf, completed.c_str(), s_len);
            s_buf[s_len] = '\0';
            s_selIdx = -1;
            s_badCmd = false;
        }
        else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) && !suggestions.empty())
        {
            s_selIdx = (s_selIdx + 1) % (int)suggestions.size();
        }
        else if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) && !suggestions.empty())
        {
            s_selIdx = (s_selIdx <= 0) ? (int)suggestions.size() - 1 : s_selIdx - 1;
        }
        else if (ImGui::IsKeyPressed(ImGuiKey_Enter) && s_len > 0)
        {
            // If an item is highlighted, use that
            if (s_selIdx >= 0 && s_selIdx < (int)suggestions.size())
            {
                const CommandEntry* e = suggestions[s_selIdx];
                std::string full = e->group + " " + e->command;
                CommandRegistry::Get().Execute(full);
            }
            else
            {
                if (!CommandRegistry::Get().Execute(s_buf))
                {
                    s_badCmd     = true;
                    s_errorTimer = 1.2f;
                }
            }
            s_active = false;
            s_len    = 0;
            s_selIdx = -1;
            memset(s_buf, 0, sizeof(s_buf));
        }
        else
        {
            for (const ImWchar* c = io.InputQueueCharacters.begin();
                 c != io.InputQueueCharacters.end(); ++c)
            {
                if (*c >= 32 && *c < 127 && s_len < (int)sizeof(s_buf) - 1)
                {
                    char ch = (char)*c;
                    if (ch >= 'a' && ch <= 'z') ch -= 32;
                    s_buf[s_len++] = ch;
                    s_buf[s_len]   = '\0';
                    s_badCmd = false;
                    s_selIdx = -1;
                }
            }
        }

        s_blink += io.DeltaTime;
    }

    if (s_badCmd)
    {
        s_errorTimer -= io.DeltaTime;
        if (s_errorTimer <= 0.f) s_badCmd = false;
    }

    // ── Draw command bar ─────────────────────────────────────────
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, kBarHeight));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(10.f, 0.f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,   0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 1.f));

    const ImGuiWindowFlags kFlags =
        ImGuiWindowFlags_NoDecoration      |
        ImGuiWindowFlags_NoMove            |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoNav             |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::Begin("##cmdbar", nullptr, kFlags);

    ImGui::PushFont(font, kFontSize);
    float fsize = ImGui::GetFontSize();

    ImDrawList* dl     = ImGui::GetWindowDrawList();
    ImVec2      winPos = ImGui::GetWindowPos();
    float       textY  = winPos.y + (kBarHeight - fsize) * 0.5f;
    float       textX  = winPos.x + 10.f;

    if (s_active)
    {
        ImVec4 textCol = s_badCmd ? UI::Colors::ColorNegative : UI::Colors::Amber;

        // Typed text
        dl->AddText(font, fsize,
                    ImVec2(textX, textY),
                    ImGui::ColorConvertFloat4ToU32(textCol),
                    s_buf, s_buf + s_len);

        float cursorX = textX + s_len * charW;

        // Ghost hint: show remaining part of top suggestion after cursor
        if (!suggestions.empty() && s_selIdx < 0)
        {
            const CommandEntry* top = suggestions[0];
            auto parsed = CommandRegistry::Parse(s_buf);
            std::string ghost;

            if (!parsed.hasSpace)
            {
                // Still typing group — ghost the rest of the group name + space
                if (top->group.size() > parsed.group.size())
                    ghost = top->group.substr(parsed.group.size()) + " ";
                else
                    ghost = " ";  // group matched exactly, hint the space
            }
            else
            {
                // Typing panel — ghost the rest of the panel name
                if (top->command.size() > parsed.panel.size())
                    ghost = top->command.substr(parsed.panel.size());
            }

            if (!ghost.empty())
                dl->AddText(font, fsize,
                            ImVec2(cursorX, textY),
                            ImGui::ColorConvertFloat4ToU32(kAmberFaint),
                            ghost.c_str());
        }

        // Block cursor
        if (fmodf(s_blink, 1.0f) < 0.5f)
        {
            dl->AddRectFilled(
                ImVec2(cursorX, textY),
                ImVec2(cursorX + charW, textY + fsize),
                ImGui::ColorConvertFloat4ToU32(kAmber));
        }
    }
    else
    {
        const char* hint    = s_badCmd ? "INVALID COMMAND" : "PRESS F1 TO ENTER COMMAND";
        ImVec4      hintCol = s_badCmd ? kRed : kAmberDim;
        float       hintW   = font->CalcTextSizeA(fsize, FLT_MAX, 0.f, hint).x;
        float       hintX   = io.DisplaySize.x - hintW - 10.f;

        dl->AddText(font, fsize,
                    ImVec2(hintX, textY),
                    ImGui::ColorConvertFloat4ToU32(hintCol),
                    hint);
    }

    // Bottom separator
    dl->AddLine(
        ImVec2(winPos.x,                    winPos.y + kBarHeight - 1.f),
        ImVec2(winPos.x + io.DisplaySize.x, winPos.y + kBarHeight - 1.f),
        ImGui::ColorConvertFloat4ToU32(ImVec4(0.25f, 0.25f, 0.25f, 1.f)));

    ImGui::PopFont();
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);

    // ── Draw suggestion dropdown via foreground drawlist (always on top) ──
    if (s_active && !suggestions.empty())
    {
        int   visRows = (int)ImMin((float)suggestions.size(), kMaxSugRows);
        float dropW   = io.DisplaySize.x;       // flush with command bar
        float dropH   = visRows * kRowHeight;
        float dropX   = 0.f;
        float dropY   = ImFloor(kBarHeight);    // floor kills any sub-pixel gap

        // Foreground drawlist renders above dockspace and all other windows
        ImDrawList* sdl = ImGui::GetForegroundDrawList();

        auto parsed = CommandRegistry::Parse(s_buf);

        // Background — same pure black as the bar
        sdl->AddRectFilled(
            ImVec2(dropX, dropY),
            ImVec2(dropX + dropW, dropY + dropH),
            ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 1.f)));

        for (int i = 0; i < visRows; ++i)
        {
            const CommandEntry* e = suggestions[i];
            float rowY = dropY + i * kRowHeight;
            float rowX = dropX;

            // Highlight selected row
            if (i == s_selIdx)
                sdl->AddRectFilled(
                    ImVec2(rowX, rowY),
                    ImVec2(rowX + dropW, rowY + kRowHeight),
                    ImGui::ColorConvertFloat4ToU32(kBgHighlight));

            float cy = rowY + (kRowHeight - fsize) * 0.5f;
            float cx = rowX + 10.f;

            if (!parsed.hasSpace)
            {
                // Group-level: "GROUP  ->  description"
                sdl->AddText(font, fsize, ImVec2(cx, cy),
                             ImGui::ColorConvertFloat4ToU32(kAmber),
                             e->group.c_str());

                float gw = font->CalcTextSizeA(fsize, FLT_MAX, 0.f, e->group.c_str()).x;
                const char* arrow = "  ->  ";
                float aw = font->CalcTextSizeA(fsize, FLT_MAX, 0.f, arrow).x;
                sdl->AddText(font, fsize, ImVec2(cx + gw, cy),
                             ImGui::ColorConvertFloat4ToU32(kAmberDim),
                             arrow);
                sdl->AddText(font, fsize, ImVec2(cx + gw + aw, cy),
                             ImGui::ColorConvertFloat4ToU32(kAmberDim),
                             e->description.c_str());
            }
            else
            {
                // Panel-level: "GROUP PANEL  description"
                std::string label = e->group + " " + e->command;
                float lw = font->CalcTextSizeA(fsize, FLT_MAX, 0.f, label.c_str()).x;
                sdl->AddText(font, fsize, ImVec2(cx, cy),
                             ImGui::ColorConvertFloat4ToU32(kAmber),
                             label.c_str());
                std::string desc = "  " + e->description;
                sdl->AddText(font, fsize, ImVec2(cx + lw, cy),
                             ImGui::ColorConvertFloat4ToU32(kAmberDim),
                             desc.c_str());
            }

            // Row separator — same colour as bar\'s bottom line
            sdl->AddLine(
                ImVec2(rowX,         rowY + kRowHeight - 1.f),
                ImVec2(rowX + dropW, rowY + kRowHeight - 1.f),
                ImGui::ColorConvertFloat4ToU32(ImVec4(0.25f, 0.25f, 0.25f, 1.f)));
        }
    }
}
