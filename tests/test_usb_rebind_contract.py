"""Guard HCV-003 Media Foundation negotiation against USB-rebind regressions."""
from pathlib import Path
import sys

source = Path(sys.argv[1] if len(sys.argv) > 1 else "src/main.cpp").read_text(encoding="utf-8")

assert "bool media_type_matches_mode" in source
assert "HRESULT find_matching_native_media_type" in source
helper_start = source.index("HRESULT find_matching_native_media_type")
helper_end = source.index("std::wstring hresult_text", helper_start)
helper = source[helper_start:helper_end]
assert "GetNativeMediaType" in helper
assert "media_type_matches_mode" in helper
assert "MF_E_NO_MORE_TYPES" in helper
assert "MF_E_INVALIDMEDIATYPE" in helper

start = source.index("void start_capture()")
end = source.index("while (!stopCapture)", start)
capture = source[start:end]

# The capture worker must re-resolve the current device symbolic link rather
# than reuse an IMFActivate or source object from startup enumeration.
assert "MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, device.link.c_str()" in capture
assert "MFEnumDeviceSources" in capture

direct = capture.index("if (SUCCEEDED(hr) && directYuy2)")
fallback = capture.index("else if (SUCCEEDED(hr))", direct)
direct_block = capture[direct:fallback]
assert "find_matching_native_media_type(reader.Get(), mode, nativeOutput)" in direct_block
assert "nativeOutput.Get()" in direct_block
assert "SetCurrentMediaType" in direct_block
assert "MFCreateMediaType" not in direct_block

fallback_block = capture[fallback:]
assert "MFCreateMediaType" in fallback_block
assert "MFVideoFormat_RGB32" in fallback_block

assert "hresult_text(hr)" in capture
assert "MF_E_TOPO_CODEC_NOT_FOUND" in source
print("USB_REBIND_NATIVE_TYPE_CONTRACT_PASS")
