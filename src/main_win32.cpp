#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <random>
#include <string>
#include <vector>

struct Ornament {
    POINT pos{};
    int radius = 6;
    COLORREF colorA = RGB(255, 0, 0);
    COLORREF colorB = RGB(255, 255, 0);
    bool on = true;
};

struct Snowflake {
    float x = 0.0f;
    float y = 0.0f;
    float speed = 0.8f;
    float drift = 0.0f;
    int radius = 2;
};

struct AppState {
    int width = 800;
    int height = 600;
    int blinkPhase = 0;
    std::vector<Ornament> ornaments;
    std::vector<Snowflake> snowflakes;
    std::mt19937 rng{std::random_device{}()};
};

static AppState g_state{};

static constexpr COLORREF kTransparentKey = RGB(255, 0, 255);
static bool g_clickThrough = false;

static void ApplyOverlayStyles(HWND hwnd) {
    LONG_PTR ex = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    ex |= WS_EX_LAYERED | WS_EX_TOPMOST;
    if (g_clickThrough) {
        ex |= WS_EX_TRANSPARENT;
    } else {
        ex &= ~WS_EX_TRANSPARENT;
    }
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, ex);
    SetLayeredWindowAttributes(hwnd, kTransparentKey, 0, LWA_COLORKEY);
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

static int ClampInt(int v, int lo, int hi) {
    return std::max(lo, std::min(hi, v));
}

static COLORREF AdjustColor(COLORREF c, int delta) {
    int r = ClampInt(GetRValue(c) + delta, 0, 255);
    int g = ClampInt(GetGValue(c) + delta, 0, 255);
    int b = ClampInt(GetBValue(c) + delta, 0, 255);
    return RGB(r, g, b);
}

static TRIVERTEX MakeVertex(int x, int y, COLORREF c) {
    TRIVERTEX v{};
    v.x = x;
    v.y = y;
    v.Red = static_cast<COLOR16>(GetRValue(c) << 8);
    v.Green = static_cast<COLOR16>(GetGValue(c) << 8);
    v.Blue = static_cast<COLOR16>(GetBValue(c) << 8);
    v.Alpha = 0x0000;
    return v;
}

static void FillTriangleGradient(HDC hdc, const POINT (&tri)[3], COLORREF top, COLORREF bottom) {
    TRIVERTEX verts[3] = {
        MakeVertex(tri[0].x, tri[0].y, top),
        MakeVertex(tri[1].x, tri[1].y, bottom),
        MakeVertex(tri[2].x, tri[2].y, bottom),
    };
    GRADIENT_TRIANGLE gt{0, 1, 2};
    GradientFill(hdc, verts, 3, &gt, 1, GRADIENT_FILL_TRIANGLE);
}

static float RandFloat(std::mt19937& rng, float lo, float hi) {
    std::uniform_real_distribution<float> dist(lo, hi);
    return dist(rng);
}

static int RandInt(std::mt19937& rng, int lo, int hi) {
    std::uniform_int_distribution<int> dist(lo, hi);
    return dist(rng);
}

static void RegenerateScene(int w, int h) {
    g_state.width = std::max(200, w);
    g_state.height = std::max(200, h);

    const int width = g_state.width;
    const int height = g_state.height;

    const int treeTopY = static_cast<int>(height * 0.10);
    const int treeBaseY = static_cast<int>(height * 0.80);
    const int treeCenterX = width / 2;
    const int treeHalfWidthBase = static_cast<int>(width * 0.25);

    const int ornamentCount = ClampInt((width * height) / 25000, 35, 140);
    g_state.ornaments.clear();
    g_state.ornaments.reserve(ornamentCount);

    std::array<COLORREF, 6> palette = {
        RGB(255, 60, 60),   // red
        RGB(60, 220, 80),   // green
        RGB(255, 210, 60),  // gold
        RGB(80, 160, 255),  // blue
        RGB(255, 120, 240), // pink
        RGB(255, 255, 255)  // white
    };

    for (int i = 0; i < ornamentCount; ++i) {
        float t = RandFloat(g_state.rng, 0.05f, 1.0f);
        int y = treeTopY + static_cast<int>(t * (treeBaseY - treeTopY));
        float halfW = t * treeHalfWidthBase;
        int x = treeCenterX + static_cast<int>(RandFloat(g_state.rng, -halfW, halfW));

        Ornament o;
        o.pos = {x, y};
        o.radius = RandInt(g_state.rng, 4, 9);
        int idxA = RandInt(g_state.rng, 0, static_cast<int>(palette.size() - 1));
        int idxB = RandInt(g_state.rng, 0, static_cast<int>(palette.size() - 1));
        o.colorA = palette[idxA];
        o.colorB = palette[idxB];
        o.on = RandInt(g_state.rng, 0, 1) == 1;
        g_state.ornaments.push_back(o);
    }

    const int snowCount = ClampInt(width / 8, 60, 220);
    g_state.snowflakes.clear();
    g_state.snowflakes.reserve(snowCount);
    for (int i = 0; i < snowCount; ++i) {
        Snowflake s;
        s.x = RandFloat(g_state.rng, 0.0f, static_cast<float>(width));
        s.y = RandFloat(g_state.rng, 0.0f, static_cast<float>(height));
        s.speed = RandFloat(g_state.rng, 0.5f, 1.8f);
        s.drift = RandFloat(g_state.rng, -0.3f, 0.3f);
        s.radius = RandInt(g_state.rng, 1, 3);
        g_state.snowflakes.push_back(s);
    }
}

