"""Guard deferred/coalesced view rendering on high-rate UI input paths."""
from pathlib import Path
import sys

source = Path(sys.argv[1] if len(sys.argv) > 1 else "src/main.cpp").read_text(encoding="utf-8")
window_proc = source[source.index("LRESULT CALLBACK window_proc"):]

def handler(case, following):
    start = window_proc.index(case)
    return window_proc[start:window_proc.index(following, start)]

for name, following in (("case WM_MOUSEMOVE:", "case WM_LBUTTONUP:"), ("case WM_MOUSEWHEEL:", "case WM_KEYUP:")):
    body = handler(name, following)
    assert "app->render(" not in body, f"synchronous render in {name} handler"
    assert "request_view_render()" in body

queue = source[source.index("    void request_view_render()"):source.index("    void render_view_request()")]
assert "viewRenderQueued.try_queue()" in queue
assert "PostMessageW(window, WM_VIEW_RENDER" in queue
assert "coalescedViewRenderRequests" in queue
assert "case WM_VIEW_RENDER:" in window_proc and "viewRenderQueued.handled()" in window_proc
assert "void render(bool consumeFrame, bool forceWithoutFrame = false, bool uiImmediate = false)" in source
assert "render(true, true, true);" in source
assert "DXGI_PRESENT_DO_NOT_WAIT" in source
assert "DXGI_ERROR_WAS_STILL_DRAWING" in source
assert "skipped interactive presents " in source
assert "app->chromeMode!=hcv::ChromeMode::Normal && (GetKeyState(VK_MENU)&0x8000)" in source
assert "app->spacePanning){app->draggingPan=true" in source
assert "app->draggingMove=true" in source
left_down = handler("case WM_LBUTTONDOWN:", "case WM_MOUSEMOVE:")
assert left_down.index("app->spacePanning") < left_down.index("GetKeyState(VK_MENU)")
assert "case WM_CAPTURECHANGED:" in source and "case WM_CANCELMODE:" in source
assert "merged view renders " in source
print("UI_RENDER_COALESCING_CONTRACT_PASS")
