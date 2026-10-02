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
hit_start = source.index("    case WM_NCHITTEST:")
hit_end = source.index("    case WM_MOUSEWHEEL:", hit_start)
assert "HTCAPTION" not in source[hit_start:hit_end]
assert "WM_NCLBUTTONDOWN, HTCAPTION" in source
assert "hcv::ViewMode viewMode{hcv::ViewMode::Auto};" in source
assert "Auto / whole frame (Shift+F10)" in source
assert "id == 3010" in source