static void DrawStar(HDC hdc, int cx, int cy, int rOuter, int rInner) {
    constexpr double pi = 3.14159265358979323846;
    std::array<POINT, 10> pts{};
    for (int i = 0; i < 10; ++i) {
        double angleDeg = i * 36.0 - 90.0;
        double angle = angleDeg * pi / 180.0;
        int r = (i % 2 == 0) ? rOuter : rInner;
        pts[i].x = cx + static_cast<int>(std::cos(angle) * r);
        pts[i].y = cy + static_cast<int>(std::sin(angle) * r);
    }
    Polygon(hdc, pts.data(), static_cast<int>(pts.size()));
}

static void DrawTree(HDC hdc) {
    const int w = g_state.width;
    const int h = g_state.height;
    const int cx = w / 2;

    const int topY = static_cast<int>(h * 0.10);
    const int baseY = static_cast<int>(h * 0.80);
    const int tierHeight = static_cast<int>((baseY - topY) / 3.0);

    COLORREF baseGreen = RGB(8, 120, 45);
    HPEN treePen = CreatePen(PS_SOLID, 2, RGB(5, 80, 30));
    HGDIOBJ oldPen = SelectObject(hdc, treePen);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));

    for (int tier = 0; tier < 3; ++tier) {
        int y0 = topY + tier * tierHeight;
        int y1 = y0 + tierHeight + static_cast<int>(tierHeight * 0.4);
        int halfW0 = static_cast<int>(w * (0.10 + tier * 0.08));
        int halfW1 = static_cast<int>(w * (0.22 + tier * 0.12));

        POINT tri[3] = {
            {cx, y0},
            {cx - halfW1, y1},
            {cx + halfW1, y1},
        };
        COLORREF topC = AdjustColor(baseGreen, 35 - tier * 8);
        COLORREF bottomC = AdjustColor(baseGreen, -10 - tier * 6);
        FillTriangleGradient(hdc, tri, topC, bottomC);
        Polygon(hdc, tri, 3);

        // subtle highlight
        HPEN highlightPen = CreatePen(PS_SOLID, 1, AdjustColor(baseGreen, 70));
        SelectObject(hdc, highlightPen);
        MoveToEx(hdc, cx, y0, nullptr);
        LineTo(hdc, cx - halfW1, y1);
        MoveToEx(hdc, cx, y0, nullptr);
        LineTo(hdc, cx + halfW1, y1);
        SelectObject(hdc, treePen);
        DeleteObject(highlightPen);

        // garland with beads
        int garlandY = y0 + static_cast<int>((y1 - y0) * 0.60);
        float t = static_cast<float>(garlandY - y0) / static_cast<float>(y1 - y0);
        int garlandHalfW = static_cast<int>(t * halfW1);
        constexpr int segments = 24;
        std::array<POINT, segments + 1> garlandPts{};
        float phase = static_cast<float>(g_state.blinkPhase) * 0.10f + tier * 0.8f;
        for (int i = 0; i <= segments; ++i) {
            float u = static_cast<float>(i) / segments;
            int x = cx - garlandHalfW + static_cast<int>(u * garlandHalfW * 2);
            float wave = std::sin(u * 3.1415926f * 2.0f + phase) * (tierHeight * 0.07f);
            garlandPts[i] = {x, garlandY + static_cast<int>(wave)};
        }
        HPEN garlandPen = CreatePen(PS_SOLID, 2, RGB(255, 210, 80));
        HGDIOBJ oldGarPen = SelectObject(hdc, garlandPen);
        Polyline(hdc, garlandPts.data(), static_cast<int>(garlandPts.size()));
        SelectObject(hdc, oldGarPen);
        DeleteObject(garlandPen);

        for (int i = 0; i <= segments; i += 3) {
            POINT p = garlandPts[i];
            int r = 3 + (i % 2);
            bool on = ((g_state.blinkPhase / 6 + i) % 2) == 0;
            COLORREF bead = on ? RGB(255, 80, 80) : RGB(240, 240, 255);
            HBRUSH beadBrush = CreateSolidBrush(bead);
            HGDIOBJ oldB = SelectObject(hdc, beadBrush);
            HGDIOBJ oldBeadPen = SelectObject(hdc, GetStockObject(NULL_PEN));
            Ellipse(hdc, p.x - r, p.y - r, p.x + r, p.y + r);
            SelectObject(hdc, oldBeadPen);
            SelectObject(hdc, oldB);
            DeleteObject(beadBrush);
        }
    }

    // trunk
    HBRUSH trunkBrush = CreateSolidBrush(RGB(120, 65, 28));
    SelectObject(hdc, trunkBrush);
    int trunkW = static_cast<int>(w * 0.06);
    int trunkH = static_cast<int>(h * 0.10);
    Rectangle(hdc, cx - trunkW / 2, baseY, cx + trunkW / 2, baseY + trunkH);
    DeleteObject(trunkBrush);

    // star
    int starY = topY - static_cast<int>(h * 0.02);
    int outer = static_cast<int>(w * 0.035);
    int inner = static_cast<int>(w * 0.017);
    HBRUSH glowBrush = CreateSolidBrush(AdjustColor(RGB(255, 220, 70), 25));
    HPEN glowPen = CreatePen(PS_SOLID, 1, AdjustColor(RGB(255, 200, 20), 40));
    SelectObject(hdc, glowBrush);
    SelectObject(hdc, glowPen);
    DrawStar(hdc, cx, starY, outer + 4, inner + 2);
    DeleteObject(glowBrush);
    DeleteObject(glowPen);

    HBRUSH starBrush = CreateSolidBrush(RGB(255, 215, 60));
    HPEN starPen = CreatePen(PS_SOLID, 2, RGB(255, 190, 10));
    SelectObject(hdc, starBrush);
    SelectObject(hdc, starPen);
    DrawStar(hdc, cx, starY, outer, inner);
    DeleteObject(starBrush);
    DeleteObject(starPen);

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(treePen);
}

