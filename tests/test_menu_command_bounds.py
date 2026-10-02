"""Guard dynamic menu command IDs before indexing device/mode vectors."""
from pathlib import Path
import sys

source = Path(sys.argv[1] if len(sys.argv) > 1 else "src/main.cpp").read_text(encoding="utf-8")
start = source.index("    case WM_COMMAND: {")
end = source.index("        if (id == 3001)", start)
handler = source[start:end]

assert "const std::size_t index = id - DEVICE_COMMAND_BASE;" in handler
assert "if (index >= app->devices.size()) return 0;" in handler
assert handler.index("if (index >= app->devices.size()) return 0;") < handler.index("preferred_mode(app->devices[index])")

assert "if (app->devices.empty() || app->selectedDevice >= app->devices.size()) return 0;" in handler
assert "const std::size_t index = id - MODE_COMMAND_BASE;" in handler
assert "if (index >= app->devices[app->selectedDevice].modes.size()) return 0;" in handler
assert handler.index("if (index >= app->devices[app->selectedDevice].modes.size()) return 0;") < handler.index("app->selectedMode = index")

print("MENU_COMMAND_BOUNDS_PASS")
