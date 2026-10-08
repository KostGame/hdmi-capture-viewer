"""Regression guard for HCV-004 UI input responsiveness.

Static contract only. A live Win32-session test must still measure focus, drag,
resize and occlusion after idle before this fix is considered accepted.
"""
from pathlib import Path
import sys

source = Path(sys.argv[1] if len(sys.argv) > 1 else "src/main.cpp").read_text(encoding="utf-8")

# One-frame wakeup stays coalesced and timestamped; the watchdog is scalar
# counters only and never records raw keyboard or mouse input.
assert "std::atomic<bool> frameWakeQueued{false}" in source
assert "std::atomic<ULONGLONG> frameWakePostedAtMs{0}" in source
assert "frameWakePostedAtMs.store(GetTickCount64()" in source
assert "frameWakePostedAtMs.exchange(0" in source
assert "maxFrameWakeLagMs" in source
assert "maxInputQueueLagMs" in source
assert "delayedInputMessages" in source

# Capture updates still run through the existing bounded mailbox and Win32
# wake, but no longer wait in DXGI Present on that same message-pump thread.
assert "case WM_NEW_FRAME:" in source
assert "else app->render(true);" in source
present_start = source.index("const auto presentStart = Clock::now();")
present_end = source.index("if (frame) {\n            if (interactiveMoveResize)", present_start)
present = source[present_start:present_end]
assert "swapChain->Present(" in present
assert "DXGI_PRESENT_DO_NOT_WAIT);" in present
assert "nonblockingPresent ? DXGI_PRESENT_DO_NOT_WAIT : 0u" not in present
assert "interactivePresent ? 0u : (vsyncEnabled ? 1u : 0u)" in present
assert "presentResult == DXGI_ERROR_WAS_STILL_DRAWING" in present
assert "busyNormalPresents" in present
assert "skippedInteractivePresent" in present
assert "presentResult == DXGI_STATUS_OCCLUDED" in present
assert "FAILED(presentResult)" in present

# No replacement of accepted native title-bar drag, Windows Snap or resize
# state-machine paths is allowed in this narrow fix.
assert "SendMessageW(GetParent(hwnd), WM_NCLBUTTONDOWN, HTCAPTION, 0)" in source
assert "case WM_ENTERSIZEMOVE:" in source
assert "case WM_EXITSIZEMOVE:" in source
assert "app->interactive_tick();" in source
assert "case WM_NCHITTEST:" in source
assert "bool vsyncEnabled{true}" in source

print("UI_ACTIVATION_NONBLOCKING_CONTRACT_PASS")
