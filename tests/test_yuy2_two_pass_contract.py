"""Guard source-resolution GPU YUY2 conversion and cheap RGB presentation."""
from pathlib import Path
import sys

source = Path(sys.argv[1] if len(sys.argv) > 1 else "src/main.cpp").read_text(encoding="utf-8")
assert "ComPtr<ID3D11Texture2D> yuy2RgbTexture;" in source
assert "ComPtr<ID3D11RenderTargetView> yuy2RgbTarget;" in source
assert "ComPtr<ID3D11ShaderResourceView> yuy2RgbView;" in source
assert "rgb.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;" in source
assert "CreateRenderTargetView(yuy2RgbTexture.Get()" in source

shader_start = source.index('const char* yuy2Shader =')
shader_end = source.index("ComPtr<ID3DBlob> yuy2Ps;", shader_start)
shader = source[shader_start:shader_end]
assert "uint x=min((uint)i.p.x,ow-1),y=min((uint)i.p.y,oh-1);" in shader
assert "floor(" not in shader and "frac(" not in shader and "x0" not in shader, "Conversion must decode one source pixel per output pixel"

convert_start = source.index("    bool convert_yuy2_frame()")
convert_end = source.index("\n    void interactive_tick()", convert_start)
convert = source[convert_start:convert_end]
for token in ("OMGetRenderTargets", "PSGetShaderResources(0, 1, &previousResource)", "RSGetViewports",
              "yuy2RgbTarget.Get()", "yuy2PixelShader.Get()", "PSSetShaderResources(0, 1, &none)",
              "PSSetShaderResources(0, 1, &previousResource)", "OMSetRenderTargets", "RSSetViewports",
              "yuy2ConvertSubmitMs"):
    assert token in convert, f"missing two-pass GPU state handling: {token}"

assert "if (gpuYuy2Active && !convert_yuy2_frame())" in source
assert "gpuYuy2Active ? yuy2RgbView.Get() : videoView.Get()" in source
assert "context->PSSetShader(pixelShader.Get()" in source
assert 'L"; GPU YUY2 two-pass "' in source
assert 'L"ms CPU; app age "' in source, "Expose CPU submission time without implying GPU timing"
print("YUY2_TWO_PASS_CONTRACT_PASS")
