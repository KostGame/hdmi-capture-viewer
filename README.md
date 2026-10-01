# HDMI Capture Viewer

A small native Windows preview for UVC HDMI capture devices. This repository now contains an early vertical slice; it is not a validated low-latency release.

## Current behavior

- Starts as a resizable window with a 1920 × 1080 client area. The video is centered and scaled to preserve its aspect ratio.
- Lists Media Foundation video capture devices and the native media types each device advertises in the **Device** and **Native format** menus. **Window → Rescan devices** refreshes the list.
- Prefers advertised YUY2 1920 × 1080 near 60 fps. Otherwise it prefers the largest advertised YUY2 mode, then the largest other advertised mode. Choose a listed mode explicitly from the menu.
- Requests the selected mode's dimensions and frame rate from the Media Foundation Source Reader. For even-width YUY2 it requests YUY2 output with Media Foundation converters disabled, checks the negotiated subtype, copies packed YUY2 rows into a tagged latest-frame buffer, and converts to display RGB in a Direct3D 11 pixel shader. The shader uses integer texture loads so chroma is not linearly filtered. Other selected profiles request RGB32 output and retain the BGRA upload/render fallback; these may require Media Foundation decoding/conversion, and the native input subtype is reported as unconfirmed. A failed request reports the Media Foundation error and does not silently select another profile.
- Initializes COM independently on the capture worker. Holds at most one pending frame and coalesces window-update notifications instead of accumulating a frame-message backlog. A dispatched frame notification renders the latest available image directly; `WM_PAINT` redraws the last uploaded image for exposure. During interactive dragging/resizing, a 16 ms timer and coalesced notifications keep rendering the newest available frame, while swap-chain resizing is limited to about 30 times/sec and the exact final size is applied upon release. Background erasure is suppressed to avoid flashes; VSync is temporarily off during interactive move/resize to avoid blocking the Windows drag loop (brief tearing is possible). The back-buffer render-target view is cached and released before swap-chain resize. The title refreshes at most once per second and reports the active GPU YUY2 path, delivered capture FPS, frame render FPS, replaced frames, callback-to-submit age, capture copy/convert time, `Present` CPU call time, and capture interval p95. These are application-side measurements, not input-to-photon latency.
- **Window → Borderless window** removes the frame while preserving the current client size and location; starting at the default 1920 × 1080 client size, the borderless preview can occupy one 4K desktop quadrant. Press F11 to restore the normal window. The normal window supports standard resize and Windows Snap.
- **Window → Fit video aspect (F9)** fixes the window after Windows Snap/FancyZones leaves a client area with the wrong aspect ratio. It shrinks only one dimension to the source video's actual ratio, keeps the window inside the current monitor work area, and preserves the nearest screen corner when possible. For a 1920×1080 source this gives an exact 16:9 client area without manually dragging an edge.
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

Connect and power the HDMI capture device before starting the app. If exactly one compatible video device is detected, the app opens its preferred advertised profile. If multiple devices are detected, choose the HDMI capture device from the **Device** menu; the app does not automatically activate an arbitrary webcam. You can select another device or profile from the menus. If enumeration happens before a device is connected, choose **Window → Rescan devices** after connecting it. Close the window to exit.

## Known gaps and unverified behavior

- The Windows x64 application must pass its GitHub Actions build and unit tests before use. The owner's previous GPU-path retest showed 60 capture/render FPS, but the **interactive drag and live resize changes still require physical visual validation**. A successful build does not prove smooth live resizing or full input-to-photon latency.
- No physical capture device was available in this implementation environment to check enumeration, driver behavior, image orientation/color, sustained FPS, signal changes, unplug/replug, or suspend/resume. The YUY2 path verifies that Source Reader output is YUY2 with converters disabled; advertised modes and RGB32 fallback output do not prove which native subtype is delivered or the sustained rate.
- The diagnosed earlier scalar CPU conversion took about 142 ms per frame on the owner's GTX 1080 system. This change removes that color conversion from the capture worker and predicts a substantial improvement in capture throughput. It does not guarantee 60 fps: the physical device path, driver, shader color output, buffer layout, and sustained preview must be retested on that hardware before drawing a performance conclusion. The older implementation remains on `main` until the owner completes that retest. Both paths still copy frame bytes; compressed/other fallback profiles use Media Foundation RGB32 decode/conversion. Device-loss recovery and repeated start/stop behavior still need hardware validation.
- Device/profile choice and window geometry are not persisted. Fullscreen mode, a diagnostic overlay, and recovery after device/signal loss are not implemented.
- Capture FPS counts delivered Source Reader samples in short app-side windows. Capture interval p95 describes spacing between returned samples; frame age is measured from the read callback return to UI submission; capture copy/convert time measures the worker's frame copy (plus any conversion in fallback decode); `Present` timing is the CPU duration of that API call. None measures input-to-photon latency or scanout. No comparison against FFplay or PotPlayer, high-speed-camera measurement, 20-minute stability run, or repeated connect/disconnect test has been performed. Do not infer an input-to-photon latency or sustained 60 fps guarantee from the selected advertised mode.
- The remaining Issue #1 requirements are future work: validate native-mode selection on hardware; measure and tune capture-to-display latency; test lifecycle recovery and long-run stability; add settings persistence and fullscreen; and complete Windows build/runtime acceptance.

There is no project license yet. Do not assume the code is licensed for reuse or redistribution.
