# Canvas spike (M0)

This is the canvas half of the M0 Raven go/no-go (`docs/roadmap.md`, M0; method in `openspec/changes/m0-foundations/design.md` D9). It measures Hefesto's own Vulkan renderer against capability `hefesto-performance`, requirement "Interactive frame rate": the median frame time is at most 16.7 ms while panning and zooming a layout of at least 10 million rectangles. The decision itself is in `docs/decisions/raven-go-no-go.md`.

**Status, 2026-11-07:** measured on one macOS machine only (Apple M3 Pro). Linux and the reference M1 Mac are not measured yet.

## What was built

| Part | Where | What it does |
|---|---|---|
| Renderer | `src/render/vulkan/` (`hefesto_render_vk`) | Instanced quads: one 16-byte instance per rectangle (`int32 x0, y0, x1, y1`), 6 vertices per instance, no index buffer. One `vkCmdDraw` per range, one push-constant block per layer (color plus view transform). Straight-alpha blending in layer order. Sub-pixel rectangles are drawn one pixel wide. The shader subtracts an integer origin before it converts to float, so precision depends on the distance to the view center rather than on the absolute coordinate. The public headers are Vulkan-free. |
| Canvas index | `hefesto/render/canvas_index.hpp` | Tile culling plus level of detail. See below. |
| Synthetic layout | `hefesto/render/synthetic_layout.hpp` | 10,000,000 rectangles over 8 layers on a 5 mm × 5 mm die, from a fixed seed. Size mix modeled on sky130 hd cells: contacts and vias, gates, li1, diffusion, met1/met2 routes up to 50 µm and 100 µm, and a few met3 routes up to 500 µm. It uses splitmix64 and integer arithmetic only, so every platform generates the same rectangles. A unit test pins the fingerprint. |
| Headless canvas | `hefesto/render/headless.hpp` | Owns its Vulkan instance and device, renders into an offscreen RGBA8 image, and measures each frame with CPU timing and GPU timestamp queries. It is also the "texture mode" render path of D9. |
| Benchmark | `bench/canvas/` (`hefesto-bench-canvas`) | Headless. Generates the layout, uploads it, then measures a 600-frame pan at full-chip zoom and a 240-frame zoom sweep from full chip down to a 1 µm field of view. Runs once drawing every rectangle (`brute_force`) and once through the canvas index (`indexed`). Prints JSON. |
| Shell benchmark | `hefesto --bench-frames N` | The same renderer inside the Raven viewport of the real main window (dock, panels, command bar, status bar), in Raven's draw-callback mode, sharing Raven's VkDevice and queue. It pans for N frames with the whole UI drawn every frame and prints JSON: the frame-to-frame interval, plus Raven's own `RFrameStats`. |

### Canvas index: culling and level of detail

The renderer has to draw 10 M rectangles. Drawing every one of them every frame takes about 42 ms per frame on the M3 Pro (results below), so the instances are ordered so that each frame needs only a few cheap decisions:

- **Culling.** The bounding box is cut into 64 × 64 tiles. Each shape goes to the tile that holds its lower-left corner. Within a (layer, group) pair the shapes are stored tile by tile, so the visible part of one tile row is one contiguous instance range. A shape smaller than S extends at most S beyond its tile, so the query widens the window by S to the left and below.
- **Level of detail.** Level L uses square cells of S_L = 256 · 4^L units (256 nm up to 262 µm, 6 levels). Each rectangle gets a size class: the smallest L with larger side < S_L. For each layer and level, the cells that contain the center of a class ≤ L rectangle are precomputed, merged into horizontal runs, and cut at tile edges. A frame with p units per pixel uses the smallest level with S_L ≥ p:
  - it draws that level's cells, each covering 1 to 4 pixels, in place of the class ≤ L rectangles;
  - it draws the larger rectangles one by one, culled by tile;
  - rectangles larger than 262 µm are always drawn.

  When zoomed in (p ≤ 64 units per pixel), every visible rectangle is drawn.
- The cells go through the same pipeline as rectangles: the GPU sees one instance buffer and a list of draw ranges.

**Cost.** For the synthetic 10 M layout the index adds 9.5 M LOD cells (312 MB of instances instead of 160 MB). It takes 0.7 to 0.8 s to build, single-threaded.

**Fidelity.** A cell marks "something of this layer is here". It does not show coverage, so sparse regions look denser than they are at low zoom. This is a spike-level choice and is not checked against Magic's display. Rendering fidelity belongs to X-5 and `hefesto-design-system`.

## Results: Apple M3 Pro (18-core GPU), macOS, MoltenVK 1.4.2

The raw JSON is in `docs/spikes/results/2026-11-07-apple-m3-pro/`. Release build, Apple Clang 21. MoltenVK comes from Homebrew; the Vulkan headers are Khronos Vulkan-Headers v1.4.321. Dataset fingerprint: 9488214721081088727.

### Headless, 1920 × 1080 (`bench-headless-10M-1920x1080.json`)

| Mode | Phase | Median | p95 | p99 | Max | GPU median | Instances per frame (median / max) | Draws per frame (max) |
|---|---|---|---|---|---|---|---|---|
| brute force | pan, full chip | 42.5 ms | 54.5 | 56.7 | 59.5 | 42.1 ms | 10.0 M / 10.0 M | 8 |
| brute force | zoom sweep | 29.9 ms | 48.2 | 55.9 | 58.6 | 29.5 ms | 10.0 M / 10.0 M | 8 |
| indexed | pan, full chip | **4.9 ms** | 7.8 | 8.7 | 9.1 | 4.6 ms | 1.13 M / 1.89 M | 833 |
| indexed | zoom sweep | **0.7 ms** | 9.5 | 13.5 | 14.3 | 0.45 ms | 0.11 M / 3.74 M | 518 |

