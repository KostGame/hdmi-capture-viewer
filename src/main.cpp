#define NOMINMAX
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <mfobjects.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#include "latest_frame.hpp"
#include "resize_gate.hpp"
#include "window_geometry.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "ole32.lib")

using Microsoft::WRL::ComPtr;
using Clock = std::chrono::steady_clock;

namespace {
constexpr UINT WM_NEW_FRAME = WM_APP + 1;
constexpr UINT WM_CAPTURE_STATUS = WM_APP + 2;
constexpr UINT_PTR LIVE_MOVE_TIMER = 7;
constexpr UINT LIVE_MOVE_TIMER_PERIOD_MS = 16;
constexpr UINT DEVICE_COMMAND_BASE = 1000;
constexpr UINT MODE_COMMAND_BASE = 2000;
constexpr UINT MAX_MENU_ITEMS = 500;

struct Mode {
    GUID subtype{};
    UINT32 width{};
    UINT32 height{};
    UINT32 fpsNumerator{};
    UINT32 fpsDenominator{1};
};
struct Device {
    std::wstring name;
    std::wstring link;
    std::vector<Mode> modes;
};
struct Vertex { float x, y, u, v; };

// COM initialization is per thread. The UI apartment does not cover capture.
struct CaptureComApartment {
    HRESULT result{CoInitializeEx(nullptr, COINIT_MULTITHREADED)};
    ~CaptureComApartment() { if (SUCCEEDED(result)) CoUninitialize(); }
};

std::wstring subtype_name(REFGUID id) {
    if (IsEqualGUID(id, MFVideoFormat_YUY2)) return L"YUY2";
    if (IsEqualGUID(id, MFVideoFormat_MJPG)) return L"MJPEG";
    if (IsEqualGUID(id, MFVideoFormat_NV12)) return L"NV12";
    if (IsEqualGUID(id, MFVideoFormat_RGB32)) return L"RGB32";
    if (IsEqualGUID(id, MFVideoFormat_UYVY)) return L"UYVY";
    return L"other";
}

std::vector<Device> enumerate_devices() {
    std::vector<Device> result;
    ComPtr<IMFAttributes> attrs;
    if (FAILED(MFCreateAttributes(&attrs, 1))) return result;
    attrs->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    IMFActivate** activations = nullptr;
    UINT32 count = 0;
    if (FAILED(MFEnumDeviceSources(attrs.Get(), &activations, &count))) return result;
    for (UINT32 i = 0; i < count; ++i) {
        Device device;
        WCHAR* text = nullptr;
        UINT32 chars = 0;
        if (SUCCEEDED(activations[i]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &text, &chars))) {
            device.name.assign(text, chars);
            CoTaskMemFree(text);
        }
        if (SUCCEEDED(activations[i]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, &text, &chars))) {
            device.link.assign(text, chars);
            CoTaskMemFree(text);
        }
        ComPtr<IMFMediaSource> source;
        ComPtr<IMFSourceReader> reader;
        if (SUCCEEDED(activations[i]->ActivateObject(IID_PPV_ARGS(&source))) &&
            SUCCEEDED(MFCreateSourceReaderFromMediaSource(source.Get(), nullptr, &reader))) {
            for (DWORD index = 0;; ++index) {
                ComPtr<IMFMediaType> type;
                if (FAILED(reader->GetNativeMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, index, &type))) break;
                GUID subtype{};
                UINT32 width = 0, height = 0, fn = 0, fd = 1;
                if (SUCCEEDED(type->GetGUID(MF_MT_SUBTYPE, &subtype)) &&
                    SUCCEEDED(MFGetAttributeSize(type.Get(), MF_MT_FRAME_SIZE, &width, &height))) {
                    MFGetAttributeRatio(type.Get(), MF_MT_FRAME_RATE, &fn, &fd);
                    if (!fd) fd = 1;
                    auto duplicate = std::find_if(device.modes.begin(), device.modes.end(), [&](const Mode& m) {
                        return IsEqualGUID(m.subtype, subtype) && m.width == width && m.height == height &&
                            static_cast<std::uint64_t>(m.fpsNumerator) * fd == static_cast<std::uint64_t>(fn) * m.fpsDenominator;
                    });
                    if (duplicate == device.modes.end()) device.modes.push_back({subtype, width, height, fn, fd});
                }
            }
            reader.Reset();
            source->Shutdown();
        }
        if (device.name.empty()) device.name = L"UVC capture device";
        if (!device.modes.empty()) result.push_back(std::move(device));
        activations[i]->Release();
    }
    CoTaskMemFree(activations);
    return result;
}

double fps(const Mode& m) { return m.fpsDenominator ? double(m.fpsNumerator) / m.fpsDenominator : 0.0; }
std::size_t preferred_mode(const Device& d) {
    auto score = [](const Mode& m) {
        const bool target = IsEqualGUID(m.subtype, MFVideoFormat_YUY2) && m.width == 1920 && m.height == 1080 && fps(m) >= 59.0 && fps(m) <= 61.0;
        const bool yuy2 = IsEqualGUID(m.subtype, MFVideoFormat_YUY2);
        return std::tuple{target ? 1 : 0, yuy2 ? 1 : 0,
                          static_cast<std::uint64_t>(m.width) * m.height,
                          static_cast<int>(fps(m) * 1000)};
    };
    std::size_t best = 0;
    for (std::size_t i = 1; i < d.modes.size(); ++i) if (score(d.modes[i]) > score(d.modes[best])) best = i;
    return best;
}

