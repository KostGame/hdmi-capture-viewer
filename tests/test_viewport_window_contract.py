"""Keep fullscreen/viewport geometry and native Win32 border hit-tests."""
from pathlib import Path
import sys
s = Path(sys.argv[1] if len(sys.argv) > 1 else "src/main.cpp").read_text(encoding="utf-8")
for token in ("savedViewMode=viewMode", "savedPanX=panX",
    "MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST)", "mi.rcMonitor",
    "WS_OVERLAPPEDWINDOW", "case WM_NCCALCSIZE:",
    "case WM_NCHITTEST:", "HTBOTTOMRIGHT", "return HTCLIENT",
    "case WM_MOUSEWHEEL:", "D3D11_BLEND_SRC_ALPHA",
    "OMSetBlendState(overlayBlend.Get()", "nativeCaptionRevealed",
    "GetWindowLongPtrW(window, GWL_STYLE)", "SetWindowLongPtrW(window",
    "SWP_FRAMECHANGED", "poll_native_caption(GetTickCount64())"):
    assert token in s, f"missing viewport contract: {token}"
wnd = s[s.index("LRESULT CALLBACK window_proc"):]
hit = wnd[wnd.index("    case WM_NCHITTEST:"):wnd.index("    case WM_MOUSELEAVE:")]
assert "app->native_caption_active()" in hit
assert "HTCAPTION" not in hit
assert "DefWindowProcW" in hit
assert "WM_NCLBUTTONDOWN, HTCAPTION" not in s
assert "hcv::ViewMode viewMode{hcv::ViewMode::Auto};" in s
assert "Auto / whole frame (Shift+F10)" in s
assert "id == 3010" in s
print("VIEWPORT_NATIVE_WINDOW_CONTRACT_PASS")
