#define NOMINMAX
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#include "latest_frame.hpp"

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

std::uint8_t clamp_byte(int value) { return static_cast<std::uint8_t>(std::clamp(value, 0, 255)); }

void yuy2_to_bgra(const BYTE* source, UINT32 width, UINT32 height, std::vector<std::uint8_t>& dest) {
    dest.resize(static_cast<std::size_t>(width) * height * 4);
    for (UINT32 y = 0; y < height; ++y) {
        const BYTE* row = source + static_cast<std::size_t>(y) * width * 2;
        for (UINT32 x = 0; x < width; x += 2) {
            const int y0 = row[x * 2 + 0], u = row[x * 2 + 1] - 128;
            const int y1 = row[x * 2 + 2], v = row[x * 2 + 3] - 128;
            for (UINT32 pair = 0; pair < 2 && x + pair < width; ++pair) {
                const int c = std::max(0, static_cast<int>(pair ? y1 : y0) - 16);
                const auto offset = (static_cast<std::size_t>(y) * width + x + pair) * 4;
                dest[offset + 0] = clamp_byte((298 * c + 516 * u + 128) >> 8);
                dest[offset + 1] = clamp_byte((298 * c - 100 * u - 208 * v + 128) >> 8);
                dest[offset + 2] = clamp_byte((298 * c + 409 * v + 128) >> 8);
                dest[offset + 3] = 255;
            }
        }
    }
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
    double conversionMs{};
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

    void set_status(std::wstring value) {
        captureStatus = std::move(value);
        update_title(true);
    }
    void update_title(bool force = false) {
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
            if (SUCCEEDED(hr)) hr = MFCreateSourceReaderFromMediaSource(source.Get(), attrs.Get(), &reader);
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
            if (SUCCEEDED(reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &negotiated)))
                negotiated->GetGUID(MF_MT_SUBTYPE, &negotiatedSubtype);
            auto* message = new std::wstring(L"Capturing " + device.name + L"; selected advertised " + subtype_name(mode.subtype) +
                L" " + std::to_wstring(mode.width) + L"x" + std::to_wstring(mode.height) + L" @ " +
                std::to_wstring(static_cast<int>(std::lround(fps(mode)))) + L"; Source Reader output " + subtype_name(negotiatedSubtype) +
                (directYuy2 ? L" (converters disabled)" : L" (decoded/converted; native input subtype not confirmed"));
            if (!directYuy2) *message += L")";
            PostMessageW(window, WM_CAPTURE_STATUS, 0, reinterpret_cast<LPARAM>(message));
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
                const std::size_t bytesPerPixel = directYuy2 ? 2 : 4;
                const std::size_t rowBytes = static_cast<std::size_t>(mode.width) * bytesPerPixel;
                const std::size_t frameBytes = rowBytes * mode.height;
                if (length >= frameBytes) {
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
                    if (directYuy2) yuy2_to_bgra(data, mode.width, mode.height, frame.bgra);
                    else frame.bgra.assign(data, data + frameBytes);
                    frame.conversionMs = std::chrono::duration<double, std::milli>(Clock::now() - conversionStart).count();
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
        const char* shader = "Texture2D videoTexture : register(t0); SamplerState videoSampler : register(s0);"
            "struct V { float2 p : POSITION; float2 uv : TEXCOORD; }; struct P { float4 p : SV_POSITION; float2 uv : TEXCOORD; };"
            "P VS(V i) { P o; o.p=float4(i.p,0,1); o.uv=i.uv; return o; }"
            "float4 PS(P i) : SV_TARGET { return videoTexture.Sample(videoSampler,i.uv); }";
        ComPtr<ID3DBlob> vs, ps, errors;
        hr = D3DCompile(shader, strlen(shader), nullptr, nullptr, nullptr, "VS", "vs_4_0", 0, 0, &vs, &errors);
        if (FAILED(hr)) return false;
        hr = D3DCompile(shader, strlen(shader), nullptr, nullptr, nullptr, "PS", "ps_4_0", 0, 0, &ps, &errors);
        if (FAILED(hr)) return false;
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

    void render(bool consumeFrame) {
        if (!d3d && !init_d3d()) { set_status(L"Direct3D 11 initialization failed."); return; }
        auto frame = consumeFrame ? pending.take() : std::optional<hcv::Frame>{};
        if (frame) {
            if (textureWidth != static_cast<UINT>(frame->width) || textureHeight != static_cast<UINT>(frame->height)) {
                videoView.Reset(); videoTexture.Reset();
                D3D11_TEXTURE2D_DESC td{}; td.Width = frame->width; td.Height = frame->height; td.MipLevels = td.ArraySize = 1;
                td.Format = DXGI_FORMAT_B8G8R8A8_UNORM; td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                if (FAILED(d3d->CreateTexture2D(&td, nullptr, &videoTexture)) || FAILED(d3d->CreateShaderResourceView(videoTexture.Get(), nullptr, &videoView))) return;
                textureWidth = frame->width; textureHeight = frame->height;
            }
            context->UpdateSubresource(videoTexture.Get(), 0, nullptr, frame->bgra.data(), frame->width * 4, 0);
            submitMs = std::chrono::duration<double, std::milli>(Clock::now() - frame->capturedAt).count();
            conversionMs = frame->conversionMs;
        }
        RECT rect{}; GetClientRect(window, &rect);
        const float width = static_cast<float>(rect.right), height = static_cast<float>(rect.bottom);
        D3D11_VIEWPORT viewport{}; viewport.Width = width; viewport.Height = height; viewport.MaxDepth = 1;
        context->RSSetViewports(1, &viewport);
        if (!backBufferView) {
            ComPtr<ID3D11Texture2D> backBuffer;
            if (SUCCEEDED(swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer)))) d3d->CreateRenderTargetView(backBuffer.Get(), nullptr, &backBufferView);
        }
        if (backBufferView) {
            const float black[] = {0,0,0,1}; ID3D11RenderTargetView* target = backBufferView.Get(); context->OMSetRenderTargets(1, &target, nullptr); context->ClearRenderTargetView(backBufferView.Get(), black);
            if (videoView && textureWidth && textureHeight && width > 0 && height > 0) {
                const float scale = std::min(width / textureWidth, height / textureHeight);
                D3D11_VIEWPORT image{}; image.Width = textureWidth * scale; image.Height = textureHeight * scale;
                image.TopLeftX = (width - image.Width) * 0.5f; image.TopLeftY = (height - image.Height) * 0.5f; image.MaxDepth = 1;
                context->RSSetViewports(1, &image);
                UINT stride = sizeof(Vertex), offset = 0; ID3D11Buffer* vb = vertexBuffer.Get();
                context->IASetInputLayout(inputLayout.Get()); context->IASetVertexBuffers(0, 1, &vb, &stride, &offset); context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
                context->VSSetShader(vertexShader.Get(), nullptr, 0); context->PSSetShader(pixelShader.Get(), nullptr, 0);
                ID3D11ShaderResourceView* view = videoView.Get(); ID3D11SamplerState* smp = sampler.Get();
                context->PSSetShaderResources(0, 1, &view); context->PSSetSamplers(0, 1, &smp); context->Draw(4, 0);
                ID3D11ShaderResourceView* none = nullptr; context->PSSetShaderResources(0, 1, &none);
            }
            const auto presentStart = Clock::now();
            swapChain->Present(vsyncEnabled ? 1u : 0u, 0);
            presentMs = std::chrono::duration<double, std::milli>(Clock::now() - presentStart).count();
        }
        if (frame) {
            ++presentedFrames;
            const auto now = Clock::now();
            const auto elapsed = std::chrono::duration<double>(now - fpsWindow).count();
            if (elapsed >= 1.0) { renderFps = presentedFrames / elapsed; presentedFrames = 0; fpsWindow = now; }
            double measuredCaptureFps, measuredCaptureP95;
            { std::lock_guard lock(captureMetricsMutex); measuredCaptureFps = captureFps; measuredCaptureP95 = captureP95Ms; }
            diagnostic = L"capture " + std::to_wstring(static_cast<int>(std::lround(measuredCaptureFps))) + L" fps; render " +
                std::to_wstring(static_cast<int>(std::lround(renderFps))) + L" fps; replaced " + std::to_wstring(pending.replaced()) +
                L"; app age " + std::to_wstring(static_cast<int>(submitMs)) + L"ms; convert " +
                std::to_wstring(static_cast<int>(conversionMs)) + L"ms; Present CPU " + std::to_wstring(static_cast<int>(presentMs)) + L"ms; capture p95 " +
                std::to_wstring(static_cast<int>(measuredCaptureP95)) + L"ms; vsync " +
                (vsyncEnabled ? L"on" : L"off") + L" (app metrics; NOT input-to-photon)";
            update_title();
        }
    }

    void paint() { render(false); }

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
    case WM_PAINT: { PAINTSTRUCT ps{}; BeginPaint(hwnd, &ps); app->paint(); EndPaint(hwnd, &ps); return 0; }
    case WM_SIZE: if (app->swapChain && wParam != SIZE_MINIMIZED && LOWORD(lParam) && HIWORD(lParam)) {
        app->context->OMSetRenderTargets(0, nullptr, nullptr);
        app->backBufferView.Reset();
        app->swapChain->ResizeBuffers(0, LOWORD(lParam), HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
    } InvalidateRect(hwnd, nullptr, FALSE); return 0;
    case WM_NEW_FRAME:
        app->frameWakeQueued.store(false, std::memory_order_release);
        app->render(true);
        return 0;
    case WM_KEYDOWN:
        if (wParam == VK_F11 && app->borderless) { app->toggle_borderless(); return 0; }
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
    case WM_DESTROY: app->stop_capture(); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) return 1;
    if (FAILED(MFStartup(MF_VERSION, MFSTARTUP_FULL))) { CoUninitialize(); return 1; }
    App state; app = &state;
    state.devices = enumerate_devices();
    if (!state.devices.empty()) state.selectedMode = preferred_mode(state.devices[0]);
    WNDCLASSW wc{}; wc.hInstance = instance; wc.lpfnWndProc = window_proc; wc.lpszClassName = L"HcvPreviewWindow";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW); wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    RegisterClassW(&wc);
    RECT rect{0,0,1920,1080}; AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, TRUE);
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
