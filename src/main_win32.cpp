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

static int ClampInt(int v, int lo, int hi) {
    return std::max(lo, std::min(hi, v));
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

    HBRUSH treeBrush = CreateSolidBrush(RGB(10, 120, 40));
    HPEN treePen = CreatePen(PS_SOLID, 2, RGB(5, 90, 30));
    HGDIOBJ oldBrush = SelectObject(hdc, treeBrush);
    HGDIOBJ oldPen = SelectObject(hdc, treePen);

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
        Polygon(hdc, tri, 3);

        // subtle highlight
        HPEN highlightPen = CreatePen(PS_SOLID, 1, RGB(30, 160, 70));
        SelectObject(hdc, highlightPen);
        MoveToEx(hdc, cx, y0, nullptr);
        LineTo(hdc, cx - halfW1, y1);
        MoveToEx(hdc, cx, y0, nullptr);
        LineTo(hdc, cx + halfW1, y1);
        SelectObject(hdc, treePen);
        DeleteObject(highlightPen);
    }

    // trunk
    HBRUSH trunkBrush = CreateSolidBrush(RGB(120, 70, 30));
    SelectObject(hdc, trunkBrush);
    int trunkW = static_cast<int>(w * 0.06);
    int trunkH = static_cast<int>(h * 0.10);
    Rectangle(hdc, cx - trunkW / 2, baseY, cx + trunkW / 2, baseY + trunkH);
    DeleteObject(trunkBrush);

    // star
    HBRUSH starBrush = CreateSolidBrush(RGB(255, 220, 70));
    HPEN starPen = CreatePen(PS_SOLID, 2, RGB(255, 200, 20));
    SelectObject(hdc, starBrush);
    SelectObject(hdc, starPen);
    DrawStar(hdc, cx, topY - static_cast<int>(h * 0.02), static_cast<int>(w * 0.03), static_cast<int>(w * 0.015));
    DeleteObject(starBrush);
    DeleteObject(starPen);

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(treeBrush);
    DeleteObject(treePen);
}

static void DrawOrnaments(HDC hdc) {
    for (const auto& o : g_state.ornaments) {
        COLORREF c = o.on ? o.colorA : o.colorB;
        HBRUSH b = CreateSolidBrush(c);
        HPEN p = CreatePen(PS_SOLID, 1, RGB(20, 20, 20));
        HGDIOBJ oldB = SelectObject(hdc, b);
        HGDIOBJ oldP = SelectObject(hdc, p);

        Ellipse(hdc, o.pos.x - o.radius, o.pos.y - o.radius, o.pos.x + o.radius, o.pos.y + o.radius);

        // tiny shine
        SetPixel(hdc, o.pos.x - o.radius / 3, o.pos.y - o.radius / 3, RGB(255, 255, 255));

        SelectObject(hdc, oldB);
        SelectObject(hdc, oldP);
        DeleteObject(b);
        DeleteObject(p);
    }
}

static void DrawSnow(HDC hdc) {
    HBRUSH snowBrush = CreateSolidBrush(RGB(255, 255, 255));
    HGDIOBJ old = SelectObject(hdc, snowBrush);
    for (const auto& s : g_state.snowflakes) {
        int x = static_cast<int>(s.x);
        int y = static_cast<int>(s.y);
        Ellipse(hdc, x - s.radius, y - s.radius, x + s.radius, y + s.radius);
    }
    SelectObject(hdc, old);
    DeleteObject(snowBrush);
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
        return 0;
    }
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
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rc{};
        GetClientRect(hwnd, &rc);
        HBRUSH bg = CreateSolidBrush(RGB(8, 18, 40));
        FillRect(hdc, &rc, bg);
        DeleteObject(bg);

        // ground snow
        RECT ground = rc;
        ground.top = static_cast<LONG>(g_state.height * 0.82);
        HBRUSH groundBrush = CreateSolidBrush(RGB(240, 245, 255));
        FillRect(hdc, &ground, groundBrush);
        DeleteObject(groundBrush);

        DrawTree(hdc);
        DrawOrnaments(hdc);
        DrawSnow(hdc);

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
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.style = CS_HREDRAW | CS_VREDRAW;

    if (!RegisterClass(&wc)) {
        MessageBox(nullptr, L"Failed to register window class.", L"Error", MB_OK | MB_ICONERROR);
        return 0;
    }

    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"Xmass Tree",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        900,
        700,
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