struct App {
    HWND window{};
    std::vector<Device> devices;
    std::size_t selectedDevice{};
    std::size_t selectedMode{};
    std::thread captureThread;
    std::atomic<bool> stopCapture{false};
    std::atomic<bool> frameWakeQueued{false};
    hcv::LatestFrame pending;
    std::uint64_t nextSequence{};
    std::wstring captureStatus = L"Choose a capture device from the Device menu.";
    std::wstring diagnostic;
    std::uint64_t presentedFrames{};
    Clock::time_point fpsWindow = Clock::now();
    double renderFps{};
    double submitMs{};
    double captureCopyMs{};
    double captureFps{};
    double captureP95Ms{};
    double presentMs{};
    std::uint64_t captureFrames{};
    Clock::time_point captureWindow = Clock::now();
    Clock::time_point previousCapture{};
    std::vector<double> captureIntervals;
    std::mutex captureMetricsMutex;
    Clock::time_point lastTitleUpdate{};
    bool borderless{};
    bool vsyncEnabled{true};
    bool interactiveMoveResize{};
    hcv::ResizeGate resizeGate;
    Clock::time_point lastInteractivePaint{};
    UINT swapWidth{}, swapHeight{};
    std::uint64_t resizeCount{}, resizeFailures{}, liveMoveFrames{};
    HRESULT lastResizeError{S_OK};
    WINDOWPLACEMENT savedPlacement{sizeof(WINDOWPLACEMENT)};
    DWORD savedStyle{};
    RECT savedClientRect{};

    ComPtr<ID3D11Device> d3d;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGISwapChain> swapChain;
    ComPtr<ID3D11VertexShader> vertexShader;
    ComPtr<ID3D11PixelShader> pixelShader;
    ComPtr<ID3D11InputLayout> inputLayout;
    ComPtr<ID3D11Buffer> vertexBuffer;
    ComPtr<ID3D11SamplerState> sampler;
    ComPtr<ID3D11RenderTargetView> backBufferView;
    ComPtr<ID3D11Texture2D> videoTexture;
    ComPtr<ID3D11ShaderResourceView> videoView;
    UINT textureWidth{};
    UINT textureHeight{};
    UINT displayWidth{};
    hcv::PixelFormat textureFormat{hcv::PixelFormat::Bgra32};
    ComPtr<ID3D11PixelShader> yuy2PixelShader;
    bool gpuYuy2Active{};

