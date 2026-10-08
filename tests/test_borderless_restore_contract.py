"""Guard Windows-owned native chrome, native move/resize, and fullscreen restore."""
from pathlib import Path
import sys

source = Path(sys.argv[1] if len(sys.argv) > 1 else "src/main.cpp").read_text(encoding="utf-8")
header = Path("src/chrome_mode.hpp").read_text(encoding="utf-8")

def body(begin, end):
    a = source.index(begin)
    return source[a:source.index(end, a)]

assert "WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN" in source
assert "SetMenu(" not in source, "Popup menu must never affect client geometry"

native = body("    void apply_native_window_style()", "    void set_chrome_mode(")
for token in ("GetWindowLongPtrW(window, GWL_STYLE)",
              "(current | WS_CAPTION)", "WS_CAPTION", "SetWindowLongPtrW",
              "SWP_FRAMECHANGED", "SWP_NOMOVE", "SWP_NOSIZE", "layout_overlay()"):
    assert token in native, f"missing Windows native style contract: {token}"
mode = body("    void set_chrome_mode(hcv::ChromeMode mode)", "    void restore_fullscreen()")
assert "apply_native_window_style()" in mode
assert "overlayPolicy.set_mode" in mode

nccalc = body("    case WM_NCCALCSIZE:", "    case WM_CREATE:")
assert "chromeMode != hcv::ChromeMode::Normal && wParam" in nccalc
assert "DefWindowProcW(hwnd, message, wParam, lParam)" in nccalc
assert "return 0;" in nccalc

main = source[source.index("LRESULT CALLBACK window_proc"):]
hit_begin = main.index("    case WM_NCHITTEST:")
hit_end = main.index("    case WM_MOUSELEAVE:", hit_begin)
hit = main[hit_begin:hit_end]
assert "app->chromeMode == hcv::ChromeMode::Normal" in hit
assert "DefWindowProcW(hwnd, message, wParam, lParam)" in hit
assert "HTCAPTION" in hit and "HTBOTTOMRIGHT" in hit
assert "return HTCLIENT" in hit
assert "overlay_button_at(app->chromeOverlay, titlePoint.x)" in hit

overlay = body("LRESULT CALLBACK overlay_proc", "LRESULT CALLBACK window_proc")
assert "HTTRANSPARENT" in overlay
assert "HTCLIENT" in overlay
assert "SendMessageW(GetParent(hwnd), WM_NCLBUTTONDOWN" not in overlay
assert "WM_NCLBUTTONDOWN, HTCAPTION, 0" not in source
assert "chromeMode != hcv::ChromeMode::Normal && overlayPolicy.visible()" in source

toggle = body("    void toggle_chrome(hcv::ChromeMode requested)", "\n};")
assert "savedChromeMode = chromeMode" in toggle
assert "GetWindowRect(window, &savedWindowRect)" in toggle
assert "apply_native_window_style()" in toggle
assert "SetWindowPos(window, HWND_TOP, mi.rcMonitor.left" in toggle
restore = body("    void restore_fullscreen()", "    void toggle_chrome(")
assert "savedWindowRect.left" in restore and "savedWindowRect.top" in restore
assert "apply_native_window_style()" in restore
assert "viewMode=savedViewMode; panX=savedPanX; panY=savedPanY;" in restore

for token in ("OVERLAY_ACTION_MENU", "OVERLAY_ACTION_MINIMIZE", "OVERLAY_ACTION_MAXIMIZE",
              "OVERLAY_ACTION_CLOSE", "DrawTextW", "TrackPopupMenuEx", "TPM_RETURNCMD",
              "WM_MOUSELEAVE", "TrackMouseEvent", "WM_OVERLAY_HOVER"):
    assert token in overlay or token in source
assert "class OverlayVisibilityPolicy" in header
print("NATIVE_CHROME_RESTORE_CONTRACT_PASS")
