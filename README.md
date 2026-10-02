# HDMI Capture Viewer

A small native Windows preview for UVC HDMI capture devices. This repository now contains an early vertical slice; it is not a validated low-latency release.

## Current behavior

- Starts as a resizable window with a 1920 × 1080 client area. **Auto** is the initial PotPlayer-like view mode: keep 1:1 source pixels when the whole frame fits, otherwise scale the whole frame down so nothing is cropped.
- Lists Media Foundation video capture devices and the native media types each device advertises in the **Device** and **Native format** menus. **Window → Rescan devices** refreshes the list.
- Prefers advertised YUY2 1920 × 1080 near 60 fps. Otherwise it prefers the largest advertised YUY2 mode, then the largest other advertised mode. Choose a listed mode explicitly from the menu.
- Requests the selected mode's dimensions and frame rate from the Media Foundation Source Reader. For even-width YUY2 it requests YUY2 output with Media Foundation converters disabled, checks the negotiated subtype, copies packed YUY2 rows into a tagged latest-frame buffer, and converts each new frame once into a source-resolution RGB texture with a Direct3D 11 pixel shader. Presentation then scales that RGB image with the hardware sampler, so larger viewports do not repeat YUY2 decoding. Other selected profiles request RGB32 output and retain the BGRA upload/render fallback; these may require Media Foundation decoding/conversion, and the native input subtype is reported as unconfirmed. A failed request reports the Media Foundation error and does not silently select another profile.
- Initializes COM independently on the capture worker. Holds at most one pending frame and coalesces window-update notifications instead of accumulating a frame-message backlog. A dispatched frame notification renders the latest available image directly; `WM_PAINT` redraws the last uploaded image for exposure. During native edge/corner resize, a 16 ms timer keeps rendering into the existing swap-chain buffer and Windows/DWM scales it; exactly one resize to the latest client dimensions is applied after release. Interactive and UI-driven presents are nonblocking and skipped when DXGI is busy; normal playback keeps the selected VSync setting. Background erasure is suppressed to avoid flashes. The back-buffer render-target view is cached and released before swap-chain resize. The title refreshes at most once per second and reports the active GPU YUY2 path and conversion-pass CPU submit time, delivered capture FPS, frame render FPS, replaced frames, callback-to-submit age, capture copy time, `Present` CPU call time, skipped interactive presents, and capture interval p95. These are application-side measurements, not input-to-photon latency.
- **Fullscreen (F11)** removes the frame and menu, fills the current monitor, and switches to Fill. F11 or Escape restores the prior style, menu, view/pan state, and exact saved outer window rectangle; maximized/minimized show state is restored where applicable.
- **Borderless window (Ctrl+F11)** is visual-only: it hides the title bar, frame, and menu but deliberately keeps the native overlapped-window style and the exact outer rectangle, so Windows Snap/FancyZones can keep the window attached to its current zone. Ctrl+F11 or Escape restores normal chrome without moving/resizing the outer rectangle. Resize edges and corners remain active; Alt+left-drag moves a frameless window when you intentionally want to detach/reposition it. Fullscreen (F11) remains a separate true popup/full-monitor mode.
- **Window → 100% / Pixel (F9)** sets a source-pixel 1:1 canvas and resets pan. It sizes the regular window to exactly 1920 × 1080 physical client pixels for the current source; repeating F9 does not accumulate frame growth. A smaller viewport crops the canvas without scaling. Scroll vertically with the wheel, use Shift+wheel horizontally, or hold Space and drag. Thin overlay thumbs indicate source overflow.
- **Window → Auto / whole frame (Shift+F10)** is the default: 1:1 when the whole source fits, otherwise automatically fit the entire frame inside the current window. Snap/FancyZones and ordinary resizing therefore need no manual edge correction. **Fit (F10)** always scales the whole source to fit; **Fill (Ctrl+F10)** covers the client area and crops source edges. All modes preserve aspect ratio.
- **Window → VSync** defaults to on. Turn it off to compare immediate presentation against synchronized presentation if the pointer still stutters; off may cause visible tearing. This uses the existing legacy DXGI presentation model, without assuming support for advanced tearing flags.
- Press **F2** (or **Window → Show metrics**) after moving the mouse for at least 10 seconds to show the current full metrics snapshot, including capture and render FPS. The snapshot dialog pauses rendering while it is open; close it before continuing the smoothness test.

No audio, recording, streaming, input forwarding, telemetry, or privileged system changes are included.

## Build on Windows

