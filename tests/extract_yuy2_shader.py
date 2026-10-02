"""Extract the exact embedded HLSL pixel shader for Windows CI compilation."""
import ast
import re
import sys
from pathlib import Path

if len(sys.argv) != 3:
    raise SystemExit("usage: extract_yuy2_shader.py main.cpp output.hlsl")

source = Path(sys.argv[1]).read_text(encoding="utf-8")
start = source.index("const char* yuy2Shader =")
end = source.index("ComPtr<ID3DBlob> yuy2Ps;", start)
fragments = re.findall(r'"(?:\\.|[^"\\])*"', source[start:end])
if len(fragments) < 5:
    raise SystemExit("could not extract the embedded GPU YUY2 shader")
shader = "".join(ast.literal_eval(fragment) for fragment in fragments)
if "packedTexture.GetDimensions" not in shader or "packedTexture.Load" not in shader:
    raise SystemExit("extracted shader does not implement packed YUY2 loading")
Path(sys.argv[2]).write_text(shader + "\n", encoding="utf-8")
print("Extracted embedded GPU YUY2 HLSL for fxc CI validation")
