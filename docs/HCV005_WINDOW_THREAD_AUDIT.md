# HCV-005 UI thread audit
AI_OWNER_SIGNATURE=chatgpt-hcv-architect-20261003
AI_TASK_TRACE=HCV-005-UI-ARCH-20261009

Accepted Git main: `7d9ab0d15706c3dcc0436c6ba7234ca4cca1394e`; fresh isolated worktree `agent/9-win32-isolation-baseline`. HCV-004 R1-R5 hardware NOT ACCEPTED. Original installed R3 and desktop shortcut protected.

## Source-proven facts
- `wWinMain` creates `HcvPreviewWindow` with `WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN`, then runs `GetMessageW/DispatchMessageW`.
- Existing `HcvChromeOverlay` child HWND imitates caption buttons with GDI and has its own input/message handler.
- Same UI thread processes `WM_NEW_FRAME -> render(true)`, `WM_VIEW_RENDER -> render_view_request -> render`, `WM_PAINT -> paint/interactive_tick`, `WM_EXITSIZEMOVE -> render`, `WM_MOVING/WM_SIZING` and live movement timer -> `interactive_tick()`.
- `render()` creates/manages D3D11 resources, resizes the swapchain, converts YUY2 and submits GPU draws. In accepted main, ordinary noninteractive `Present()` with VSync enabled may wait for synchronization; the no-wait flag is used only for interactive paths.
- `WM_RBUTTONUP` invokes `TrackPopupMenuEx`, which establishes a nested native modal menu loop. `WM_ENTERSIZEMOVE` also uses native modal move-size processing.
- `WM_KILLFOCUS`/`WM_CANCELMODE` can call `ReleaseCapture`; patched in experimental R5 but not shown to resolve stalls.

## Observed hardware evidence
R5 CSV `C:\CODE\captures\hcv-004-r5-run64\HCV004-R5-MOUSE-20261009-022920.csv`: 649 samples, about 180 sec. Owner: about 2 seconds to activate from another app, left/right-click delays and corner unsnap stalls. Mouse message *queue age* max 13,609 ms; 108 slow queued messages; WM_NULL max 17.47 ms, no timeouts; capture/render 61/60 fps; delivered WM_MOUSEACTIVATE 14, client left-down 37, right-up 8, popup requests 8; all three focus/capture ownership counters zero. Queue age is **not** physical click-to-window-response latency and may be inflated by modal message loops.

## Hypotheses, not proven causes
1. GPU/upload/resize/`Present()` on the UI thread blocks input.
2. Additional child chrome HWND or native non-client activation interferes with Snap.
3. Popup/menu modal loop and synchronous menu work interferes with responsiveness.
4. Current latency metric conflates nested-loop dispatch with true delayed input.

## Next controlled test gate
A. Minimal Win32 top-level shell, native WS_OVERLAPPEDWINDOW, no video, custom chrome or D3D; Snap, unsnap, return-from-occlusion, first left/right click.
B. Accepted viewer **without capture or renderer**, retaining its custom chrome and window event handlers. Compare with A.
C. Accepted viewer with real capture/render, same interactions. If A/B smooth and C stalls, investigate render/UI-thread separation. If A also stalls, investigate Windows/system shell and user input tools.
D. Instrument WndProc duration, menu loop, native move-size, activation, focus, and GPU calls without recording keystrokes or coordinates. Use short reproducible cycles rather than long random stress.

No merge, release, shortcut edit, installed app replacement, or claim of hardware fix without owner physical acceptance.