    void set_status(std::wstring value) {
        captureStatus = std::move(value);
        update_title(true);
    }
    void update_title(bool force = false) {
        // Caption changes trigger nonclient updates. Avoid them in the move/size modal loop.
        if (interactiveMoveResize) return;
        const auto now = Clock::now();
        if (!force && lastTitleUpdate.time_since_epoch().count() && now - lastTitleUpdate < std::chrono::seconds(1)) return;
        lastTitleUpdate = now;
        std::wstring title = L"HDMI Capture Viewer — " + captureStatus;
        if (!diagnostic.empty()) title += L" | " + diagnostic;
        SetWindowTextW(window, title.c_str());
    }
    void stop_capture() {
        stopCapture = true;
        if (captureThread.joinable()) captureThread.join();
    }
    std::wstring mode_label(const Mode& m) const {
        return std::to_wstring(m.width) + L" x " + std::to_wstring(m.height) + L" @ " +
            std::to_wstring(static_cast<int>(std::lround(fps(m)))) + L" fps " + subtype_name(m.subtype);
    }
    void start_capture() {
        stop_capture();
        pending.take();
        if (devices.empty() || selectedDevice >= devices.size()) { set_status(L"No capture devices with readable video formats found."); return; }
        auto device = devices[selectedDevice];
        if (selectedMode >= device.modes.size()) selectedMode = preferred_mode(device);
        const Mode mode = device.modes[selectedMode];
        stopCapture = false;
        set_status(L"Opening " + device.name + L"; requesting " + mode_label(mode));
        captureThread = std::thread([this, device = std::move(device), mode]() {
            CaptureComApartment com;
            if (FAILED(com.result)) {
                auto* error = new std::wstring(L"Capture thread COM initialization failed: " +
                    std::to_wstring(static_cast<unsigned long>(com.result)));
                if (!PostMessageW(window, WM_CAPTURE_STATUS, 0, reinterpret_cast<LPARAM>(error))) delete error;
                return;
            }
            ComPtr<IMFAttributes> attrs;
            ComPtr<IMFMediaSource> source;
            ComPtr<IMFSourceReader> reader;
            const bool directYuy2 = IsEqualGUID(mode.subtype, MFVideoFormat_YUY2);
            HRESULT hr = MFCreateAttributes(&attrs, 2);
            if (SUCCEEDED(hr)) hr = attrs->SetUINT32(MF_LOW_LATENCY, TRUE);
            if (SUCCEEDED(hr) && !directYuy2) hr = attrs->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
            if (SUCCEEDED(hr) && directYuy2) hr = attrs->SetUINT32(MF_READWRITE_DISABLE_CONVERTERS, TRUE);
            ComPtr<IMFAttributes> activateAttrs;
            if (SUCCEEDED(hr)) hr = MFCreateAttributes(&activateAttrs, 2);
            if (SUCCEEDED(hr)) hr = activateAttrs->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
            if (SUCCEEDED(hr)) hr = activateAttrs->SetString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, device.link.c_str());
            IMFActivate** list = nullptr;
            UINT32 count = 0;
            if (SUCCEEDED(hr)) hr = MFEnumDeviceSources(activateAttrs.Get(), &list, &count);
            if (SUCCEEDED(hr) && count == 0) hr = MF_E_NOT_FOUND;
            if (SUCCEEDED(hr)) hr = list[0]->ActivateObject(IID_PPV_ARGS(&source));
            if (list) { for (UINT32 i = 0; i < count; ++i) list[i]->Release(); CoTaskMemFree(list); }
            // Do not proceed with output negotiation until the reader exists.
            if (SUCCEEDED(hr) && !source) hr = E_POINTER;
            if (SUCCEEDED(hr)) hr = MFCreateSourceReaderFromMediaSource(source.Get(), attrs.Get(), &reader);
            if (SUCCEEDED(hr) && !reader) hr = E_POINTER;
            ComPtr<IMFMediaType> output;
            if (SUCCEEDED(hr)) hr = MFCreateMediaType(&output);
            if (SUCCEEDED(hr)) hr = output->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
            if (SUCCEEDED(hr)) hr = output->SetGUID(MF_MT_SUBTYPE, directYuy2 ? MFVideoFormat_YUY2 : MFVideoFormat_RGB32);
            if (SUCCEEDED(hr)) hr = MFSetAttributeSize(output.Get(), MF_MT_FRAME_SIZE, mode.width, mode.height);
            if (SUCCEEDED(hr)) hr = MFSetAttributeRatio(output.Get(), MF_MT_FRAME_RATE, mode.fpsNumerator, mode.fpsDenominator);
            if (SUCCEEDED(hr)) hr = MFSetAttributeRatio(output.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
            if (SUCCEEDED(hr)) hr = reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, output.Get());
            if (SUCCEEDED(hr)) hr = reader->SetStreamSelection(MF_SOURCE_READER_FIRST_VIDEO_STREAM, TRUE);
            if (FAILED(hr)) {
                auto* message = new std::wstring(L"Could not open requested " + subtype_name(mode.subtype) + L" " +
                    std::to_wstring(mode.width) + L"x" + std::to_wstring(mode.height) + L" @ " +
                    std::to_wstring(static_cast<int>(std::lround(fps(mode)))) + L"; Media Foundation error " + std::to_wstring(static_cast<unsigned long>(hr)) + L". Choose another listed format.");
                PostMessageW(window, WM_CAPTURE_STATUS, 0, reinterpret_cast<LPARAM>(message));
                if (source) source->Shutdown();
                return;
            }
            ComPtr<IMFMediaType> negotiated;
            GUID negotiatedSubtype{};
            LONG negotiatedStride = 0;
            if (SUCCEEDED(reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &negotiated))) {
                negotiated->GetGUID(MF_MT_SUBTYPE, &negotiatedSubtype);
                UINT32 stride = 0;
                if (SUCCEEDED(negotiated->GetUINT32(MF_MT_DEFAULT_STRIDE, &stride))) negotiatedStride = static_cast<LONG>(stride);
            }
            if (directYuy2 && (!IsEqualGUID(negotiatedSubtype, MFVideoFormat_YUY2) || (mode.width & 1))) {
                auto* error = new std::wstring(L"YUY2 GPU path requires negotiated YUY2 output and even frame width; choose another listed format.");
                if (!PostMessageW(window, WM_CAPTURE_STATUS, 0, reinterpret_cast<LPARAM>(error))) delete error;
                reader.Reset(); source->Shutdown(); return;
            }
            auto* message = new std::wstring(L"Capturing " + device.name + L"; selected advertised " + subtype_name(mode.subtype) +
                L" " + std::to_wstring(mode.width) + L"x" + std::to_wstring(mode.height) + L" @ " +
                std::to_wstring(static_cast<int>(std::lround(fps(mode)))) + L"; Source Reader output " + subtype_name(negotiatedSubtype) +
                (directYuy2 ? L" (converters disabled; GPU YUY2 selected)" : L" (decoded/converted; native input subtype not confirmed"));
            if (!directYuy2) *message += L")";
            if (!PostMessageW(window, WM_CAPTURE_STATUS, 0, reinterpret_cast<LPARAM>(message))) delete message;
            while (!stopCapture) {
                DWORD stream = 0, flags = 0;
                LONGLONG timestamp = 0;
                ComPtr<IMFSample> sample;
                hr = reader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &stream, &flags, &timestamp, &sample);
                const auto sampleReadyAt = Clock::now();
                if (FAILED(hr)) {
                    auto* error = new std::wstring(L"Capture stopped: Media Foundation read error " + std::to_wstring(static_cast<unsigned long>(hr)));
                    PostMessageW(window, WM_CAPTURE_STATUS, 0, reinterpret_cast<LPARAM>(error));
                    break;
                }
                if (flags & MF_SOURCE_READERF_ENDOFSTREAM) break;
                if (!sample) continue;
                ComPtr<IMFMediaBuffer> buffer;
                if (FAILED(sample->ConvertToContiguousBuffer(&buffer))) continue;
                BYTE* data = nullptr;
                DWORD maxLength = 0, length = 0;
                if (FAILED(buffer->Lock(&data, &maxLength, &length))) continue;
                const std::size_t bytesPerPixel = 4;
                const std::size_t rowBytes = static_cast<std::size_t>(mode.width) * (directYuy2 ? 2u : bytesPerPixel);
                std::size_t yuy2Bytes = 0;
                const bool validYuy2Size = hcv::packed_yuy2_size(static_cast<int>(mode.width), static_cast<int>(mode.height), yuy2Bytes);
                const LONG pitch = negotiatedStride ? negotiatedStride : static_cast<LONG>(rowBytes);
                const std::size_t sourcePitch = pitch > 0 ? static_cast<std::size_t>(pitch) : 0;
                const std::size_t frameBytes = rowBytes * mode.height;
                const bool layoutValid = pitch > 0 && sourcePitch >= rowBytes &&
                    sourcePitch <= (static_cast<std::size_t>(-1) - rowBytes) / mode.height &&
                    sourcePitch * (mode.height - 1) + rowBytes <= length;
                if ((!directYuy2 || validYuy2Size) && !layoutValid) {
                    buffer->Unlock();
                    auto* error = new std::wstring(L"Capture buffer stride or length is unsupported; refusing unsafe frame copy.");
                    if (!PostMessageW(window, WM_CAPTURE_STATUS, 0, reinterpret_cast<LPARAM>(error))) delete error;
                    break;
                }
                if ((!directYuy2 || validYuy2Size) && layoutValid) {
                    {
                        std::lock_guard lock(captureMetricsMutex);
                        if (previousCapture.time_since_epoch().count()) captureIntervals.push_back(std::chrono::duration<double, std::milli>(sampleReadyAt - previousCapture).count());
                        previousCapture = sampleReadyAt;
                        ++captureFrames;
                        const auto elapsed = std::chrono::duration<double>(sampleReadyAt - captureWindow).count();
                        if (elapsed >= 1.0) {
                            captureFps = captureFrames / elapsed;
                            if (!captureIntervals.empty()) {
                                auto sorted = captureIntervals;
                                std::sort(sorted.begin(), sorted.end());
                                captureP95Ms = sorted[static_cast<std::size_t>(0.95 * (sorted.size() - 1))];
                            }
                            captureFrames = 0;
                            captureIntervals.clear();
                            captureWindow = sampleReadyAt;
                        }
                    }
                    hcv::Frame frame;
                    frame.width = static_cast<int>(mode.width);
                    frame.height = static_cast<int>(mode.height);
                    frame.sequence = ++nextSequence;
                    frame.capturedAt = sampleReadyAt;
                    const auto conversionStart = Clock::now();
                    frame.format = directYuy2 ? hcv::PixelFormat::Yuy2 : hcv::PixelFormat::Bgra32;
                    frame.pixels.resize(frameBytes);
                    for (UINT32 y = 0; y < mode.height; ++y)
                        std::memcpy(frame.pixels.data() + static_cast<std::size_t>(y) * rowBytes,
                            data + static_cast<std::size_t>(y) * sourcePitch, rowBytes);
                    frame.captureCopyMs = std::chrono::duration<double, std::milli>(Clock::now() - conversionStart).count();
                    pending.publish(std::move(frame));
                    // Keep the Win32 message queue bounded as well as the frame mailbox.
                    if (!frameWakeQueued.exchange(true, std::memory_order_acq_rel)) {
                        if (!PostMessageW(window, WM_NEW_FRAME, 0, 0))
                            frameWakeQueued.store(false, std::memory_order_release);
                    }
                }
                buffer->Unlock();
            }
            reader.Reset();
            if (source) source->Shutdown();
        });
    }

    bool init_d3d() {
        RECT rect{}; GetClientRect(window, &rect);
        DXGI_SWAP_CHAIN_DESC desc{};
        desc.BufferDesc.Width = std::max<LONG>(1, rect.right - rect.left);
        desc.BufferDesc.Height = std::max<LONG>(1, rect.bottom - rect.top);
        desc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = 2;
        desc.OutputWindow = window;
        desc.Windowed = TRUE;
        desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        D3D_FEATURE_LEVEL level{};
        HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
            D3D11_SDK_VERSION, &desc, &swapChain, &d3d, &level, &context);
        if (FAILED(hr)) return false;
        swapWidth = desc.BufferDesc.Width;
        swapHeight = desc.BufferDesc.Height;
        const char* shader = "Texture2D videoTexture : register(t0); SamplerState videoSampler : register(s0);"
            "struct V { float2 p : POSITION; float2 uv : TEXCOORD; }; struct P { float4 p : SV_POSITION; float2 uv : TEXCOORD; };"
            "P VS(V i) { P o; o.p=float4(i.p,0,1); o.uv=i.uv; return o; }"
            "float4 PS(P i) : SV_TARGET { return videoTexture.Sample(videoSampler,i.uv); }";
        ComPtr<ID3DBlob> vs, ps, errors;
        hr = D3DCompile(shader, strlen(shader), nullptr, nullptr, nullptr, "VS", "vs_4_0", 0, 0, &vs, &errors);
        if (FAILED(hr)) return false;
        hr = D3DCompile(shader, strlen(shader), nullptr, nullptr, nullptr, "PS", "ps_4_0", 0, 0, &ps, &errors);
        if (FAILED(hr)) return false;
        const char* yuy2Shader =
            "Texture2D packedTexture : register(t0);"
            "struct P { float4 p : SV_POSITION; float2 uv : TEXCOORD; };"
            "float3 decodeYuy2(uint x,uint y,uint tw,uint th){ uint ow=tw*2; x=min(x,ow-1); y=min(y,th-1);"
            "float4 p=packedTexture.Load(int3(x/2,y,0)); float yy=((x&1)==0?p.r:p.b)*255.0;"
            "float u=p.g*255.0-128.0,v=p.a*255.0-128.0,c=max(0.0,yy-16.0);"
            "return saturate(float3((298*c+409*v+128)/256.0/255.0,"
            "(298*c-100*u-208*v+128)/256.0/255.0,(298*c+516*u+128)/256.0/255.0));}"
            "float4 PS(P i) : SV_TARGET { uint tw,th; packedTexture.GetDimensions(tw,th); uint ow=tw*2,oh=th;"
            "float2 pos=saturate(i.uv)*float2(ow,oh)-0.5; pos=clamp(pos,float2(0,0),float2(ow-1,oh-1));"
            "uint x0=(uint)floor(pos.x),y0=(uint)floor(pos.y); uint x1=min(x0+1,ow-1),y1=min(y0+1,oh-1);"
            "float2 f=frac(pos); float3 a=lerp(decodeYuy2(x0,y0,tw,th),decodeYuy2(x1,y0,tw,th),f.x);"
            "float3 b=lerp(decodeYuy2(x0,y1,tw,th),decodeYuy2(x1,y1,tw,th),f.x);"
            "return float4(lerp(a,b,f.y),1.0); }";
        ComPtr<ID3DBlob> yuy2Ps;
        hr = D3DCompile(yuy2Shader, strlen(yuy2Shader), nullptr, nullptr, nullptr, "PS", "ps_4_0", 0, 0, &yuy2Ps, &errors);
        if (FAILED(hr) || FAILED(d3d->CreatePixelShader(yuy2Ps->GetBufferPointer(), yuy2Ps->GetBufferSize(), nullptr, &yuy2PixelShader))) return false;
        if (FAILED(d3d->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, &vertexShader)) ||
            FAILED(d3d->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr, &pixelShader))) return false;
        D3D11_INPUT_ELEMENT_DESC elements[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0}
        };
        if (FAILED(d3d->CreateInputLayout(elements, 2, vs->GetBufferPointer(), vs->GetBufferSize(), &inputLayout))) return false;
        const Vertex vertices[] = {{-1,1,0,0},{1,1,1,0},{-1,-1,0,1},{1,-1,1,1}};
        D3D11_BUFFER_DESC vb{}; vb.ByteWidth = sizeof(vertices); vb.Usage = D3D11_USAGE_IMMUTABLE; vb.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA initial{}; initial.pSysMem = vertices;
        if (FAILED(d3d->CreateBuffer(&vb, &initial, &vertexBuffer))) return false;
        D3D11_SAMPLER_DESC sd{}; sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR; sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        if (FAILED(d3d->CreateSamplerState(&sd, &sampler))) return false;
        return true;
    }

    // WM_SIZE only records the newest dimensions. Releasing the render target
    // and resizing the swap chain at every intermediate mouse position causes
    // severe stalls and flicker during interactive drag/resize.
    bool apply_pending_resize(bool force = false) {
        if (!swapChain) return true;
        const auto now = Clock::now();
        if (!resizeGate.ready(now, interactiveMoveResize && !force)) return true;
        const UINT w = static_cast<UINT>(resizeGate.width());
        const UINT h = static_cast<UINT>(resizeGate.height());
        if (w == swapWidth && h == swapHeight) {
            resizeGate.completed(now);
            return true;
        }
        context->OMSetRenderTargets(0, nullptr, nullptr);
        backBufferView.Reset();
        context->Flush();
        const HRESULT hr = swapChain->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN, 0);
        resizeGate.attempted(now);
        if (FAILED(hr)) {
            ++resizeFailures;
            lastResizeError = hr;
            return false; // Do not draw to a discarded or mismatched buffer.
        }
        swapWidth = w;
        swapHeight = h;
        ++resizeCount;
        resizeGate.completed(now);
        return true;
    }

    void interactive_tick() {
        if (!interactiveMoveResize) return;
        const auto now = Clock::now();
        if (lastInteractivePaint.time_since_epoch().count() &&
            now - lastInteractivePaint < std::chrono::milliseconds(15)) return;
        lastInteractivePaint = now;
        render(true);
    }

    void render(bool consumeFrame) {
        if (!window || IsIconic(window)) return;
        if (!d3d && !init_d3d()) { set_status(L"Direct3D 11 initialization failed."); return; }
        auto frame = consumeFrame ? pending.take() : std::optional<hcv::Frame>{};
        // Timer + posted wakeups may race to process the same frame. Never
        // present duplicates unless a resize or explicit paint needs it.
        if (consumeFrame && !frame && !resizeGate.ready(Clock::now(), interactiveMoveResize)) return;
        if (!apply_pending_resize()) return;
        if (frame) {
            const UINT texWidth = static_cast<UINT>(frame->format == hcv::PixelFormat::Yuy2 ? frame->width / 2 : frame->width);
            const UINT texHeight = static_cast<UINT>(frame->height);
            if (textureWidth != texWidth || textureHeight != texHeight || textureFormat != frame->format) {
                videoView.Reset(); videoTexture.Reset();
                D3D11_TEXTURE2D_DESC td{}; td.Width = texWidth; td.Height = texHeight; td.MipLevels = td.ArraySize = 1;
                td.Format = frame->format == hcv::PixelFormat::Yuy2 ? DXGI_FORMAT_R8G8B8A8_UNORM : DXGI_FORMAT_B8G8R8A8_UNORM;
                td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                if (FAILED(d3d->CreateTexture2D(&td, nullptr, &videoTexture)) || FAILED(d3d->CreateShaderResourceView(videoTexture.Get(), nullptr, &videoView))) return;
                textureWidth = texWidth; textureHeight = texHeight; textureFormat = frame->format;
            }
            const std::size_t expected = static_cast<std::size_t>(frame->width) * frame->height * (frame->format == hcv::PixelFormat::Yuy2 ? 2 : 4);
            if (frame->pixels.size() != expected || (frame->format == hcv::PixelFormat::Yuy2 && (frame->width & 1))) {
                set_status(L"Invalid frame byte size or dimensions; frame rejected."); return;
            }
            context->UpdateSubresource(videoTexture.Get(), 0, nullptr, frame->pixels.data(), texWidth * 4, 0);
            displayWidth = static_cast<UINT>(frame->width);
            gpuYuy2Active = frame->format == hcv::PixelFormat::Yuy2;
            submitMs = std::chrono::duration<double, std::milli>(Clock::now() - frame->capturedAt).count();
            captureCopyMs = frame->captureCopyMs;
        }
        // During a deferred resize, use the dimensions of the *actual*
        // swap-chain target, not the potentially newer client rectangle.
        const float width = static_cast<float>(swapWidth), height = static_cast<float>(swapHeight);
        D3D11_VIEWPORT viewport{}; viewport.Width = width; viewport.Height = height; viewport.MaxDepth = 1;
        context->RSSetViewports(1, &viewport);
        if (!backBufferView) {
            ComPtr<ID3D11Texture2D> backBuffer;
            if (SUCCEEDED(swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer)))) d3d->CreateRenderTargetView(backBuffer.Get(), nullptr, &backBufferView);
        }
        if (backBufferView) {
            const float black[] = {0,0,0,1}; ID3D11RenderTargetView* target = backBufferView.Get(); context->OMSetRenderTargets(1, &target, nullptr); context->ClearRenderTargetView(backBufferView.Get(), black);
            if (videoView && textureWidth && textureHeight && width > 0 && height > 0) {
                const float scale = std::min(width / displayWidth, height / textureHeight);
                D3D11_VIEWPORT image{}; image.Width = displayWidth * scale; image.Height = textureHeight * scale;
                image.TopLeftX = (width - image.Width) * 0.5f; image.TopLeftY = (height - image.Height) * 0.5f; image.MaxDepth = 1;
                context->RSSetViewports(1, &image);
                UINT stride = sizeof(Vertex), offset = 0; ID3D11Buffer* vb = vertexBuffer.Get();
                context->IASetInputLayout(inputLayout.Get()); context->IASetVertexBuffers(0, 1, &vb, &stride, &offset); context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
                context->VSSetShader(vertexShader.Get(), nullptr, 0); context->PSSetShader(gpuYuy2Active ? yuy2PixelShader.Get() : pixelShader.Get(), nullptr, 0);
                ID3D11ShaderResourceView* view = videoView.Get(); ID3D11SamplerState* smp = sampler.Get();
                context->PSSetShaderResources(0, 1, &view); context->PSSetSamplers(0, 1, &smp); context->Draw(4, 0);
                ID3D11ShaderResourceView* none = nullptr; context->PSSetShaderResources(0, 1, &none);
            }
            const auto presentStart = Clock::now();
            // Waiting for VSync inside Windows' modal drag loop stalls mouse
            // tracking. Keep the user's VSync preference for normal playback.
            swapChain->Present(interactiveMoveResize ? 0u : (vsyncEnabled ? 1u : 0u), 0);
            presentMs = std::chrono::duration<double, std::milli>(Clock::now() - presentStart).count();
        }
        if (frame) {
            if (interactiveMoveResize) ++liveMoveFrames;
            ++presentedFrames;
            const auto now = Clock::now();
            const auto elapsed = std::chrono::duration<double>(now - fpsWindow).count();
            if (elapsed >= 1.0) { renderFps = presentedFrames / elapsed; presentedFrames = 0; fpsWindow = now; }
            double measuredCaptureFps, measuredCaptureP95;
            { std::lock_guard lock(captureMetricsMutex); measuredCaptureFps = captureFps; measuredCaptureP95 = captureP95Ms; }
            diagnostic = L"capture " + std::to_wstring(static_cast<int>(std::lround(measuredCaptureFps))) + L" fps; render " +
                std::to_wstring(static_cast<int>(std::lround(renderFps))) + L" fps; replaced " + std::to_wstring(pending.replaced()) +
                L"; GPU YUY2 " + (gpuYuy2Active ? L"active" : L"inactive") + L"; app age " + std::to_wstring(static_cast<int>(submitMs)) + L"ms; capture copy/convert " +
                std::to_wstring(static_cast<int>(captureCopyMs)) + L"ms; Present CPU " + std::to_wstring(static_cast<int>(presentMs)) + L"ms; capture p95 " +
                std::to_wstring(static_cast<int>(measuredCaptureP95)) + L"ms; vsync " +
                (vsyncEnabled ? L"on" : L"off") + L"; resize " +
                std::to_wstring(resizeCount) + L"/fail " + std::to_wstring(resizeFailures) +
                L"; last resize hr " + std::to_wstring(static_cast<unsigned long>(lastResizeError)) +
                L"; live frames " + std::to_wstring(liveMoveFrames) +
                L" (app metrics; NOT input-to-photon)";
            update_title();
        }
    }

    void paint() { render(false); }

    bool video_dimensions(int& width, int& height) const {
        width = static_cast<int>(displayWidth);
        height = static_cast<int>(textureHeight);
        if ((width <= 0 || height <= 0) && selectedDevice < devices.size() &&
            selectedMode < devices[selectedDevice].modes.size()) {
            const auto& mode = devices[selectedDevice].modes[selectedMode];
            width = static_cast<int>(mode.width);
            height = static_cast<int>(mode.height);
        }
        return width > 0 && height > 0;
    }

    void place_window_size_preserving_corner(int newOuterW, int newOuterH, const RECT& oldOuter) {
        MONITORINFO mi{sizeof(mi)};
        if (!GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &mi)) return;
        const RECT work = mi.rcWork;
        constexpr int edgeSnapTolerance = 48;
        auto near_edge = [&](int a, int b) { return std::abs(a - b) <= edgeSnapTolerance; };

        int x = oldOuter.left;
        int y = oldOuter.top;
        if (near_edge(oldOuter.right, work.right)) x = work.right - newOuterW;
        else if (near_edge(oldOuter.left, work.left)) x = work.left;
        if (near_edge(oldOuter.bottom, work.bottom)) y = work.bottom - newOuterH;
        else if (near_edge(oldOuter.top, work.top)) y = work.top;

        x = std::clamp(x, static_cast<int>(work.left),
            std::max(static_cast<int>(work.left), static_cast<int>(work.right) - newOuterW));
        y = std::clamp(y, static_cast<int>(work.top),
            std::max(static_cast<int>(work.top), static_cast<int>(work.bottom) - newOuterH));
        SetWindowPos(window, nullptr, x, y, newOuterW, newOuterH,
            SWP_NOZORDER | SWP_NOACTIVATE);
    }

    void set_video_pixels_100_percent() {
        if (!window) return;
        int videoW{}, videoH{};
        if (!video_dimensions(videoW, videoH)) return;
        if (IsZoomed(window)) ShowWindow(window, SW_RESTORE);

        RECT outer{};
        if (!GetWindowRect(window, &outer)) return;

        const DWORD style = static_cast<DWORD>(GetWindowLongPtrW(window, GWL_STYLE));
        const DWORD exStyle = static_cast<DWORD>(GetWindowLongPtrW(window, GWL_EXSTYLE));
        const UINT dpi = GetDpiForWindow(window);
        RECT wanted{0, 0, videoW, videoH};
        if (!AdjustWindowRectExForDpi(&wanted, style, GetMenu(window) != nullptr, exStyle, dpi)) return;

        const int outerW = wanted.right - wanted.left;
        const int outerH = wanted.bottom - wanted.top;
        place_window_size_preserving_corner(outerW, outerH, outer);

        // One bounded correction handles menu/nonclient rounding without cumulative growth.
        RECT client{}, correctedOuter{};
        if (GetClientRect(window, &client) && GetWindowRect(window, &correctedOuter)) {
            const int actualW = client.right - client.left;
            const int actualH = client.bottom - client.top;
            if (actualW != videoW || actualH != videoH) {
                const int correctedW = (correctedOuter.right - correctedOuter.left) + (videoW - actualW);
                const int correctedH = (correctedOuter.bottom - correctedOuter.top) + (videoH - actualH);
                place_window_size_preserving_corner(correctedW, correctedH, correctedOuter);
            }
        }
    }

    void fit_window_to_video_aspect() {
        if (!window) return;
        if (IsZoomed(window)) ShowWindow(window, SW_RESTORE);
        RECT client{}, outer{};
        if (!GetClientRect(window, &client) || !GetWindowRect(window, &outer)) return;
        int videoW{}, videoH{};
        if (!video_dimensions(videoW, videoH)) return;
        const int clientW = client.right - client.left;
        const int clientH = client.bottom - client.top;
        const auto target = hcv::fit_inside_aspect(clientW, clientH, videoW, videoH);
        if (target.width == clientW && target.height == clientH) return;
        const int newOuterW = (outer.right - outer.left) + (target.width - clientW);
        const int newOuterH = (outer.bottom - outer.top) + (target.height - clientH);
        place_window_size_preserving_corner(newOuterW, newOuterH, outer);
    }

    void rebuild_menus() {
        HMENU previous = GetMenu(window);
        if (borderless) {
            SetMenu(window, nullptr);
            if (previous) DestroyMenu(previous);
            return;
        }
        HMENU root = CreateMenu();
        HMENU deviceMenu = CreatePopupMenu(), formatMenu = CreatePopupMenu(), windowMenu = CreatePopupMenu();
        for (std::size_t i = 0; i < devices.size() && i < MAX_MENU_ITEMS; ++i)
            AppendMenuW(deviceMenu, MF_STRING | (i == selectedDevice ? MF_CHECKED : 0), DEVICE_COMMAND_BASE + static_cast<UINT>(i), devices[i].name.c_str());
        if (devices.empty()) AppendMenuW(deviceMenu, MF_GRAYED, 0, L"No UVC capture device found");
        if (!devices.empty() && selectedDevice < devices.size()) {
            const auto& d = devices[selectedDevice];
            for (std::size_t i = 0; i < d.modes.size() && i < MAX_MENU_ITEMS; ++i) {
                const auto label = mode_label(d.modes[i]);
                AppendMenuW(formatMenu, MF_STRING | (i == selectedMode ? MF_CHECKED : 0), MODE_COMMAND_BASE + static_cast<UINT>(i), label.c_str());
            }
        }
        AppendMenuW(windowMenu, MF_STRING | (borderless ? MF_CHECKED : 0), 3001, L"Borderless window");
        AppendMenuW(windowMenu, MF_STRING, 3002, L"Rescan devices");
        AppendMenuW(windowMenu, MF_STRING | (vsyncEnabled ? MF_CHECKED : 0), 3003, L"VSync (off may tear)");
        AppendMenuW(windowMenu, MF_STRING, 3004, L"Show metrics (F2)");
        AppendMenuW(windowMenu, MF_STRING, 3005, L"Video size 100% / source pixels (F9)");
        AppendMenuW(windowMenu, MF_STRING, 3006, L"Fit video aspect inside current window");
        AppendMenuW(root, MF_POPUP, reinterpret_cast<UINT_PTR>(deviceMenu), L"Device");
        AppendMenuW(root, MF_POPUP, reinterpret_cast<UINT_PTR>(formatMenu), L"Native format");
        AppendMenuW(root, MF_POPUP, reinterpret_cast<UINT_PTR>(windowMenu), L"Window");
        SetMenu(window, root);
        if (previous) DestroyMenu(previous);
        DrawMenuBar(window);
    }

    void toggle_borderless() {
        if (!borderless) {
            HMENU menu = GetMenu(window);
            SetMenu(window, nullptr);
            if (menu) DestroyMenu(menu);
            savedStyle = static_cast<DWORD>(GetWindowLongPtrW(window, GWL_STYLE));
            savedPlacement.length = sizeof(savedPlacement); GetWindowPlacement(window, &savedPlacement);
            GetClientRect(window, &savedClientRect);
            MapWindowPoints(window, HWND_DESKTOP, reinterpret_cast<POINT*>(&savedClientRect), 2);
            SetWindowLongPtrW(window, GWL_STYLE, WS_POPUP | WS_VISIBLE);
            SetWindowPos(window, HWND_TOP, savedClientRect.left, savedClientRect.top,
                savedClientRect.right - savedClientRect.left, savedClientRect.bottom - savedClientRect.top, SWP_FRAMECHANGED);
            borderless = true;
        } else {
            SetWindowLongPtrW(window, GWL_STYLE, savedStyle);
            SetWindowPlacement(window, &savedPlacement);
            SetWindowPos(window, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
            borderless = false;
        }
        rebuild_menus();
    }
};

