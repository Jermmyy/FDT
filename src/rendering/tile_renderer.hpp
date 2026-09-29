#pragma once
#include <d3d11.h>
#include <Ultralight/Ultralight.h>
#include <imgui.h>
#include <unordered_map>
#include <list>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <vector>

//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// [SECTION] Tile Rendering System
//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// This system handles asynchronous rasterization of large HTML documents using Ultralight.
// Because rasterizing a massive 10,000px tall xbrl filing blocks the main thread, we chop the document into horizontal "Tiles" and rasterize them on a background worker thread.
//
// LIFETIME / THREADING:
// - Main Thread: Creates the TileRasterizer, pumps UI, manages D3D11 uploads (UploadTile).
// - Worker Thread: Owns the Ultralight rasterization loop. Spits out RasterResults.
//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------


namespace Rendering
{
    constexpr int TILE_HEIGHT    = 1024;
    constexpr int TILE_MAX_VRAM  = 12; // Max tiles kept in VRAM. ( TILE_MAX_VRAM * TILE_HEIGHT * ViewWidth * 4 bytes = ~VRAM footprint)
    constexpr int TILE_LOOKAHEAD = 2; // How many tiles before the fold we preemptively rasterize.

    // Represents a single uploaded GPU texture slice of the document.
    // OWNERSHIP: The underlying D3D11 resources are owned by the TileCache.
    struct Tile {
        int index = -1;
        ID3D11Texture2D* texture = nullptr; // FIXME: Raw pointer. Relying on TileCache::ReleaseTile() for cleanup.
        ID3D11ShaderResourceView* srv = nullptr;
        int width = 0;
        int height = 0;
        bool ready = false;
    };

    // Request sent FROM Main Thread TO Worker Thread.
    struct RasterJob {
        int tile_index = -1;
        int doc_width = 0;
        int doc_height = 0;
    };

    // Payload sent FROM Worker Thread to Main Thread.
    struct RasterResult {
        int tile_index = -1;
        int width = 0;
        int height = 0;
        // FIXME: Potentially massive heap allocation per tile  (~8MB for a 1920x1024 tile).
        // This vector allocation happens on the worker, is passed by value/move, and freed on main.
        // If memory fragmentation becomes an issue, we need a thread-safe Object Pool for these buffers. 
        std::vector<uint32_t> pixels; 
    };

    //----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    // TileCache
    // VRAM management for tiles. Uses a standard LRU (Least Recently Used) cache.
    // [MAIN THREAD ONLY]
    class TileCache {
    public:
        TileCache() = default;
        ~TileCache() { Clear(); }
        Tile* Get(int i);
        void Insert(int i, Tile tile);
        void Evict(int first, int last);
        bool Has(int i) const;
        void MarkQueued(int i);
        bool IsQueued(int i) const;
        void ResetQueued(int i) { m_queued[i] = false; }
        void Clear();
        // Must be called once per frame.
        // NOTE: We do deferred D3D11 release (m_pending_release) because a tile might be evicted from the LRU while the GPU is still executing a command buffer that uses it.
        void FlushReleased();
    private:
        std::list<int> m_lru_order;
        std::unordered_map<int, std::list<int>::iterator> m_lru_map;
        std::unordered_map<int, Tile> m_tiles;
        std::unordered_map<int, bool> m_queued;
        std::vector<Tile> m_pending_release;
        void ReleaseTile(Tile& t);
        void Touch(int i);
    };
    // FIXME 
    //----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    // TileRasterizer
    // Orchestrates the background Ultralight renderer
    //----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    class TileRasterizer {
    public:
        TileRasterizer();
        ~TileRasterizer() { Shutdown(); }

        // Must be called during engine init
        static void SetDevice(ID3D11Device* device, ID3D11DeviceContext* context);
        
        // Starts the worker thread.
        void Init(ultralight::RefPtr<ultralight::Renderer> renderer, const std::string& html, int width, int height);
        
        void Enqueue(RasterJob job); // [MAIN THREAD] Push a job to the worker. Non-blocking.
        void DrainResults(std::vector<RasterResult>& out); // [MAIN THREAD] Pops all finished rasterizations off the queue.
        bool UploadTile(const RasterResult& result, Tile& out); // [MAIN THREAD] Takes CPU pixels from RasterResult and does the actual D3D11 Map/Unmap or UpdateSubresource.
        void Shutdown();
        int  GetDocHeight() const { return m_doc_height; }

    private:
        void WorkerLoop();
        RasterResult RasterizeTile(const RasterJob& job);

        ultralight::RefPtr<ultralight::Renderer> m_renderer;
        ultralight::RefPtr<ultralight::View> m_view;
        std::atomic<int> m_doc_height { 0 };

        std::thread m_thread;
        std::atomic<bool> m_running { false };
        std::mutex m_job_mutex;
        std::condition_variable m_job_cv;
        std::queue<RasterJob> m_job_queue;
        std::mutex m_result_mutex;
        std::vector<RasterResult> m_results;
    };
}