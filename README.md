# HDMI Capture Viewer

**A lightweight, low-latency live preview for USB HDMI capture devices on Windows.**

> **Project status:** Planning. The application and installers are not available yet. Development is tracked in [Issue #1](https://github.com/KostGame/hdmi-capture-viewer/issues/1).

## What this project aims to do

HDMI Capture Viewer is intended to do one thing: display a live HDMI capture feed in a desktop window, prioritizing **the freshest available frame** over buffered playback.

Planned MVP features:
- A resizable video window with a default 1920 × 1080 viewing area, suitable for a quarter of a 4K desktop.
- Capture-device detection and selection of device-supported resolutions, frame rates, and pixel formats.
- A low-buffering video path; prefer native YUY2 at 1080p60 **when the connected hardware can actually sustain it**.
- Aspect-ratio-preserving scaling, normal/borderless/fullscreen modes, and remembered settings.
- Optional diagnostics for capture and rendering performance.

## What it is not

The initial release will not be a recorder, streaming studio, media player, or remote-desktop/KVM tool. Keyboard and mouse input remain connected to the HDMI source device; this viewer only displays its video.

## Performance expectations

Reducing **application-side** buffering is the design goal, not a promise of zero delay. Actual end-to-end latency depends on the capture device, USB connection, source, GPU, display, and Windows presentation path. Device-advertised formats and frame rates will be verified experimentally.

## Implementation plan

The initial architecture under evaluation is native C++20, Windows Media Foundation, and Direct3D 11. Implementation choices will be confirmed using real latency measurements rather than theoretical API comparisons.

See [HCV-001: Minimal low-latency HDMI/UVC preview](https://github.com/KostGame/hdmi-capture-viewer/issues/1) for the project scope, constraints, tests, and acceptance criteria.

## License and contributions

A license has not been selected yet. Until one is added, the public repository should **not** be assumed to permit reuse or redistribution of its contents.
