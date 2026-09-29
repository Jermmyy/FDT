#include "tile_renderer.hpp"
#undef min
#undef max
#include <Ultralight/Renderer.h>
#include <Ultralight/View.h>
#include <Ultralight/Buffer.h>
#include <Ultralight/Bitmap.h>
#include <algorithm>
#include <cassert>
#include <cstring>
#include <cmath>
#include <string>
#include <tracy/Tracy.hpp>

// Grab DX11 device from ImGui backend — call on main thread only
static ID3D11Device*        s_device  = nullptr;
static ID3D11DeviceContext* s_context = nullptr;

namespace Rendering
{

// ==================================================================
// TileRasterizer — static device setup
// ==================================================================

void TileRasterizer::SetDevice(ID3D11Device* dev, ID3D11DeviceContext* ctx)
{
    s_device  = dev;
    s_context = ctx;
}

TileRasterizer::TileRasterizer() = default;

// ==================================================================
// TileRasterizer::Init
//   Called on the main thread once.
//
//   Strategy: create the view at full document height so Ultralight
//   paints the entire document into one big bitmap. The worker then
//   reads different row-offsets from that bitmap per tile — no
//   scrolling, no per-tile render, no main-thread handshake needed.
// ==================================================================
void TileRasterizer::Init(ultralight::RefPtr<ultralight::Renderer> renderer,
                           const std::string& html,
                           int width, int height)
{
    m_renderer = renderer;

    ultralight::ViewConfig cfg;
    cfg.is_accelerated = false;
    cfg.is_transparent = false;
    cfg.initial_focus  = false;

    constexpr int k_max_load_polls = 200;
    constexpr int k_initial_height = 8192; // tall enough to trigger full layout

    // ── Phase 1: load at generous height to query true scrollHeight ──
    m_view = renderer->CreateView((uint32_t)width, (uint32_t)k_initial_height, cfg, nullptr);

    ultralight::String ul_html(html.c_str());
    m_view->LoadHTML(ul_html);

    for (int i = 0; i < k_max_load_polls && m_view->is_loading(); ++i)
    {
        renderer->Update();
        renderer->Render();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    renderer->Update();
    renderer->Render();

    ultralight::String js_exc;
    ultralight::String js_res = m_view->EvaluateScript(
        ultralight::String("document.body.scrollHeight.toString()"), &js_exc);

    std::string height_str = js_res.utf8().data();
    int queried_height     = height_str.empty() ? 0 : std::stoi(height_str);
    int true_height        = queried_height > 0 ? queried_height : height;
    m_doc_height           = true_height;

    // ── Phase 2: re-create view at exact document height ─────────────
    // Every pixel is now painted into the bitmap; worker reads row
    // offsets directly with no scrolling or re-rendering required.
    m_view = nullptr;
    m_view = renderer->CreateView((uint32_t)width, (uint32_t)true_height, cfg, nullptr);
    m_view->LoadHTML(ul_html);

    for (int i = 0; i < k_max_load_polls && m_view->is_loading(); ++i)
    {
        renderer->Update();
        renderer->Render();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    renderer->Update();
    renderer->Render();

    m_running = true;
    m_thread  = std::thread(&TileRasterizer::WorkerLoop, this);
}

// ==================================================================
// Shutdown / queue management
// ==================================================================

void TileRasterizer::Shutdown()
{
    if (!m_running) return;
    m_running = false;
    m_job_cv.notify_all();
    if (m_thread.joinable()) m_thread.join();

    {
        std::lock_guard<std::mutex> lock(m_job_mutex);
        std::queue<RasterJob> empty;
        std::swap(m_job_queue, empty);
    }
    {
        std::lock_guard<std::mutex> lock(m_result_mutex);
        m_results.clear();
    }

    m_view = nullptr;
    m_renderer = nullptr;
}

void TileRasterizer::Enqueue(RasterJob job)
{
    { std::lock_guard<std::mutex> lock(m_job_mutex); m_job_queue.push(job); }
    m_job_cv.notify_one();
}

void TileRasterizer::DrainResults(std::vector<RasterResult>& out)
{
    std::lock_guard<std::mutex> lock(m_result_mutex);
    out.insert(out.end(),
               std::make_move_iterator(m_results.begin()),
               std::make_move_iterator(m_results.end()));
    m_results.clear();
}

// ==================================================================
// Worker thread
// ==================================================================

void TileRasterizer::WorkerLoop()
{
    while (m_running)
    {
        RasterJob job;
        {
            std::unique_lock<std::mutex> lock(m_job_mutex);
            m_job_cv.wait(lock, [this]{ return !m_job_queue.empty() || !m_running; });
            if (!m_running && m_job_queue.empty()) break;
            job = m_job_queue.front();
            m_job_queue.pop();
        }

        RasterResult result = RasterizeTile(job);

        { std::lock_guard<std::mutex> lock(m_result_mutex); m_results.push_back(std::move(result)); }
    }
}

// ==================================================================
// RasterizeTile
//   The view bitmap contains the full document. We simply lock it and
//   copy the row range [tile_y .. tile_y + tile_h).
//
//   Ultralight bitmap pixel format: BGRA8 (kBitmapFormat_BGRA8_UNORM_sRGB)
//   D3D11 target format            : DXGI_FORMAT_R8G8B8A8_UNORM
//   → swap R and B channels during copy.
// ==================================================================
RasterResult TileRasterizer::RasterizeTile(const RasterJob& job)
{
    ZoneScopedN("TileRasterizer::RasterizeTile");
    RasterResult result;
    result.tile_index = job.tile_index;

    printf("[Worker] Rasterizing tile %d (doc_h: %d)\n", job.tile_index, job.doc_height);

    const int tile_y = job.tile_index * TILE_HEIGHT;
    const int tile_h = std::min(TILE_HEIGHT, job.doc_height - tile_y);

    if (tile_h <= 0 || job.doc_width <= 0 || !m_view)
    {
        printf("[Worker] ERROR: Invalid dimensions for tile %d (tile_h:%d, doc_w:%d)\n",
               job.tile_index, tile_h, job.doc_width);
        return result;
    }

    result.width  = job.doc_width;
    result.height = tile_h;
    result.pixels.resize((size_t)job.doc_width * tile_h, 0xFFFFFFFFu);

    ultralight::BitmapSurface* surface =
        static_cast<ultralight::BitmapSurface*>(m_view->surface());
    if (!surface)
    {
        printf("[Worker] ERROR: No surface for tile %d\n", job.tile_index);
        return result;
    }

    ultralight::RefPtr<ultralight::Bitmap> bitmap = surface->bitmap();
    if (!bitmap || bitmap->IsEmpty())
    {
        printf("[Worker] ERROR: Bitmap empty for tile %d\n", job.tile_index);
        return result;
    }

    void* raw = bitmap->LockPixels();
    if (!raw)
    {
        printf("[Worker] ERROR: LockPixels failed for tile %d\n", job.tile_index);
        return result;
    }

    const uint32_t src_stride   = bitmap->row_bytes();
    const uint8_t* src_base     = reinterpret_cast<const uint8_t*>(raw);
    const int      bitmap_h     = (int)bitmap->height();
    const int      rows_to_copy = std::min(tile_h, bitmap_h - tile_y);

    if (rows_to_copy <= 0)
    {
        bitmap->UnlockPixels();
        printf("[Worker] ERROR: tile_y=%d is beyond bitmap_h=%d for tile %d\n",
               tile_y, bitmap_h, job.tile_index);
        return result;
    }
    for (int y = 0; y < rows_to_copy; ++y)
    {
        // Offset into the bitmap by tile_y to read the correct document rows
        const uint32_t* src_row =
            reinterpret_cast<const uint32_t*>(src_base + (size_t)(tile_y + y) * src_stride);
        uint32_t* dst_row = result.pixels.data() + (size_t)y * job.doc_width;

        for (int x = 0; x < job.doc_width; ++x)
        {
            uint32_t px  = src_row[x];
            uint8_t  b   = (px      ) & 0xFF;
            uint8_t  g   = (px >>  8) & 0xFF;
            uint8_t  r   = (px >> 16) & 0xFF;
            uint8_t  a   = (px >> 24) & 0xFF;
            dst_row[x]   = ((uint32_t)a << 24) |
                           ((uint32_t)b << 16) |
                           ((uint32_t)g <<  8) |
                            (uint32_t)r;
        }
    }

    bitmap->UnlockPixels();
    printf("[Worker] Finished tile %d (%d rows from y=%d)\n", job.tile_index, rows_to_copy, tile_y);
    return result;
}

// ==================================================================
// UploadTile — unchanged; creates a D3D11 texture from CPU pixels
// ==================================================================
bool TileRasterizer::UploadTile(const RasterResult& result, Tile& out)
{
    ZoneScopedN("TileRasterizer::UploadTile");
    if (!s_device || result.pixels.empty()) return false;

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width            = (UINT)result.width;
    desc.Height           = (UINT)result.height;
    desc.MipLevels        = 1;
    desc.ArraySize        = 1;
    desc.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage            = D3D11_USAGE_DEFAULT;
    desc.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA init = {};
    init.pSysMem     = result.pixels.data();
    init.SysMemPitch = (UINT)(result.width * 4);

    ID3D11Texture2D* tex = nullptr;
    if (FAILED(s_device->CreateTexture2D(&desc, &init, &tex))) return false;

    ID3D11ShaderResourceView* srv = nullptr;
    if (FAILED(s_device->CreateShaderResourceView(tex, nullptr, &srv)))
    { tex->Release(); return false; }

    out.texture = tex;
    out.srv     = srv;
    out.width   = result.width;
    out.height  = result.height;
    out.index   = result.tile_index;
    out.ready   = true;
    return true;
}

// ==================================================================
// TileCache — unchanged; purely manages GPU-side lifetimes
// ==================================================================

bool TileCache::Has(int i) const      { return m_tiles.count(i) > 0; }
bool TileCache::IsQueued(int i) const { auto it = m_queued.find(i); return it != m_queued.end() && it->second; }
void TileCache::MarkQueued(int i)     { m_queued[i] = true; }

Tile* TileCache::Get(int i)
{
    auto it = m_tiles.find(i);
    if (it == m_tiles.end()) return nullptr;
    Touch(i);
    return &it->second;
}

void TileCache::Insert(int i, Tile tile)
{
    auto ex = m_tiles.find(i);
    if (ex != m_tiles.end()) ReleaseTile(ex->second);

    m_tiles[i]  = std::move(tile);
    m_queued[i] = false;

    m_lru_order.push_front(i);
    m_lru_map[i] = m_lru_order.begin();

    while ((int)m_lru_order.size() > TILE_MAX_VRAM)
    {
        int evict = m_lru_order.back();
        m_lru_order.pop_back();
        m_lru_map.erase(evict);
        ReleaseTile(m_tiles[evict]);
        m_tiles.erase(evict);
    }
}

void TileCache::Clear()
{
    for (auto& [idx, tile] : m_tiles) ReleaseTile(tile);
    m_tiles.clear(); m_queued.clear(); m_lru_order.clear(); m_lru_map.clear();
}

void TileCache::FlushReleased()
{
    for (auto& t : m_pending_release) ReleaseTile(t);
    m_pending_release.clear();
}

void TileCache::Touch(int i)
{
    auto it = m_lru_map.find(i);
    if (it == m_lru_map.end()) return;
    m_lru_order.erase(it->second);
    m_lru_order.push_front(i);
    m_lru_map[i] = m_lru_order.begin();
}

void TileCache::ReleaseTile(Tile& t)
{
    if (t.srv)     { t.srv->Release();     t.srv     = nullptr; }
    if (t.texture) { t.texture->Release(); t.texture = nullptr; }
    t.ready = false;
}

}
