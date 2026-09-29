#include "SecfilingPanel.hpp"
#include "../core/AlertBus.hpp"
#include "../core/PanelRegistry.hpp"
#include "../components/PanelComponents.hpp"
#include "../components/SecComponents.hpp"
#include "imgui.h"
#include <chrono>

static const char* k_form_types[] = { "all", "10-K", "10-Q", "8-K", "20-F", "DEF 14A", "S-1" };

void InitSecFilingPanel(SecFilingPanelState& state, const std::string& user_agent) {
    state.api.SetUserAgent(user_agent);
}

static void PollAsync(SecFilingPanelState& s) {
    using namespace std::chrono_literals;

    if (s.company_state == SEC::FetchState::Loading && s.company_future.valid()) {
        if (s.company_future.wait_for(0ms) == std::future_status::ready) {
            try {
                s.companies = s.company_future.get();
                s.company_state = SEC::FetchState::Done;
            } catch (const std::exception& e) {
                s.error_msg = e.what();
                s.company_state = SEC::FetchState::Error;
            }
        }
    }

     if (s.filings_state == SEC::FetchState::Loading && s.filings_future.valid()) {
        if (s.filings_future.wait_for(0ms) == std::future_status::ready) {
            try {
                s.filings       = s.filings_future.get();
                s.filings_state = SEC::FetchState::Done;
            } catch (const std::exception& e) {
                s.error_msg     = e.what();
                s.filings_state = SEC::FetchState::Error;
            }
        }
    }

    if (s.doc_state == SEC::FetchState::Loading && s.doc_future.valid()) {
        if (s.doc_future.wait_for(0ms) == std::future_status::ready) {
            try {
                s.doc_future.get();
                s.doc_state = SEC::FetchState::Done;
            } catch (const std::exception& e) {
                s.error_msg = e.what();
                s.doc_state = SEC::FetchState::Error;
            }
        }
    }
}


