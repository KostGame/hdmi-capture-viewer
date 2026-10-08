"""Guard borderless state restoration and frameless hit testing."""
import pathlib
import sys

source = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8")
for token in ("savedViewMode=viewMode", "savedPanX=panX",
              "MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST)", "mi.rcMonitor",
              "WS_OVERLAPPEDWINDOW", "case WM_NCCALCSIZE:",
              "case WM_NCHITTEST:", "HTBOTTOMRIGHT", "return HTCLIENT", "case WM_MOUSEWHEEL:",
              "D3D11_BLEND_SRC_ALPHA", "OMSetBlendState(overlayBlend.Get()", "CreateWindowExW(0, L\"HcvChromeOverlay\""):
    assert token in source, f"missing borderless/viewport contract: {token}"
main = source[source.index("LRESULT CALLBACK window_proc"):]
hit_start = main.index("    case WM_NCHITTEST:")
hit_end = main.index("    case WM_MOUSELEAVE:", hit_start)
assert "HTCAPTION" in main[hit_start:hit_end]
assert "WM_NCLBUTTONDOWN, HTCAPTION, 0" not in source
assert "SWP_FRAMECHANGED" in source
assert "hcv::ViewMode viewMode{hcv::ViewMode::Auto};" in source
assert "Auto / whole frame (Shift+F10)" in source
assert "id == 3010" in source