App* app = nullptr;

LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (!app) return DefWindowProcW(hwnd, message, wParam, lParam);
    switch (message) {
    case WM_ERASEBKGND: return 1; // D3D paints the whole client: prevent white/gray flashes.
    case WM_ENTERSIZEMOVE:
        app->interactiveMoveResize = true;
        app->lastInteractivePaint = {};
        SetTimer(hwnd, LIVE_MOVE_TIMER, LIVE_MOVE_TIMER_PERIOD_MS, nullptr);
        return 0;
    case WM_EXITSIZEMOVE:
        KillTimer(hwnd, LIVE_MOVE_TIMER);
        app->interactiveMoveResize = false;
        app->render(true); // Apply the exact final size immediately.
        app->update_title(true);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_MOVING:
    case WM_SIZING:
        app->interactive_tick(); // Synchronous progress even if posted frame messages stall.
        return DefWindowProcW(hwnd, message, wParam, lParam);
    case WM_TIMER:
        if (wParam == LIVE_MOVE_TIMER) {
            app->interactive_tick();
            return 0;
        }
        break;
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        BeginPaint(hwnd, &ps);
        if (app->interactiveMoveResize) app->interactive_tick();
        else app->paint();
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED && LOWORD(lParam) && HIWORD(lParam)) {
            app->resizeGate.request(LOWORD(lParam), HIWORD(lParam));
            if (!app->interactiveMoveResize) InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    case WM_NEW_FRAME:
        app->frameWakeQueued.store(false, std::memory_order_release);
        if (app->interactiveMoveResize) app->interactive_tick();
        else app->render(true);
        return 0;
    case WM_KEYDOWN:
        if (wParam == VK_F11 && app->borderless) { app->toggle_borderless(); return 0; }
        if (wParam == VK_F9) { app->set_video_pixels_100_percent(); return 0; }
        if (wParam == VK_F2) {
            std::wstring snapshot = app->captureStatus + L"\r\n\r\n" + app->diagnostic;
            MessageBoxW(hwnd, snapshot.c_str(), L"HDMI Capture Viewer - captured metrics", MB_OK | MB_ICONINFORMATION);
            return 0;
        }
        return 0;
    case WM_CAPTURE_STATUS: {
        std::unique_ptr<std::wstring> value(reinterpret_cast<std::wstring*>(lParam));
        app->set_status(std::move(*value)); return 0;
    }
    case WM_COMMAND: {
        const UINT id = LOWORD(wParam);
        if (id >= DEVICE_COMMAND_BASE && id < DEVICE_COMMAND_BASE + MAX_MENU_ITEMS) {
            app->selectedDevice = id - DEVICE_COMMAND_BASE;
            app->selectedMode = preferred_mode(app->devices[app->selectedDevice]); app->rebuild_menus(); app->start_capture(); return 0;
        }
        if (id >= MODE_COMMAND_BASE && id < MODE_COMMAND_BASE + MAX_MENU_ITEMS) {
            app->selectedMode = id - MODE_COMMAND_BASE; app->rebuild_menus(); app->start_capture(); return 0;
        }
        if (id == 3001) { app->toggle_borderless(); return 0; }
        if (id == 3003) {
            app->vsyncEnabled = !app->vsyncEnabled;
            app->rebuild_menus();
            // Recompute the diagnostic label on the next captured frame.
            app->lastTitleUpdate = {};
            return 0;
        }
        if (id == 3004) {
            std::wstring snapshot = app->captureStatus + L"\r\n\r\n" + app->diagnostic;
            MessageBoxW(hwnd, snapshot.c_str(), L"HDMI Capture Viewer - captured metrics", MB_OK | MB_ICONINFORMATION);
            return 0;
        }
        if (id == 3005) { app->set_video_pixels_100_percent(); return 0; }
        if (id == 3006) { app->fit_window_to_video_aspect(); return 0; }
        if (id == 3002) {
            app->stop_capture(); app->devices = enumerate_devices(); app->selectedDevice = 0;
            app->selectedMode = app->devices.empty() ? 0 : preferred_mode(app->devices[0]);
            app->rebuild_menus();
            if (app->devices.empty()) app->set_status(L"No capture devices found after rescan.");
            else if (app->devices.size() == 1) app->start_capture();
            else app->set_status(L"Multiple video devices found. Choose the HDMI capture device from the Device menu.");
            return 0;
        }
        break;
    }
    case WM_DESTROY: KillTimer(hwnd, LIVE_MOVE_TIMER); app->stop_capture(); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    // Window/client geometry is specified in physical pixels. This matters on
    // the owner's 4K desktop where Windows UI scaling is not 100%.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) return 1;
    if (FAILED(MFStartup(MF_VERSION, MFSTARTUP_FULL))) { CoUninitialize(); return 1; }
    App state; app = &state;
    state.devices = enumerate_devices();
    if (!state.devices.empty()) state.selectedMode = preferred_mode(state.devices[0]);
    WNDCLASSW wc{}; wc.hInstance = instance; wc.lpfnWndProc = window_proc; wc.lpszClassName = L"HcvPreviewWindow";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW); wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    RegisterClassW(&wc);
    const UINT initialDpi = GetDpiForSystem();
    RECT rect{0,0,1920,1080};
    AdjustWindowRectExForDpi(&rect, WS_OVERLAPPEDWINDOW, TRUE, 0, initialDpi);
    state.window = CreateWindowW(wc.lpszClassName, L"HDMI Capture Viewer", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr, instance, nullptr);
    if (!state.window) { MFShutdown(); CoUninitialize(); return 1; }
    state.rebuild_menus(); ShowWindow(state.window, show); UpdateWindow(state.window);
    if (state.devices.empty()) state.set_status(L"No supported UVC capture device found. Connect one, then use Window > Rescan devices.");
    else if (state.devices.size() == 1) state.start_capture();
    else state.set_status(L"Multiple video devices found. Choose the HDMI capture device from the Device menu.");
    MSG msg{}; while (GetMessageW(&msg, nullptr, 0, 0) > 0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    state.stop_capture(); app = nullptr; MFShutdown(); CoUninitialize(); return static_cast<int>(msg.wParam);
}
