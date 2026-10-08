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
assert "nativeCaptionRevealed = savedNativeCaptionRevealed" in restore
assert "nativeCaptionPinned = savedNativeCaptionPinned" in restore
assert "apply_native_window_style()" in restore
assert "savedWindowRect" in restore
toggle = body("    void toggle_chrome(hcv::ChromeMode requested)", "\n};")
assert "savedChromeMode = chromeMode" in toggle
assert "nativeCaptionRevealed = false" in toggle
assert "SetWindowPos(window, HWND_TOP, mi.rcMonitor.left" in toggle
assert "wParam == VK_F11" in s
assert "id == 3009" in s

# Deterministic lock after a real native move/size, not guessed Snap edges.
poll = body("    void poll_native_caption(ULONGLONG nowMs)", "    void show_shortcuts()")
assert "if (nativeCaptionPinned)" in poll
assert poll.index("if (nativeCaptionPinned)") < poll.index("nativeCaptionRevealed = false;")
assert "snap_like_window()" not in s
assert "snap_like_placement" not in s
assert "case WM_WINDOWPOSCHANGED:" in s
assert "hcv::caption_should_pin_after_window_move(app->nativeCaptionRevealed" in s
assert "SWP_NOMOVE" in s and "SWP_NOSIZE" in s
assert "case WM_ENTERSIZEMOVE:" in s and "app->nativeCaptionPinned = true" in s
assert "case WM_ENTERMENULOOP:" in s and "case WM_EXITMENULOOP:" in s
assert "systemMenuActive = true" in s and "systemMenuActive = false" in s
assert "savedNativeCaptionRevealed = nativeCaptionRevealed" in toggle
assert "savedNativeCaptionPinned = nativeCaptionPinned" in toggle
assert "nativeCaptionPinned = false" in toggle
assert "nativeCaptionPinned = false" in body("    void set_chrome_mode(", "    void restore_fullscreen(")
print("NATIVE_CHROME_RESTORE_CONTRACT_PASS")