void RenderSecFilingPanel(SecFilingPanelState& s) {
    PollAsync(s);

    float left_w  = 300.0f;
    float avail_y = ImGui::GetContentRegionAvail().y - 24.0f;

    // Left and right child both need to have borders enabled
    // Left Child Flags
    ImGuiChildFlags lFlags = ImGuiChildFlags_ResizeX | ImGuiChildFlags_AlwaysUseWindowPadding | ImGuiChildFlags_Borders;

    // ---- Left column ----
    ImGui::BeginChild("##sec_left", ImVec2(left_w, avail_y), lFlags);

    // Search bar
    ImGui::Text("Company Search");
    ImGui::Separator();
    ImGui::Spacing();

    // Press enter to start typing if panel focused
    if (ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_Enter)) {
        ImGui::SetKeyboardFocusHere();
    }

    ImGui::SetNextItemWidth(-80.0f);
    bool enter = ImGui::InputText("##search", s.search_buf, sizeof(s.search_buf),
                                   ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();

    bool searching = (s.company_state == SEC::FetchState::Loading);
    if (searching) ImGui::BeginDisabled();
    if (ImGui::Button("Search", ImVec2(70.0f, 0)) || enter) {
        if (s.search_buf[0] != '\0' && !searching) {
            s.error_msg        = "";
            s.companies.clear();
            s.filings.clear();
            s.selected_company = -1;
            s.selected_filing  = -1;
            s.company_state    = SEC::FetchState::Loading;
            std::string query  = s.search_buf;
            s.company_future   = std::async(std::launch::async, [&s, query]() {
                return s.api.SearchCompany(query);
            });
        }
    }
    if (searching) ImGui::EndDisabled();

    // Form type filter
    ImGui::Spacing();
    ImGui::Text("Form:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(110.0f);
    if (ImGui::BeginCombo("##form", k_form_types[s.form_filter_idx])) {
        for (int i = 0; i < IM_ARRAYSIZE(k_form_types); ++i) {
            bool sel = (s.form_filter_idx == i);
            if (ImGui::Selectable(k_form_types[i], sel))
                s.form_filter_idx = i;
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::Spacing();

    // Company list
    ImGui::Text("Companies");
    ImGui::Separator();

    if (s.company_state == SEC::FetchState::Loading) {
        ImGui::TextDisabled("Searching...");
    } else if (s.companies.empty()) {
        ImGui::TextDisabled("No results.");
    } else {
        ImGui::BeginChild("##companies", ImVec2(0, 160.0f), false);
        for (int i = 0; i < (int)s.companies.size(); ++i) {
            const auto& c = s.companies[i];
            char label[256];
            if (!c.ticker.empty())
                snprintf(label, sizeof(label), "%-6s  %s##c%d", c.ticker.c_str(), c.name.c_str(), i);
            else
                snprintf(label, sizeof(label), "%s##c%d", c.name.c_str(), i);

            if (ImGui::Selectable(label, s.selected_company == i)) {
                s.selected_company = i;
                s.selected_filing  = -1;
                s.renderer.Clear();
                s.filings.clear();
                s.filings_state    = SEC::FetchState::Loading;
                std::string cik    = c.cik;
                std::string form   = (s.form_filter_idx == 0) ? "" : k_form_types[s.form_filter_idx];
                s.filings_future   = std::async(std::launch::async, [&s, cik, form]() {
                    return s.api.GetFilings(cik, form, 50);
                });
            }
        }
        ImGui::EndChild();
    }

    ImGui::Spacing();

    // Filings table
    ImGui::Text("Filings");
    ImGui::Separator();

    if (s.selected_company < 0) {
        ImGui::TextDisabled("Select a company.");
    } else if (s.filings_state == SEC::FetchState::Loading) {
        ImGui::TextDisabled("Loading...");
    } else if (s.filings.empty()) {
        ImGui::TextDisabled("No filings found.");
    } else {
        float remaining = ImGui::GetContentRegionAvail().y - 4.0f;
        ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg
                              | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit;

        if (ImGui::BeginTable("##filings", 3, flags, ImVec2(0, remaining))) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("Form", ImGuiTableColumnFlags_WidthFixed, 58.0f);
            ImGui::TableSetupColumn("Date", ImGuiTableColumnFlags_WidthFixed, 88.0f);
            ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            for (int i = 0; i < (int)s.filings.size(); ++i) {
                const auto& f = s.filings[i];
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                char sel_id[64];
                snprintf(sel_id, sizeof(sel_id), "%s##f%d", f.form_type.c_str(), i);
                if (ImGui::Selectable(sel_id, s.selected_filing == i,
                                    ImGuiSelectableFlags_SpanAllColumns)) {
                    s.selected_filing = i;
                    s.doc_state       = SEC::FetchState::Loading;
                    s.renderer.Clear();
                    s.error_msg       = "";
                    std::string url   = f.primary_doc_url;
                    s.doc_future = std::async(std::launch::async, [&s, url]() {
                        std::string html = s.api.FetchDocument(url);
                        s.renderer.LoadHTML(html);

                        AlertBus::Get().Post(AlertType::SEC, "RENDER", "Form 4 rasterization complete.");
                    });
                }

                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(f.filing_date.c_str());

                ImGui::TableSetColumnIndex(2);
                if (!f.description.empty())
                    ImGui::TextUnformatted(f.description.c_str());
                else
                    ImGui::TextDisabled("-");
            }
            ImGui::EndTable();
        }
    }

    ImGui::EndChild();

    ImGui::SameLine(0, 0);

    // Right Child Flags
    ImGuiChildFlags rFlags = ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding;

    // ---- Right column: document viewer ----
    ImGui::BeginChild("##sec_right", ImVec2(0.0f, avail_y), rFlags);

    if (s.doc_state == SEC::FetchState::Loading) {
        ImGui::TextDisabled("Fetching document...");
    } else if (s.doc_state == SEC::FetchState::Error) {
        ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "Error: %s", s.error_msg.c_str());
    } else if (s.doc_state == SEC::FetchState::Done) {
        s.renderer.Render(ImGui::GetContentRegionAvail());
    } else {
        ImGui::TextDisabled("Select a filing to view.");
    }

    ImGui::EndChild();

    // ---- Status bar ----
    ImGui::Separator();
    if (!s.error_msg.empty()) {
        ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "  %s", s.error_msg.c_str());
    } else if (s.doc_state == SEC::FetchState::Loading) {
        ImGui::TextDisabled("  Fetching document...");
    } else if (s.selected_filing >= 0 && s.selected_company >= 0
               && s.doc_state == SEC::FetchState::Done) {
        ImGui::TextDisabled("  %s  |  %s  |  %s",
            s.filings[s.selected_filing].form_type.c_str(),
            s.filings[s.selected_filing].filing_date.c_str(),
            s.companies[s.selected_company].name.c_str());
    } else {
        ImGui::TextDisabled("  SEC Filing Viewer");
    }
}

namespace UI
{
    void SpawnSecFilingPanel(ultralight::Renderer* renderer)
    {
        auto& registry = PanelRegistry::Get().GetRegistry();
        std::string uniqueTitle = "SEC Filings##sec";

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

        auto state = std::make_shared<SecFilingPanelState>();
        InitSecFilingPanel(*state, "PersonalDataTerminal Jeremy.feytens@gmail.com");
        state->renderer.Init(renderer);
        std::printf(">>> SEC STATE CREATED: Memory Allocated\n");

        registry.emplace<SecClientComp>(entity, state);

        // Render loop callback
        registry.emplace<PanelRenderFn>(entity, [](entt::registry& reg, entt::entity ent) {
            auto& comp = reg.get<SecClientComp>(ent);
            comp.state->renderer.GetTileCache().FlushReleased();
            RenderSecFilingPanel(*comp.state);
            });

        // Automatic cleanup on close
        registry.emplace<PanelDestroyFn>(entity, [renderer](entt::registry&, entt::entity) {
            if (renderer) {
                renderer->PurgeMemory();
                std::printf(">>> ULTRALIGHT MEMORY PURGED\n");
            }
            });
    }
}