Use Visual Studio 2022 Build Tools or Visual Studio with the **Desktop development with C++** workload and a Windows 10/11 SDK. From an x64 Native Tools Command Prompt at the repository root:

```bat
cl /nologo /std:c++20 /EHsc /W4 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /Isrc src\main.cpp /Fe:hdmi-capture-viewer.exe mf.lib mfplat.lib mfreadwrite.lib mfuuid.lib d3d11.lib dxgi.lib d3dcompiler.lib user32.lib gdi32.lib shcore.lib
```

The GitHub Actions workflow uses `windows-2022` and the x64 MSVC toolchain to build this executable. No release or installer is produced.

The platform-independent one-slot frame queue check can be built on Linux with g++ 13 or later:

```sh
g++ -std=c++20 -Wall -Wextra -Werror -pthread tests/latest_frame_tests.cpp -o /tmp/hcv-latest-frame-tests
/tmp/hcv-latest-frame-tests
```

## Use

| Action | Shortcut | Behavior |
| --- | --- | --- |
| Auto | Shift+F10 | Default PotPlayer-like mode: whole frame always visible; 1:1 when it fits, otherwise scale down |
| Pixel100 | F9 | 1:1 source pixels; wheel/Shift+wheel pans, Space+drag pans (Space-pan takes priority over Alt-move) |
| Fit | F10 | Entire source visible with uniform scaling and bars if needed |
| Fill | Ctrl+F10 | Fill viewport, crop source edges |
| Fullscreen | F11 | Current monitor, Fill; F11/Esc restores prior window/view |
| Borderless window | Ctrl+F11 | Hide chrome at current rect; Alt+left-drag moves, resize edges remain active; Ctrl+F11/Esc restores chrome |
| Metrics | F2 | Show the current app-side metrics snapshot |

Pixel100 keeps text sharp by avoiding scaling; a viewport smaller than the source shows only the panned portion. Scrollbars are visual overlays and do not reserve client pixels.

Connect and power the HDMI capture device before starting the app. If exactly one compatible video device is detected, the app opens its preferred advertised profile. If multiple devices are detected, choose the HDMI capture device from the **Device** menu; the app does not automatically activate an arbitrary webcam. You can select another device or profile from the menus. If enumeration happens before a device is connected, choose **Window → Rescan devices** after connecting it. Close the window to exit.

## Known gaps and unverified behavior

- The Windows x64 application must pass its GitHub Actions build and unit tests before use. The **two-pass GPU YUY2 path, exact fullscreen restore, borderless-window geometry, interactive drag, live resize, hidden hit zones, and panning require Windows build and physical visual validation**. A successful build does not prove smooth live resizing or full input-to-photon latency.
- No physical capture device was available in this implementation environment to check enumeration, driver behavior, image orientation/color, sustained FPS, signal changes, unplug/replug, or suspend/resume. The YUY2 path verifies that Source Reader output is YUY2 with converters disabled; advertised modes and RGB32 fallback output do not prove which native subtype is delivered or the sustained rate.
- The diagnosed earlier scalar CPU conversion took about 142 ms per frame on the owner's GTX 1080 system. This change removes that color conversion from the capture worker and predicts a substantial improvement in capture throughput. It does not guarantee 60 fps: the physical device path, driver, shader color output, buffer layout, and sustained preview must be retested on that hardware before drawing a performance conclusion. The older implementation remains on `main` until the owner completes that retest. Both paths still copy frame bytes; compressed/other fallback profiles use Media Foundation RGB32 decode/conversion. Device-loss recovery and repeated start/stop behavior still need hardware validation.
- Device/profile choice and window geometry are not persisted. A diagnostic overlay and recovery after device/signal loss are not implemented.
- Capture FPS counts delivered Source Reader samples in short app-side windows. Capture interval p95 describes spacing between returned samples; frame age is measured from the read callback return to UI submission; capture copy/convert time measures the worker's frame copy (plus any conversion in fallback decode); `Present` timing is the CPU duration of that API call. None measures input-to-photon latency or scanout. No comparison against FFplay or PotPlayer, high-speed-camera measurement, 20-minute stability run, or repeated connect/disconnect test has been performed. Do not infer an input-to-photon latency or sustained 60 fps guarantee from the selected advertised mode.
- The remaining Issue #1 requirements are future work: validate native-mode selection on hardware; measure and tune capture-to-display latency; test lifecycle recovery and long-run stability; add settings persistence; and complete Windows build/runtime acceptance.

There is no project license yet. Do not assume the code is licensed for reuse or redistribution.
