"""HCV-005 controls must stay isolated from user installed viewer."""
from pathlib import Path
src=Path("src/main.cpp").read_text(encoding="utf-8")
shell=Path("src/win32_shell_probe.cpp").read_text(encoding="utf-8")
wf=Path(".github/workflows/hcv005-diagnostic.yml").read_text(encoding="utf-8")

for name in ("#ifdef HCV005_NO_VIDEO", "Diagnostic control B",
             "capture and D3D rendering intentionally disabled"):
    assert name in src, name
assert src.count("#ifdef HCV005_NO_VIDEO") == 1
assert "HCV005_NO_VIDEO) || defined(HCV005_CAPTURE_ONLY)" in src
assert "HCV005 diagnostic - no video rendering" in src
assert "HCV005_CAPTURE_ONLY" in src and "HCV005_GPU_ONLY" in src
assert "SetTimer(state.window, 31005, 33, nullptr)" in src
assert "void render(bool consumeFrame" in src
assert "#else\n    if (state.devices.empty())" in src
assert 'state.start_capture();\n    else state.set_status(' in src
assert "WS_OVERLAPPEDWINDOW" in shell
for token in ("GetMessageW", "TranslateMessage", "DispatchMessageW",
              "WM_MOUSEACTIVATE", "WM_LBUTTONDOWN", "WM_RBUTTONUP",
              "WM_ENTERSIZEMOVE", "WM_EXITSIZEMOVE", "TrackPopupMenuEx"):
    assert token in shell, token
for token in ("mfplat", "d3d11", "dxgi", "HcvChromeOverlay", "Present(",
              "SetCapture", "ReleaseCapture"):
    assert token not in shell, f"Pure shell contains forbidden dependency: {token}"
assert "HCV005_NO_VIDEO" in wf
assert "hcv005-win32-shell.exe" in wf
assert "hcv005-viewer-no-video.exe" in wf
assert "on:" in wf and "workflow_dispatch" in wf
assert "push:" in wf and "agent/9-win32-isolation-baseline" in wf
assert "hdmi-capture-viewer.exe" not in wf
print("HCV005_DIAGNOSTIC_ISOLATION_CONTRACT_PASS")
