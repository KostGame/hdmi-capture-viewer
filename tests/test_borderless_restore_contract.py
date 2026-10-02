"""Guard stable native chrome and PotPlayer-style child overlay contracts."""
from pathlib import Path
import sys

source = Path(sys.argv[1] if len(sys.argv) > 1 else "src/main.cpp").read_text(encoding="utf-8")
header = Path("src/chrome_mode.hpp").read_text(encoding="utf-8")

def body(start_marker, end_marker):
    start = source.index(start_marker)
    return source[start:source.index(end_marker, start)]

assert "WS_OVERLAPPEDWINDOW" in source
assert "SetWindowLongPtrW(window" not in source, "Parent native style must remain stable in every mode"
assert "SetMenu(" not in source, "The app menu is popup-only and never changes client geometry"
assert "case WM_NCCALCSIZE:\n        return 0;" in source
assert "void set_chrome_mode(hcv::ChromeMode mode)" in source
mode = body("    void set_chrome_mode(hcv::ChromeMode mode)", "    void restore_fullscreen()")
assert "chromeMode = mode" in mode and "overlayPolicy.set_mode" in mode
assert "update_overlay_visibility()" in mode
assert "SetWindowLongPtrW" not in mode and "SetWindowPos" not in mode and "request_view_render" not in mode

toggle = body("    void toggle_chrome(hcv::ChromeMode requested)", "\n};")
assert "savedChromeMode = chromeMode" in toggle
assert "GetWindowRect(window, &savedWindowRect)" in toggle
assert "SetWindowPos(window, HWND_TOP, mi.rcMonitor.left" in toggle
restore = body("    void restore_fullscreen()", "    void toggle_chrome(")
assert "savedWindowRect.left" in restore and "savedWindowRect.top" in restore
assert "savedWindowRect.right - savedWindowRect.left" in restore
assert "viewMode=savedViewMode; panX=savedPanX; panY=savedPanY;" in restore

assert "CreateWindowExW(0, L\"HcvChromeOverlay\"" in source
for token in ("OVERLAY_ACTION_MENU", "OVERLAY_ACTION_MINIMIZE", "OVERLAY_ACTION_MAXIMIZE",
              "OVERLAY_ACTION_CLOSE", "DrawTextW", "TrackPopupMenuEx", "TPM_RETURNCMD",
              "WM_MOUSELEAVE", "TrackMouseEvent", "WM_OVERLAY_HOVER"):
    assert token in source, f"missing internal overlay behavior: {token}"
assert "app->show_app_menu(p.x, p.y)" in source
assert "case WM_NCHITTEST:" in source and "HTBOTTOMRIGHT" in source
hit_test = body("    case WM_NCHITTEST:", "    case WM_MOUSEWHEEL:")
assert "chromeMode" not in hit_test, "Resize zones stay active in every logical mode"
assert "HTCAPTION" not in source
assert "GetKeyState(VK_MENU)" in source
assert "wParam == VK_F11" in source and "GetKeyState(VK_CONTROL)" in source
assert "app->toggle_chrome(hcv::ChromeMode::Fullscreen)" in source
assert "app->toggle_chrome(hcv::ChromeMode::BorderlessWindow)" in source
assert "VK_ESCAPE && app->chromeMode != hcv::ChromeMode::Normal" in source
assert "NormalPinned" in source and "BorderlessAutoHide" in source and "FullscreenAutoHide" in source
assert "overlayPolicy.visible()?L\"visible\":L\"hidden\"" in source
assert "class OverlayVisibilityPolicy" in header
print("CHROME_MODE_RESTORE_CONTRACT_PASS")
