// HCV-005 control A: pure Windows-native caption / Snap / focus / menu.
// No video capture, GPU, custom child HWND, D3D, or global hooks.
#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <string>
#include <cstdint>

namespace {
HWND window{};
UINT_PTR titleTimer = 1;
std::uint64_t activates{}, leftDown{}, rightUp{}, moveStarts{}, moveEnds{}, popupRequests{}, focusLost{};
bool moving{};
void title() {
    std::wstring s = L"HCV005 native shell | activates " + std::to_wstring(activates)
      + L"; left down " + std::to_wstring(leftDown)
      + L"; right up " + std::to_wstring(rightUp)
      + L"; start move " + std::to_wstring(moveStarts)
      + L"; end move " + std::to_wstring(moveEnds)
      + L"; focus lost " + std::to_wstring(focusLost)
      + L"; popup " + std::to_wstring(popupRequests);
    SetWindowTextW(window, s.c_str());
}
LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch(msg) {
    case WM_CREATE:
        window = hwnd;
        SetTimer(hwnd, titleTimer, 250, nullptr);
        return 0;
    case WM_TIMER:
        if (wp == titleTimer && !moving) title();
        return 0;
    case WM_MOUSEACTIVATE:
        ++activates;
        return DefWindowProcW(hwnd, msg, wp, lp);
    case WM_KILLFOCUS:
        ++focusLost;
        return DefWindowProcW(hwnd, msg, wp, lp);
    case WM_LBUTTONDOWN:
        ++leftDown;
        InvalidateRect(hwnd, nullptr, TRUE);
        return 0;
    case WM_RBUTTONUP: {
        ++rightUp;
        ++popupRequests;
        HMENU m = CreatePopupMenu();
        if (m) {
            AppendMenuW(m, MF_STRING, 100, L"Test popup (no action)");
            POINT p{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            ClientToScreen(hwnd, &p);
            TrackPopupMenuEx(m, TPM_RIGHTBUTTON, p.x, p.y, hwnd, nullptr);
            DestroyMenu(m);
        }
        return 0;
    }
    case WM_ENTERSIZEMOVE:
        moving = true; ++moveStarts; return 0;
    case WM_EXITSIZEMOVE:
        moving = false; ++moveEnds; title(); return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd, &ps);
        RECT r{}; GetClientRect(hwnd, &r);
        FillRect(dc, &r, reinterpret_cast<HBRUSH>(COLOR_WINDOW+1));
        SetBkMode(dc, TRANSPARENT);
        DrawTextW(dc, L"Native Win32 control A (no video).\nSnap to corners. Unsnap. Switch between apps.\nTest first left click and right-click popup.\nCompare against capture viewer.", -1, &r, DT_CENTER|DT_VCENTER|DT_WORDBREAK);
        EndPaint(hwnd, &ps); return 0;
    }
    case WM_DESTROY:
        KillTimer(hwnd, titleTimer);
        PostQuitMessage(0); return 0;
    default: return DefWindowProcW(hwnd, msg, wp, lp);
    }
}
}
int WINAPI wWinMain(HINSTANCE h, HINSTANCE, PWSTR, int show) {
    WNDCLASSW c{};
    c.hInstance=h; c.lpfnWndProc=proc; c.lpszClassName=L"HCV005WindowsShellProbe";
    c.hCursor=LoadCursorW(nullptr, IDC_ARROW);
    if (!RegisterClassW(&c)) return 2;
    HWND hwnd = CreateWindowExW(0,c.lpszClassName,L"HCV005 native shell", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,CW_USEDEFAULT,1050,660,nullptr,nullptr,h,nullptr);
    if (!hwnd) return 3;
    ShowWindow(hwnd,show); UpdateWindow(hwnd);
    MSG m{};
    while (GetMessageW(&m,nullptr,0,0)>0) {TranslateMessage(&m);DispatchMessageW(&m);}
    return static_cast<int>(m.wParam);
}