static void DrawOrnaments(HDC hdc) {
    for (const auto& o : g_state.ornaments) {
        COLORREF c = o.on ? o.colorA : o.colorB;
        int glowR = o.radius + (o.on ? 3 : 1);
        HBRUSH glow = CreateSolidBrush(AdjustColor(c, 40));
        HGDIOBJ oldB = SelectObject(hdc, glow);
        HGDIOBJ oldPGlow = SelectObject(hdc, GetStockObject(NULL_PEN));
        Ellipse(hdc, o.pos.x - glowR, o.pos.y - glowR, o.pos.x + glowR, o.pos.y + glowR);
        SelectObject(hdc, oldPGlow);
        SelectObject(hdc, oldB);
        DeleteObject(glow);

        HBRUSH b = CreateSolidBrush(c);
        HPEN p = CreatePen(PS_SOLID, 1, RGB(25, 25, 25));
        oldB = SelectObject(hdc, b);
        HGDIOBJ oldP = SelectObject(hdc, p);

        Ellipse(hdc, o.pos.x - o.radius, o.pos.y - o.radius, o.pos.x + o.radius, o.pos.y + o.radius);

        // tiny shine
        if (o.radius >= 5) {
            int innerR = o.radius - 2;
            HBRUSH inner = CreateSolidBrush(AdjustColor(c, 25));
            HGDIOBJ oldInner = SelectObject(hdc, inner);
            Ellipse(hdc, o.pos.x - innerR, o.pos.y - innerR, o.pos.x + innerR, o.pos.y + innerR);
            SelectObject(hdc, oldInner);
            DeleteObject(inner);
        }
        SetPixel(hdc, o.pos.x - o.radius / 3, o.pos.y - o.radius / 3, RGB(255, 255, 255));

        SelectObject(hdc, oldB);
        SelectObject(hdc, oldP);
        DeleteObject(b);
        DeleteObject(p);
    }
}

static void DrawSnow(HDC hdc) {
    HBRUSH whiteBrush = CreateSolidBrush(RGB(255, 255, 255));
    HBRUSH blueBrush = CreateSolidBrush(RGB(230, 240, 255));
    HGDIOBJ old = SelectObject(hdc, whiteBrush);
    for (const auto& s : g_state.snowflakes) {
        int x = static_cast<int>(s.x);
        int y = static_cast<int>(s.y);
        HBRUSH use = (s.radius >= 3) ? blueBrush : whiteBrush;
        SelectObject(hdc, use);
        Ellipse(hdc, x - s.radius, y - s.radius, x + s.radius, y + s.radius);
    }
    SelectObject(hdc, old);
    DeleteObject(whiteBrush);
    DeleteObject(blueBrush);
}

