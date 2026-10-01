# HDMI Capture Viewer

A small native Windows preview for UVC HDMI capture devices. This repository now contains an early vertical slice; it is not a validated low-latency release.

## Current behavior

- Starts as a resizable window with a 1920 × 1080 client area. The video is centered and scaled to preserve its aspect ratio.
- Lists Media Foundation video capture devices and the native media types each device advertises in the **Device** and **Native format** menus. **Window → Rescan devices** refreshes the list.
- Prefers advertised YUY2 1920 × 1080 near 60 fps. Otherwise it prefers the largest advertised YUY2 mode, then the largest other advertised mode. Choose a listed mode explicitly from the menu.
- Requests the selected mode's dimensions and frame rate from the Media Foundation Source Reader. For YUY2 it requests YUY2 output with Media Foundation converters disabled, checks that the negotiated output subtype is YUY2, and converts packed YUY2 pixels to BGRA on the capture worker. For other selected profiles it requests RGB32 output, which may require Media Foundation decoding/conversion; the native input subtype is then explicitly reported as unconfirmed. A failed request reports the Media Foundation error; it does not silently select a different listed profile.
- Initializes COM independently on the capture worker. Holds at most one pending frame and coalesces window-update notifications instead of accumulating a frame-message backlog. D3D11 presents the latest available image and the title shows render FPS, replaced-frame count, and callback-to-submit time. That time is not input-to-photon latency.
- **Window → Borderless window** removes the frame while preserving the current client size and location; starting at the default 1920 × 1080 client size, the borderless preview can occupy one 4K desktop quadrant. Press F11 to restore the normal window. The normal window supports standard resize and Windows Snap.

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

- This Linux worktree has no Windows SDK or MSVC compiler, so the Win32 application has not been compiled or run here. The Windows Actions build is configured but has not run in this offline implementation session.
- No physical capture device was available for checking enumeration, driver behavior, image orientation/color, sustained FPS, signal changes, unplug/replug, or suspend/resume. The YUY2 path verifies that Source Reader output is YUY2 with converters disabled; advertised modes and RGB32 fallback output do not prove which native subtype is delivered or the sustained rate.
- The YUY2 path converts to BGRA on the CPU before D3D11 upload, while compressed/other fallback profiles use Media Foundation RGB32 decode/conversion. Both paths copy frames. Device-loss recovery and robust repeated start/stop handling need hardware validation.
- Device/profile choice and window geometry are not persisted. Fullscreen mode, a diagnostic overlay, and recovery after device/signal loss are not implemented.
- Callback-to-submit timing is an application-side estimate, not end-to-end latency evidence. No comparison against FFplay or PotPlayer, high-speed-camera measurement, 20-minute stability run, or repeated connect/disconnect test has been performed. Do not infer an input-to-photon latency or sustained 60 fps guarantee from the selected advertised mode.
- The remaining Issue #1 requirements are future work: validate native-mode selection on hardware; measure and tune capture-to-display latency; test lifecycle recovery and long-run stability; add settings persistence and fullscreen; and complete Windows build/runtime acceptance.

There is no project license yet. Do not assume the code is licensed for reuse or redistribution.
