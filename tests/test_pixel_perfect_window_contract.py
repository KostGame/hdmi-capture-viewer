"""Guard the DPI-aware pixel-perfect F9 contract."""
from pathlib import Path
import sys

source = Path(sys.argv[1] if len(sys.argv) > 1 else 'src/main.cpp').read_text(encoding='utf-8')
main = source.index('int WINAPI wWinMain')
create = source.index('CreateWindowW', main)
dpi = source.index('SetProcessDpiAwarenessContext', main)
assert dpi < create, 'DPI awareness must be enabled before window creation'
assert 'DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2' in source
assert 'AdjustWindowRectExForDpi' not in source, 'Custom full-client sizing must not use system nonclient estimates'
assert 'void set_video_pixels_100_percent()' in source
assert 'GetDpiForWindow(window)' in source
assert 'if (wParam == VK_F9) { app->set_video_pixels_100_percent(); return 0; }' in source
assert 'void set_video_pixels_100_percent()' in source and 'update_view_menu_checks();\n        request_view_render();' in source
assert 'id == 3005' in source and 'set_video_pixels_100_percent()' in source
assert 'id == 3006' in source and 'viewMode=hcv::ViewMode::Fit' in source
f9 = source[source.index('void set_video_pixels_100_percent()'):source.index('void fit_window_to_video_aspect()')]
assert 'place_window_size_preserving_corner(videoW, videoH, outer)' in f9
assert 'One bounded correction' in f9 and 'GetClientRect(window, &client)' in f9
print('PIXEL_PERFECT_WINDOW_CONTRACT_PASS')
