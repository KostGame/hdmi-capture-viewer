"""HCV-004 R3: bounded UI frame rendering and real Windows caption."""
from pathlib import Path
import sys
s = Path(sys.argv[1] if len(sys.argv) > 1 else "src/main.cpp").read_text(encoding="utf-8")
for token in ("std::atomic<bool> frameWakeQueued{false}",
              "std::atomic<ULONGLONG> frameWakePostedAtMs{0}",
              "frameWakePostedAtMs.store(GetTickCount64()",
              "frameWakePostedAtMs.exchange(0", "maxFrameWakeLagMs",
              "maxInputQueueLagMs", "delayedInputMessages"):
    assert token in s
present = s[s.index("const auto presentStart = Clock::now();"):s.index(
    "if (frame) {\n            if (interactiveMoveResize)", s.index("const auto presentStart = Clock::now();"))]
for token in ("swapChain->Present(", "DXGI_PRESENT_DO_NOT_WAIT);",
              "DXGI_ERROR_WAS_STILL_DRAWING", "DXGI_STATUS_OCCLUDED",
              "busyNormalPresents", "skippedInteractivePresent"):
    assert token in present, token
assert "nonblockingPresent ? DXGI_PRESENT_DO_NOT_WAIT : 0u" not in present
assert "case WM_NEW_FRAME:" in s and "else app->render(true);" in s
assert "WM_NCLBUTTONDOWN, HTCAPTION, 0" not in s
assert "void poll_native_caption(ULONGLONG nowMs)" in s
assert "case WM_ENTERSIZEMOVE:" in s
assert "case WM_EXITSIZEMOVE:" in s
assert "app->interactive_tick();" in s
assert "bool vsyncEnabled{true}" in s
print("UI_NATIVE_HOVER_NONBLOCKING_CONTRACT_PASS")
