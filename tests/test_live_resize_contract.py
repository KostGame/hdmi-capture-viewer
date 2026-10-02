"""Guard against per-WM_SIZE swap-chain churn and repaint flashes."""
from pathlib import Path
import sys

source = Path(sys.argv[1] if len(sys.argv) > 1 else 'src/main.cpp').read_text(encoding='utf-8')
start = source.index('case WM_SIZE:')
end = source.index('case WM_NEW_FRAME:', start)
size_handler = source[start:end]
assert 'resizeGate.request' in size_handler
assert 'ResizeBuffers' not in size_handler, 'Never resize DXGI on every WM_SIZE'
assert source.count('ResizeBuffers(') == 1, 'Unexpected extra DXGI resize path'
assert 'case WM_ERASEBKGND: return 1;' in source
assert 'case WM_ENTERSIZEMOVE:' in source
assert 'case WM_EXITSIZEMOVE:' in source
assert 'case WM_TIMER:' in source
assert 'case WM_MOVING:' in source and 'case WM_SIZING:' in source
assert 'context->Flush();' in source
assert 'resizeGate.completed(now);' in source
resize = source[source.index('bool apply_pending_resize()'):source.index('bool convert_yuy2_frame()')]
assert 'if (interactiveMoveResize) return true;' in resize
assert resize.index('if (interactiveMoveResize) return true;') < resize.index('ResizeBuffers(')
exit_handler = source[source.index('case WM_EXITSIZEMOVE:'):source.index('case WM_MOVING:')]
assert exit_handler.index('interactiveMoveResize = false;') < exit_handler.index('app->render(true, true, true);')
assert 'interactiveMoveResize || draggingMove || uiImmediate' in source
assert 'DXGI_PRESENT_DO_NOT_WAIT' in source
assert 'DXGI_ERROR_WAS_STILL_DRAWING' in source and '++skippedInteractivePresent' in source
print('LIVE_RESIZE_CONTRACT_PASS')
