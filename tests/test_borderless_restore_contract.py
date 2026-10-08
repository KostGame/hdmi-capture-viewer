"""HCV-004 R3: native Windows caption replaces the custom child chrome."""
from pathlib import Path
import sys

s = Path(sys.argv[1] if len(sys.argv) > 1 else "src/main.cpp").read_text(encoding="utf-8")
def body(a, b):
    i = s.index(a)
    return s[i:s.index(b, i)]

assert "WS_OVERLAPPEDWINDOW" in s
assert "SWP_FRAMECHANGED" in s
assert "SetMenu(" not in s
assert "void set_chrome_mode(hcv::ChromeMode mode)" in s
native = body("    void apply_native_window_style()", "    void set_chrome_mode(")
for token in ("native_caption_active()", "SetWindowLongPtrW(window, GWL_STYLE, desired)",
              "GetWindowLongPtrW(window, GWL_STYLE)", "SWP_FRAMECHANGED",
              "WS_CAPTION", "SWP_NOACTIVATE"):
    assert token in native, token
assert "if (desired == current) return;" in native

nccalc = body("    case WM_NCCALCSIZE:", "    case WM_CREATE:")
assert "native_caption_active()" in nccalc
assert "DefWindowProcW" in nccalc

hit = body("    case WM_NCHITTEST:", "    case WM_MOUSELEAVE:")
assert "native_caption_active()" in hit
assert "DefWindowProcW" in hit
assert "HTBOTTOMRIGHT" in hit and "return HTCLIENT" in hit
assert "HTCAPTION" not in hit

for forbidden in ("chromeOverlay", "HcvChromeOverlay", "OVERLAY_HEIGHT_96", "overlay_height()", "overlay_proc",
                  "overlay_button_at", "WM_OVERLAY_", "OVERLAY_ACTION_",
                  "WM_NCLBUTTONDOWN, HTCAPTION, 0", "DrawTextW(",
                  "CreateSolidBrush(", "RegisterClassW(&overlayClass)"):
    assert forbidden not in s, f"custom chrome survived: {forbidden}"

restore = body("    void restore_fullscreen()", "    void toggle_chrome(")
assert "chromeMode = savedChromeMode" in restore
assert "nativeCaptionRevealed = false" in restore
assert "apply_native_window_style()" in restore
assert "savedWindowRect" in restore
toggle = body("    void toggle_chrome(hcv::ChromeMode requested)", "\n};")
assert "savedChromeMode = chromeMode" in toggle
assert "nativeCaptionRevealed = false" in toggle
assert "SetWindowPos(window, HWND_TOP, mi.rcMonitor.left" in toggle
assert "wParam == VK_F11" in s
assert "id == 3009" in s

# Snap safety: preserve a revealed native titlebar while window placement
# resembles a Windows Snap layout, while the system menu is open, or while
# Windows is tracking an interactive move/resize.
snap = body("    bool snap_like_window() const {", "    void poll_native_caption(")
assert "IsZoomed(window)" in snap
assert "MONITOR_DEFAULTTONEAREST" in snap
assert "hcv::snap_like_placement(" in snap
assert "MulDiv(24, dpi, 96)" in snap
poll = body("    void poll_native_caption(ULONGLONG nowMs)", "    void show_shortcuts()")
assert "hcv::keep_revealed_caption(nativeCaptionRevealed" in poll
assert "snap_like_window()" in poll
assert "nativeCaptionLastHoverMs = nowMs;" in poll
assert poll.index("keep_revealed_caption") < poll.index("nativeCaptionRevealed = false;")
assert "case WM_ENTERMENULOOP:" in s and "case WM_EXITMENULOOP:" in s
assert "systemMenuActive = true" in s and "systemMenuActive = false" in s
print("NATIVE_CHROME_RESTORE_CONTRACT_PASS")