static void UpdateAnimation() {
    g_state.blinkPhase = (g_state.blinkPhase + 1) % 60;
    if (g_state.blinkPhase % 10 == 0) {
        for (auto& o : g_state.ornaments) {
            if (RandInt(g_state.rng, 0, 2) == 0) {
                o.on = !o.on;
            }
        }
    }

    for (auto& s : g_state.snowflakes) {
        s.y += s.speed;
        s.x += s.drift;
        if (s.y > g_state.height + 10) {
            s.y = RandFloat(g_state.rng, -30.0f, -5.0f);
            s.x = RandFloat(g_state.rng, 0.0f, static_cast<float>(g_state.width));
            s.speed = RandFloat(g_state.rng, 0.5f, 1.8f);
            s.drift = RandFloat(g_state.rng, -0.3f, 0.3f);
            s.radius = RandInt(g_state.rng, 1, 3);
        }
        if (s.x < -10) s.x = static_cast<float>(g_state.width + 5);
        if (s.x > g_state.width + 10) s.x = -5.0f;
    }
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        RECT rc{};
        GetClientRect(hwnd, &rc);
        RegenerateScene(rc.right - rc.left, rc.bottom - rc.top);
        SetTimer(hwnd, 1, 33, nullptr);
        ApplyOverlayStyles(hwnd);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_SIZE: {
        int w = LOWORD(lParam);
        int h = HIWORD(lParam);
        RegenerateScene(w, h);
        InvalidateRect(hwnd, nullptr, TRUE);
        return 0;
    }
    case WM_TIMER: {
        UpdateAnimation();
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    case WM_LBUTTONDOWN:
        ReleaseCapture();
        SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        return 0;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE || wParam == 'Q') {
            DestroyWindow(hwnd);
            return 0;
        }
        if (wParam == 'C') {
            g_clickThrough = !g_clickThrough;
            ApplyOverlayStyles(hwnd);
            return 0;
        }
        return 0;
    case WM_RBUTTONUP: {
        HMENU menu = CreatePopupMenu();
        AppendMenu(menu, MF_STRING, 1, g_clickThrough ? L"Disable Click-Through (C)" : L"Enable Click-Through (C)");
        AppendMenu(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenu(menu, MF_STRING, 2, L"Exit (Esc)");
        POINT pt{};
        GetCursorPos(&pt);
        SetForegroundWindow(hwnd);
        int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd, nullptr);
        DestroyMenu(menu);
        if (cmd == 1) {
            g_clickThrough = !g_clickThrough;
            ApplyOverlayStyles(hwnd);
        } else if (cmd == 2) {
            DestroyWindow(hwnd);
        }
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rc{};
        GetClientRect(hwnd, &rc);

        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;
        HDC mem = CreateCompatibleDC(hdc);
        HBITMAP bmp = CreateCompatibleBitmap(hdc, w, h);
        HGDIOBJ oldBmp = SelectObject(mem, bmp);

        HBRUSH keyBrush = CreateSolidBrush(kTransparentKey);
        FillRect(mem, &rc, keyBrush);
        DeleteObject(keyBrush);

        DrawTree(mem);
        DrawOrnaments(mem);
        DrawSnow(mem);

        BitBlt(hdc, 0, 0, w, h, mem, 0, 0, SRCCOPY);

        SelectObject(mem, oldBmp);
        DeleteObject(bmp);
        DeleteDC(mem);

        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"XmassTreeWindow";

    WNDCLASS wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.style = CS_HREDRAW | CS_VREDRAW;

    if (!RegisterClass(&wc)) {
        MessageBox(nullptr, L"Failed to register window class.", L"Error", MB_OK | MB_ICONERROR);
        return 0;
    }

    RECT work{};
    SystemParametersInfo(SPI_GETWORKAREA, 0, &work, 0);
    int winW = 420;
    int winH = 520;
    int x = work.right - winW - 20;
    int y = work.bottom - winH - 20;

    HWND hwnd = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        CLASS_NAME,
        L"Xmass Tree",
        WS_POPUP,
        x,
        y,
        winW,
        winH,
        nullptr,
        nullptr,
        hInstance,
        nullptr);

    if (!hwnd) {
        MessageBox(nullptr, L"Failed to create window.", L"Error", MB_OK | MB_ICONERROR);
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);

    MSG msg{};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
