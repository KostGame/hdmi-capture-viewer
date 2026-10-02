"""Guard deferred/coalesced view rendering on high-rate UI input paths."""
from pathlib import Path
import sys

source = Path(sys.argv[1] if len(sys.argv) > 1 else "src/main.cpp").read_text(encoding="utf-8")

def handler(case, following):
    return source[source.index(case):source.index(following, source.index(case))]

for name, following in (("case WM_MOUSEMOVE:", "case WM_LBUTTONUP:"), ("case WM_MOUSEWHEEL:", "case WM_KEYUP:")):
    body = handler(name, following)
    assert "render(false)" not in body, f"synchronous render in {name} handler"
    assert "request_view_render()" in body

queue = source[source.index("    void request_view_render()"):source.index("    void render_view_request()")]
assert "viewRenderQueued.try_queue()" in queue
assert "PostMessageW(window, WM_VIEW_RENDER" in queue
assert "coalescedViewRenderRequests" in queue
assert "case WM_VIEW_RENDER:" in source and "viewRenderQueued.handled()" in source
assert "void render(bool consumeFrame, bool forceWithoutFrame = false)" in source
assert "render(true, true);" in source
assert "merged view renders " in source
print("UI_RENDER_COALESCING_CONTRACT_PASS")
