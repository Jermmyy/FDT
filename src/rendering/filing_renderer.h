#pragma once
#include <Ultralight/Ultralight.h>
#include <imgui.h>
#include <string>
#include <memory>
#include "tile_renderer.hpp"

namespace Rendering
{

    class FilingRenderer
    {
    public:
        FilingRenderer();
        ~FilingRenderer() { Clear(); }

        void Init(ultralight::RefPtr<ultralight::Renderer> renderer);
        void LoadHTML(const std::string& raw_html);
        void Render(ImVec2 available_size);
        void Clear();
        TileCache& GetTileCache() { return m_tile_cache; }

    private:
        std::string StripIXBRLTags(const std::string& raw_html) const;

        static const char* k_master_css;

        // Ultralight
        ultralight::RefPtr<ultralight::Renderer> m_renderer;
        std::string  m_clean_html;   // stored so we can re-init on width change
        float        m_doc_height  = 0.0f;
        float        m_doc_width   = 0.0f;
        int          m_last_width  = 0;

        // Tiling
        TileCache      m_tile_cache;
        TileRasterizer m_rasterizer;
        bool           m_rasterizer_init = false;
    };
}