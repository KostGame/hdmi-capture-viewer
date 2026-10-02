"""Guard fullscreen restore and borderless-window geometry contracts."""
from pathlib import Path
import sys

source = Path(sys.argv[1] if len(sys.argv) > 1 else "src/main.cpp").read_text(encoding="utf-8")
start = source.index("    void restore_normal_chrome()")
end = source.index("\n};", start)
chrome = source[start:end]

assert "enum class ChromeMode { Normal, BorderlessWindow, Fullscreen }" in Path("src/chrome_mode.hpp").read_text(encoding="utf-8")
assert "void toggle_chrome(hcv::ChromeMode requested)" in chrome
assert "if (chromeMode != hcv::ChromeMode::Normal) restore_normal_chrome();" in chrome
assert "savedViewMode=viewMode; savedPanX=panX; savedPanY=panY;" in chrome
assert "viewMode=hcv::ViewMode::Fill;" in chrome
assert "SetWindowPos(window, nullptr, exactRect.left, exactRect.top, width, height" in chrome
assert "SetWindowPos(window, nullptr, savedWindowRect.left, savedWindowRect.top, width, height" in chrome
assert "SetMenu(window, nullptr);" in chrome
assert "chromeMode = hcv::ChromeMode::Normal;" in chrome
assert "wParam == VK_F11" in source and "GetKeyState(VK_CONTROL)" in source
assert "app->toggle_chrome(hcv::ChromeMode::Fullscreen)" in source
assert "app->toggle_chrome(hcv::ChromeMode::BorderlessWindow)" in source
assert "VK_ESCAPE && app->chromeMode != hcv::ChromeMode::Normal" in source
assert "if(hcv::frameless(app->chromeMode)) return 0;" in source
assert "if(hcv::frameless(app->chromeMode)){" in source
print("CHROME_MODE_RESTORE_CONTRACT_PASS")
