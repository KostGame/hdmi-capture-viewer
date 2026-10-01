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
assert 'interactiveMoveResize ? 0u' in source
print('LIVE_RESIZE_CONTRACT_PASS')
