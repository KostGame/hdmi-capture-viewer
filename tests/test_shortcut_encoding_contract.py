"""Guard UTF-8 Windows build and root-level shortcut dispatch."""
from pathlib import Path
import sys

source = Path(sys.argv[1] if len(sys.argv) > 1 else "src/main.cpp").read_text(encoding="utf-8")
workflow = Path(sys.argv[2] if len(sys.argv) > 2 else ".github/workflows/windows-build.yml").read_text(encoding="utf-8")

for token in (
    "bool handle_app_shortcut(App& state, const MSG& msg)",
    "msg.hwnd != state.window && !IsChild(state.window, msg.hwnd)",
    "msg.wParam == 'B' && ctrl",
    "msg.wParam == VK_F1",
    "msg.wParam == VK_F10",
    "msg.wParam == VK_F11",
    "if (handle_app_shortcut(state, msg)) continue;",
):
    assert token in source, f"missing root shortcut routing: {token}"

# Russian help and punctuation are UTF-8 in source. MSVC must be told so explicitly.
compile_lines = [line for line in workflow.splitlines() if "src\\main.cpp" in line and "cl /nologo" in line]
assert compile_lines, "main MSVC compile command missing"
assert all("/utf-8" in line for line in compile_lines), "MSVC main build must use /utf-8"
assert "эта памятка" in source
assert "переключение системной рамки Windows" in source

print("SHORTCUT_ENCODING_CONTRACT_PASS")
