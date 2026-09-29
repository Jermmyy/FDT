#define NOMINMAX
#include "filing_renderer.h"
#include <imgui_internal.h>
#include <regex>
#include <algorithm>

namespace Rendering
{

// k_master_css is still useful — inject it via LoadHTML wrapping the HTML in a <style> block
const char* FilingRenderer::k_master_css = R"css(
* {
    box-sizing: border-box;
}
html, body {
    font-family: 'Arial', 'Helvetica', sans-serif;
    font-size: 14px;
    color: #000000;
    background: #ffffff;
    margin: 10px;
    line-height: normal;
}
div  { display: block; }
span { display: inline; }
table {
    display: table;
    border-collapse: collapse;
    width: 100%;
    margin: 0;
    font-size: inherit;
}
tr            { display: table-row; }
td, th {
    display: table-cell;
    padding: 0;
    border: none;
    vertical-align: bottom;
}
tr:nth-child(even) td { background-color: transparent; }
b, strong { font-weight: bold; }
i, em     { font-style: italic; }
a         { color: #0000ee; text-decoration: underline; }
)css";

FilingRenderer::FilingRenderer() = default;

// ------------------------------------------------------------------
// Init — store the Ultralight renderer, called once after GPU setup
// ------------------------------------------------------------------
void FilingRenderer::Init(ultralight::RefPtr<ultralight::Renderer> renderer)
{
    m_renderer = renderer;
}

// ------------------------------------------------------------------
// Clear
// ------------------------------------------------------------------
void FilingRenderer::Clear()
{
    m_rasterizer.Shutdown();
    m_tile_cache.Clear();
    m_clean_html.clear();
    m_doc_width      = 0.0f;
    m_doc_height     = 0.0f;
    m_last_width     = 0;
    m_rasterizer_init = false;
}

// ------------------------------------------------------------------
// StripIXBRLTags — unchanged, still useful for cleaning SEC filings
// ------------------------------------------------------------------
std::string FilingRenderer::StripIXBRLTags(const std::string& raw_html) const
{
    std::string html = raw_html;

    std::regex xml_re(R"(<\?xml[^>]*\?>)", std::regex_constants::icase);
    html = std::regex_replace(html, xml_re, "");

    std::regex html_re(R"(<html[^>]*>)", std::regex_constants::icase);
    html = std::regex_replace(html, html_re, "<html>");

    std::vector<std::string> metadata = { "ix:header", "ix:hidden", "ix:resources", "link:linkbaseRef" };
    for (const auto& tag : metadata) {
        std::regex re("<" + tag + R"([\s\S]*?</)" + tag + ">", std::regex_constants::icase);
        html = std::regex_replace(html, re, "");
    }

    std::regex ix_tags(R"(</?ix:[^>]*>)", std::regex_constants::icase);
    html = std::regex_replace(html, ix_tags, "");

    return html;
}

// ------------------------------------------------------------------
// InjectCSS — wraps our master CSS into the HTML <head>
// Ultralight doesn't have a separate master_css parameter like litehtml,
// so we inject it as a <style> block.
// ------------------------------------------------------------------
static std::string InjectCSS(const std::string& html, const char* css)
{
    std::string style_block = std::string("<style>") + css + "</style>";

    // Try to insert just before </head>
    auto pos = html.find("</head>");
    if (pos == std::string::npos)
        pos = html.find("</HEAD>");

    if (pos != std::string::npos)
    {
        std::string result = html;
        result.insert(pos, style_block);
        return result;
    }

    // Fallback: prepend to the whole document
    return style_block + html;
}

// ------------------------------------------------------------------
// LoadHTML
// ------------------------------------------------------------------
void FilingRenderer::LoadHTML(const std::string& raw_html)
{
    // Tear down previous state
    m_rasterizer.Shutdown();
    m_tile_cache.Clear();
    m_rasterizer_init = false;
    m_last_width      = 0;
    m_doc_width       = 0.0f;
    m_doc_height      = 0.0f;

    // Clean + inject CSS
    m_clean_html = InjectCSS(StripIXBRLTags(raw_html), k_master_css);
}

// ------------------------------------------------------------------
// Render — main thread, called every frame
// ------------------------------------------------------------------
void FilingRenderer::Render(ImVec2 available_size)
{
    if (available_size.x <= 0 || available_size.y <= 0) return;
    if (m_clean_html.empty())
    {
        ImGui::TextDisabled("No document loaded.");
        return;
    }
    if (!m_renderer)
    {
        ImGui::TextDisabled("Renderer not initialised.");
        return;
    }

    // ── 1. (Re-)init rasterizer on first frame or width change ───
    int render_width = (int)available_size.x;
    if (render_width != m_last_width)
    {
        m_rasterizer.Shutdown();
        m_tile_cache.Clear();
        m_rasterizer_init = false;

        // Ultralight determines doc height after the View loads and renders.
        // We use available_size as the viewport; the view will expand vertically.
        m_doc_width  = available_size.x;
        m_doc_height = available_size.y; // will grow once view renders
        m_last_width = render_width;
    }

    // ── 2. Init rasterizer once per layout pass ──────────────────
    if (!m_rasterizer_init)
    {
        // Pass a tall initial height so Ultralight renders the full document.
        // After Init() the JS query has already run and GetDocHeight() is valid.
        m_rasterizer.Init(m_renderer, m_clean_html,
                          (int)m_doc_width, TILE_HEIGHT);

        int true_height = m_rasterizer.GetDocHeight();
        if (true_height > 0)
            m_doc_height = (float)true_height;

        m_rasterizer_init = true;
    }

    // ── 3. Upload completed tiles from worker thread ─────────────
    {
        std::vector<RasterResult> results;
        m_rasterizer.DrainResults(results);
        for (auto& res : results)
        {
            printf("[Main] Drained result for tile %d (w:%d, h:%d, pixels:%zu)\n", 
                   res.tile_index, res.width, res.height, res.pixels.size());

            if (res.tile_index == 0 && res.height > 0)
                m_doc_width = (float)res.width;

            Tile tile;
            if (m_rasterizer.UploadTile(res, tile))
            {
                printf("[Main] Successfully uploaded tile %d to GPU\n", res.tile_index);
                m_tile_cache.Insert(res.tile_index, std::move(tile));
            }
            else
            {
                printf("[Main] ERROR: Upload failed for tile %d! Resetting queued state.\n", res.tile_index);
                m_tile_cache.ResetQueued(res.tile_index);
            }
        }
    }

    // ── 4. Scrollable child window ────────────────────────────────
    ImGui::SetNextWindowContentSize(ImVec2(m_doc_width, m_doc_height));
    ImGui::BeginChild("##filing_scroll", available_size, false,
                       ImGuiWindowFlags_HorizontalScrollbar);

    float scroll_y = ImGui::GetScrollY();
    float scroll_x = ImGui::GetScrollX();

    // ── 5. Visible + lookahead tile range ─────────────────────────
    int tile_count = (int)std::ceil(m_doc_height / TILE_HEIGHT) + 1;
    int tile_first = std::max(0, (int)(scroll_y / TILE_HEIGHT) - TILE_LOOKAHEAD);
    int tile_last  = std::min(tile_count - 1,
                              (int)((scroll_y + available_size.y) / TILE_HEIGHT)
                              + TILE_LOOKAHEAD);

    m_tile_cache.FlushReleased();

    // ── 7. Enqueue missing tiles ──────────────────────────────────
    for (int t = tile_first; t <= tile_last; t++)
    {
        if (!m_tile_cache.Has(t) && !m_tile_cache.IsQueued(t))
        {
            printf("[Main] Enqueueing missing tile %d\n", t);
            m_tile_cache.MarkQueued(t);
            RasterJob job;
            job.tile_index = t;
            job.doc_width  = (int)m_doc_width;
            job.doc_height = (int)m_doc_height;
            m_rasterizer.Enqueue(job);
        }
    }

    // ── 8. Composite tiles ────────────────────────────────────────
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 window_pos     = ImGui::GetCursorScreenPos();
    window_pos.x         -= scroll_x;

    int vis_first = std::max(0, (int)(scroll_y / TILE_HEIGHT));
    int vis_last  = std::min(tile_count - 1,
                             (int)((scroll_y + available_size.y) / TILE_HEIGHT));

    // if (ImGui::IsWindowAppearing() || ImGui::GetIO().MouseWheel != 0.0f) {
    //     printf("[Render] ScrollY: %.1f, WinH: %.1f | Visible Tiles: %d to %d (Total: %d)\n", 
    //         scroll_y, available_size.y, vis_first, vis_last, tile_count);
    // }

    ImGui::Dummy(ImVec2(m_doc_width, m_doc_height));

    ImVec2 content_start = ImGui::GetItemRectMin();

    for (int t = vis_first; t <= vis_last; t++)
    {
        float tile_y_doc = (float)(t * TILE_HEIGHT);
        
        // NO MORE window_pos, NO MORE scroll_y subtraction here!
        ImVec2 tl = { content_start.x, content_start.y + tile_y_doc };

        Tile* tile = m_tile_cache.Get(t);
        if (tile && tile->ready)
        {
            // Use tile->width/height to handle the last tile which might be shorter
            ImVec2 br = { tl.x + tile->width, tl.y + tile->height };
            draw_list->AddImage((ImTextureID)tile->srv, tl, br);
        }
        else
        {
            // Placeholder for missing/rendering tiles
            float ph_h = std::min((float)TILE_HEIGHT, m_doc_height - tile_y_doc);
            ImVec2 br  = { tl.x + m_doc_width, tl.y + ph_h };
            draw_list->AddRectFilled(tl, br, IM_COL32(17, 17, 17, 255));
        }
    }

    ImGui::EndChild();
    }

} // namespace Rendering