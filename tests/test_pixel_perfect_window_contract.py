"""Guard the DPI-aware pixel-perfect F9 contract."""
from pathlib import Path
import sys

source = Path(sys.argv[1] if len(sys.argv) > 1 else 'src/main.cpp').read_text(encoding='utf-8')
main = source.index('int WINAPI wWinMain')
create = source.index('CreateWindowW', main)
dpi = source.index('SetProcessDpiAwarenessContext', main)
assert dpi < create, 'DPI awareness must be enabled before window creation'
assert 'DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2' in source
assert 'AdjustWindowRectExForDpi' in source
assert 'void set_video_pixels_100_percent()' in source
assert 'GetDpiForWindow(window)' in source
assert 'if (wParam == VK_F9) { app->set_video_pixels_100_percent(); return 0; }' in source
assert 'void set_video_pixels_100_percent()' in source and 'update_view_menu_checks();\n        request_view_render();' in source
assert 'id == 3005' in source and 'set_video_pixels_100_percent()' in source
assert 'id == 3006' in source and 'fit_window_to_video_aspect()' in source
assert 'One bounded correction' in source
print('PIXEL_PERFECT_WINDOW_CONTRACT_PASS')
