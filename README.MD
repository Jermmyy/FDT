# Data Terminal

A custom desktop data terminal for real-time financial and intelligence monitoring

---

## Tech Stack

- **C++**
- **ImGui** : modified docking branch fork (provided in source)
- **Ultralight** : HTML rendering engine (may be replaced in the future)

## Features

### SEC Filing Viewer
IXBRL stripping -> Ultralight render -> tiled rasterization on worker threads -> SRV upload -> LRU VRAM cache -> per-frame viewport compositing.

*Potentially use AVX2 as we are parsing large documents*

### Alert System
`AlertBus` singleton, thread-safe (surely) queue drained on render thread, `std::function´ subscriber fan-out. Feeds feed panel, audio engine, toast overlay.

### Alert Feed Panel
`ImDrawList`-only rows, `GetTime()`-based fast-attack/decay flash envelope, left-edge type stripe, source pill badge, per-type filters, ring buffer with head eviction.

### Rates Panels
Various panels for monitoring interest rate data.
 
---
 
*More to be documented as built.*