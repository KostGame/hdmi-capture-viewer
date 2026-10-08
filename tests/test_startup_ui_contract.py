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
    'hcv::reveal_strip_px',
    'hcv::cursor_in_reveal_strip',
    'void reveal_chrome_from_cursor',
    'menuActive',
    'viewMode == hcv::ViewMode::Auto || viewMode == hcv::ViewMode::Fit',
    "wParam == VK_F1",
    "wParam == 'B'",
    'Keyboard help (F1)',
    'Auto-hide chrome (Ctrl+B)',
    'SWP_FRAMECHANGED',
    'WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN',
    'case WM_NCMOUSEMOVE:',
):
    assert token in source, f"missing startup/UI contract: {token}"

# Normal mode uses the true Windows caption; borderless mode returns HTCAPTION
# from the top-level window. The child never synthesizes mouse messages.
main = source[source.index("LRESULT CALLBACK window_proc"):]
hit_start = main.index("    case WM_NCHITTEST:")
hit_end = main.index("    case WM_MOUSELEAVE:", hit_start)
assert "HTCAPTION" in main[hit_start:hit_end]
assert "DefWindowProcW(hwnd, message, wParam, lParam)" in main[hit_start:hit_end]
assert "WM_NCLBUTTONDOWN, HTCAPTION, 0" not in source

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
assert "hcv::reveal_strip_px" in source
assert "visibleTop + revealPx" in source
assert "in.top + hcv::reveal_strip_px" in source
assert "overlayY + overlay_height()" not in source
assert "d.x +=" in render and "d.y +=" in render

timer_start = source.index("if (wParam == CHROME_HIDE_TIMER)")
timer_end = source.index("        break;", timer_start)
timer = source[timer_start:timer_end]
assert "reveal_chrome_from_cursor(now)" in timer
assert "menuActive" in timer
assert "overlayPolicy.timer(now)" in timer

menu_start = source.index("void show_app_menu")
menu_end = source.index("void set_chrome_mode", menu_start)
menu = source[menu_start:menu_end]
assert "menuActive = true" in menu and "menuActive = false" in menu
assert menu.count("overlayPolicy.reveal") >= 2

print("STARTUP_UI_CONTRACT_PASS")
