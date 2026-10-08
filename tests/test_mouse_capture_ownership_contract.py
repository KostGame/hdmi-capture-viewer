"""HCV-004 R5: do not disrupt Windows-owned caption mouse capture."""
from pathlib import Path
import sys

source = Path(sys.argv[1] if len(sys.argv) > 1 else "src/main.cpp").read_text(encoding="utf-8")
def between(a, b):
    i = source.index(a)
    return source[i:source.index(b, i)]

cancel = between("    case WM_CANCELMODE:", "    case WM_CAPTURECHANGED:")
loss = between("    case WM_KILLFOCUS:", "    case WM_NEW_FRAME:")
assert "const bool appOwnsCapture = app->draggingPan || app->draggingMove;" in cancel
assert "if(appOwnsCapture && GetCapture()==hwnd)ReleaseCapture();" in cancel
assert "return 0;" in cancel
assert "DefWindowProcW" not in cancel
assert "const bool appOwnsCapture = app->draggingPan || app->draggingMove;" in loss
assert "if(appOwnsCapture && heldByWindow)" in loss
assert "if(heldByWindow && !appOwnsCapture)" in loss
assert "ReleaseCapture();" in loss
assert "focusLossPreservedSystemCapture" in loss
assert "focusLossWhileNativeMove" in loss
assert "focusLossReleasedOwnCapture" in loss
assert "if(GetCapture()==hwnd)ReleaseCapture();" not in cancel + loss

down = between("    case WM_LBUTTONDOWN:", "    case WM_NCMOUSEMOVE:")
right = between("    case WM_RBUTTONUP:", "    case WM_MOUSEWHEEL:")
assert "++app->clientLeftDownEvents;" in down
assert "++app->clientRightUpEvents;" in right
assert "case WM_MOUSEACTIVATE:" in source
assert "++app->mouseActivateEvents;" in source
assert "++menuOpenRequests;" in source
for metric in ("focus loss native move ", "system capture preserved ",
               "own capture released ", "mouse activation ", "left down ",
               "right up ", "menu requested "):
    assert metric in source, metric
assert "case WM_ENTERSIZEMOVE:" in source
assert "case WM_EXITSIZEMOVE:" in source
assert "DXGI_PRESENT_DO_NOT_WAIT" in source
print("MOUSE_CAPTURE_OWNERSHIP_CONTRACT_PASS")
