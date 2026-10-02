"""Regression: capture reader must be created before its first use.

This catches the missing factory call that crashed the 2026-10-02 GPU build.
"""
import re
import sys
from pathlib import Path

source = Path(sys.argv[1] if len(sys.argv) > 1 else "src/main.cpp").read_text(encoding="utf-8")
start = source.index("void start_capture()")
end = source.index("while (!stopCapture)", start)
capture = source[start:end]
factory = re.search(r"\bhr\s*=\s*MFCreateSourceReaderFromMediaSource\s*\([^;]+&reader\s*\)", capture)
assert factory, "Capture worker does not create the source reader"
first_use = capture.index("reader->SetCurrentMediaType")
assert factory.end() < first_use, "Reader creation must precede media negotiation"
assert capture.count("MFCreateSourceReaderFromMediaSource") == 1, "Unexpected reader creation count"
guard = re.search(r"if\s*\(\s*SUCCEEDED\(hr\)\s*&&\s*!reader\s*\)\s*hr\s*=\s*E_POINTER\s*;", capture)
assert guard and factory.end() < guard.start() < first_use, "Missing reader null guard"
print("CAPTURE_READER_INIT_REGRESSION_PASS")