- Frame time here is CPU time: plan the ranges, record, submit, and wait for the GPU fence. It has no presentation and no UI.
- Upload: 160 MB in 118 ms (brute force) and 312 MB in 77 ms (indexed), through a 32 MB staging buffer. On Apple's unified memory, mapping device-local memory directly would remove the copy; this was not tried.
- Memory: the renderer holds 312 MB of device memory. Peak process RSS was 1.41 GB, because the generator output, the brute-force scene and the index were all in memory at once.

### Headless, 2880 × 1800 (Retina framebuffer of a 1440 × 900 window; `bench-headless-10M-2880x1800.json`)

| Mode | Phase | Median | p95 | p99 | Max | Instances (median / max) |
|---|---|---|---|---|---|---|
| indexed | pan, full chip | 9.9 ms | 15.6 | 16.4 | 16.5 | 2.49 M / 4.07 M |
| indexed | zoom sweep | 0.8 ms | 12.3 | 15.8 | 16.8 | 0.11 M / 4.07 M |

Finer pixels move the frame to a finer LOD level, so more instances are drawn.

### Inside Raven, the real main window, 1440 × 900 (`shell-*.json`)

| Run | Frame interval, median | p95 | p99 | Max | Raven `render_ms`, median |
|---|---|---|---|---|---|
| empty canvas, vsync requested off | 8.32 ms | 9.36 | 9.68 | 9.93 | 8.11 |
| 10 M rectangles, vsync requested off | **13.30 ms** | 16.04 | 16.30 | 24.97 | 13.20 |
| 10 M rectangles, vsync on | **13.27 ms** | 16.05 | 16.27 | 31.84 | 13.18 |

- The frame interval is the time between two of Raven's frame callbacks, with the dock, panels, command bar and status bar drawn in the same frame. Raven's own layout plus paint takes a median of 0.007 ms.
- The empty-canvas interval of 8.32 ms equals the 120 Hz refresh of the built-in display. Presentation therefore appears to stay synchronized even with `RInitDesc::disable_vsync`. This was not verified. If it holds, these numbers include waiting for presentation, and Raven's own UI cost on its render thread cannot be separated from that wait in this run.
- With 10 M rectangles the frame takes about 13 ms, with the GPU work of the canvas dominating (compare the 2880 × 1800 headless pan). This meets the 16.7 ms median target on this machine. The margin is thin at p95 (16.0 ms).

## Reading against the target

| Condition (D9) | Status |
|---|---|
| Median ≤ 16.7 ms during pan, macOS, with Raven's UI in the same frame | **Met on Apple M3 Pro** (13.3 ms). **Not measured on the reference M1 Mac.** The M1's GPU has much less throughput (8 cores against 18), so the Retina full-chip pan is at risk there without more renderer work. |
| Same, Linux (X11 and Wayland), mid-range discrete GPU | **Not measured.** No Linux machine with a GPU was available. CI (D12) builds the desktop leg but cannot judge performance. |
| Zoom sweep down to a 1 µm field of view | Met headless on the M3 Pro (0.7 to 0.8 ms median). |
| Peak GPU memory | 312 MB of instances for 10 M rectangles plus the LOD cells, measured. |
| Brute-force rendering (no index) | **Fails** (42.5 ms). Culling and level of detail are required, not optional. |

## What would close the gaps

1. **Run both benchmarks on the reference devices** (an M1 Mac; a Linux x86_64 machine with a mid-range discrete GPU under X11 and under Wayland) and commit the JSON next to these results.
2. **Renderer improvements, if the M1 misses the target.** These are Hefesto-renderer work and independent of Raven:
   - draw coverage instead of solid cells at low zoom: one coverage texture per layer, which also fixes the density fidelity above;
   - upload LOD cells lazily per level;
   - map device-local host-visible memory on unified-memory GPUs;
   - build the index on worker threads.
3. **Measure presentation separately.** A run with GPU timestamps around the canvas draw inside Raven's frame would show how much of the 13 ms belongs to the canvas, to Raven and to the presentation wait.

## How to reproduce

```sh
# macOS without the LunarG SDK: Homebrew MoltenVK plus Khronos headers
brew install molten-vk                      # 1.4.2 was used
git clone --depth 1 --branch v1.4.321 https://github.com/KhronosGroup/Vulkan-Headers.git /tmp/vulkan-headers
cmake -S . -B build/bench -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DHEFESTO_BUILD_DESKTOP=ON -DHEFESTO_BUILD_BENCH=ON \
  -DVulkan_INCLUDE_DIR=/tmp/vulkan-headers/include \
  -DVulkan_LIBRARY=/opt/homebrew/lib/libMoltenVK.dylib
cmake --build build/bench

build/bench/bench/canvas/hefesto-bench-canvas --output headless.json            # no display needed
build/bench/src/shell/desktop/hefesto --rects 10000000 --bench-frames 600 \
  --bench-output shell.json                                                     # opens a window for ~10 s
```

With the LunarG Vulkan SDK (or `vulkan-loader` and `vulkan-headers` from a package manager), `find_package(Vulkan)` finds everything and the two `-DVulkan_*` options are not needed. Raven loads the Vulkan library at run time: it tries `libvulkan.1.dylib` first, then `libMoltenVK.dylib`.
