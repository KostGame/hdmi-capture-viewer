"""Guard owner-target startup selection, overlay layout, help and safe Auto geometry."""
from pathlib import Path
import sys

source = Path(sys.argv[1] if len(sys.argv) > 1 else "src/main.cpp").read_text(encoding="utf-8")

for token in (
    'preferred_device_index',
    'vid_345f&pid_2131',
    'usb3.0 video',
    'preferredDevice < state.devices.size() || state.devices.size() == 1',
    'if (app->chromeOverlay) app->layout_overlay();',
    'void layout_overlay()',
    'hcv::Insets visible_insets() const',
    'viewMode == hcv::ViewMode::Auto || viewMode == hcv::ViewMode::Fit',
    "wParam == VK_F1",
    "wParam == 'B'",
    'Keyboard help (F1)',
    'Auto-hide chrome (Ctrl+B)',
    'WM_NCLBUTTONDOWN, HTCAPTION',
):
    assert token in source, f"missing startup/UI contract: {token}"

# Native caption semantics may only be initiated from the internal child overlay,
# never from the video client's WM_NCHITTEST.
hit_start = source.index("    case WM_NCHITTEST:")
hit_end = source.index("    case WM_MOUSEWHEEL:", hit_start)
assert "HTCAPTION" not in source[hit_start:hit_end]

# The child must be laid out immediately at WM_CREATE, not wait for a later resize.
create_start = source.index("    case WM_CREATE:")
create_end = source.index("    case WM_ERASEBKGND:", create_start)
create = source[create_start:create_end]
assert "app->layout_overlay();" in create

# Auto/Fit compensate for invisible snap-frame pixels that can sit outside rcWork.
render_start = source.index("void render(bool consumeFrame")
render_end = source.index("const auto presentStart", render_start)
render = source[render_start:render_end]
assert "visible_insets()" in render
assert "layoutW" in render and "layoutH" in render
assert "d.x +=" in render and "d.y +=" in render

print("STARTUP_UI_CONTRACT_PASS")
