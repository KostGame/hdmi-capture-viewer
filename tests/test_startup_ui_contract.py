"""Owner's capture device, full-view geometry, and native Windows chrome."""
from pathlib import Path
import sys
s = Path(sys.argv[1] if len(sys.argv) > 1 else "src/main.cpp").read_text(encoding="utf-8")
for token in (
    "preferred_device_index", "vid_345f&pid_2131", "usb3.0 video",
    "preferredDevice < state.devices.size() || state.devices.size() == 1",
    "hcv::Insets visible_insets() const", "hcv::reveal_strip_px",
    "void poll_native_caption(ULONGLONG nowMs)",
    "nativeCaptionRevealed", "nativeCaptionLastHoverMs",
    "menuActive", "GetAncestor(hovered, GA_ROOT) == window",
    "viewMode == hcv::ViewMode::Auto || viewMode == hcv::ViewMode::Fit",
    "wParam == VK_F1", "wParam == 'B'", "Keyboard help (F1)",
    "Auto-hide chrome (Ctrl+B)", "WS_OVERLAPPEDWINDOW",
    "case WM_NCMOUSEMOVE:", "SWP_FRAMECHANGED",
):
    assert token in s, f"missing startup/native UI contract: {token}"

hit = s[s.index("LRESULT CALLBACK window_proc"):]
hit = hit[hit.index("    case WM_NCHITTEST:"):hit.index("    case WM_MOUSELEAVE:")]
assert "native_caption_active()" in hit and "DefWindowProcW" in hit
assert "HTBOTTOMRIGHT" in hit
assert "HTCAPTION" not in hit
render = s[s.index("void render(bool consumeFrame"):s.index("const auto presentStart")]
assert "visible_insets()" in render
assert "layoutW" in render and "layoutH" in render
assert "d.x +=" in render and "d.y +=" in render
assert "overlayY + overlay_height()" not in render

timer = s[s.index("if (wParam == CHROME_HIDE_TIMER)"):s.index("        break;", s.index("if (wParam == CHROME_HIDE_TIMER)"))]
assert "poll_native_caption(GetTickCount64())" in timer
assert "overlayPolicy" not in timer
menu = s[s.index("void show_app_menu"):s.index("void apply_native_window_style")]
assert "menuActive = true" in menu and "menuActive = false" in menu
assert "TrackPopupMenuEx" in menu
print("STARTUP_NATIVE_UI_CONTRACT_PASS")
