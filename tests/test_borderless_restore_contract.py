"""Guard exact physical-window-rectangle restoration when leaving borderless mode."""
from pathlib import Path
import sys

source = Path(sys.argv[1] if len(sys.argv) > 1 else "src/main.cpp").read_text(encoding="utf-8")
start = source.index("    void toggle_borderless()")
end = source.index("\n    }\n};", start) + len("\n    }")
toggle = source[start:end]
restore = toggle[toggle.index("        } else {"):]

assert "RECT savedWindowRect{};" in source
assert "GetWindowRect(window, &savedWindowRect);" in toggle
assert "SetWindowPos(window, nullptr, savedWindowRect.left, savedWindowRect.top, width, height" in restore
assert "SetWindowPlacement(window, &savedPlacement)" not in restore, "Placement metadata can lose Snap/FancyZones geometry"
assert restore.index("borderless = false;") < restore.index("SWP_FRAMECHANGED"), "WM_NCCALCSIZE must see normal mode while the frame is recalculated"
assert "savedShowCmd == SW_SHOWMAXIMIZED" in restore
assert "savedShowCmd == SW_SHOWMINIMIZED" in restore
assert "wParam == VK_F11 && !(lParam & (1LL << 30))" in source, "Ignore repeated keydown messages so F11 toggles once per press"
assert "wParam == VK_ESCAPE && app->borderless" in source
assert "if (!borderless) rebuild_menus();" in toggle, "Borderless must not recreate a visible menu"
print("BORDERLESS_EXACT_RESTORE_CONTRACT_PASS")
