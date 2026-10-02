"""Guard fullscreen restore and Snap-preserving borderless contracts."""
from pathlib import Path
import sys

source = Path(sys.argv[1] if len(sys.argv) > 1 else "src/main.cpp").read_text(encoding="utf-8")
start = source.index("    void restore_normal_chrome()")
end = source.index("\n};", start)
chrome = source[start:end]
toggle_start = chrome.index("    void toggle_chrome(hcv::ChromeMode requested)")
toggle = chrome[toggle_start:]

assert "enum class ChromeMode { Normal, BorderlessWindow, Fullscreen }" in Path("src/chrome_mode.hpp").read_text(encoding="utf-8")
assert "if (chromeMode != hcv::ChromeMode::Normal) restore_normal_chrome();" in toggle
assert "savedViewMode=viewMode; savedPanX=panX; savedPanY=panY;" in toggle
assert "viewMode=hcv::ViewMode::Fill;" in toggle

# Fullscreen remains a true popup/full-monitor transition.
assert "SetWindowLongPtrW(window, GWL_STYLE, WS_POPUP | WS_THICKFRAME | WS_VISIBLE);" in toggle
assert "SetWindowPos(window, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top" in toggle

# BorderlessWindow is visual-only. It must keep normal style + exact outer rect,
# otherwise Windows Snap/FancyZones can drop the zone association.
marker = toggle.index("// Visual borderless only")
borderless_entry = toggle[marker:toggle.index("DrawMenuBar(window);", marker)]
assert "SetWindowLongPtrW" not in borderless_entry
assert "SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED" in borderless_entry
assert "Windows Snap/FancyZones keep" in borderless_entry

# Leaving BorderlessWindow must also avoid moving/resizing the outer rectangle.
assert "const auto leavingMode = savedChromeMode;" in chrome
assert "if (leavingMode == hcv::ChromeMode::Fullscreen)" in chrome
restore_marker = chrome.index("// BorderlessWindow never changes")
borderless_restore = chrome[restore_marker:chrome.index("savedMenu=nullptr;", restore_marker)]
assert "SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED" in borderless_restore
assert "savedWindowRect.left" not in borderless_restore

assert "SetMenu(window, nullptr);" in toggle
assert "chromeMode = hcv::ChromeMode::Normal;" in chrome
assert "wParam == VK_F11" in source and "GetKeyState(VK_CONTROL)" in source
assert "GetKeyState(VK_MENU)" in source
assert "SetCapture(hwnd)" in source and "GetCursorPos(&cursor)" in source
assert "HTCAPTION" not in source
assert "app->toggle_chrome(hcv::ChromeMode::Fullscreen)" in source
assert "app->toggle_chrome(hcv::ChromeMode::BorderlessWindow)" in source
assert "VK_ESCAPE && app->chromeMode != hcv::ChromeMode::Normal" in source
assert "if(hcv::frameless(app->chromeMode)) return 0;" in source
assert "if(hcv::frameless(app->chromeMode)){" in source
print("CHROME_MODE_RESTORE_CONTRACT_PASS")
