#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#endif

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#include <winreg.h>
#endif

#include <GLFW/glfw3.h>

#ifndef GL_MULTISAMPLE
#define GL_MULTISAMPLE 0x809D
#endif

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

struct Color {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;
};

static Color FromRGB(int r, int g, int b, float a = 1.0f) {
    return {
        r / 255.0f,
        g / 255.0f,
        b / 255.0f,
        a,
    };
}

static int ClampInt(int v, int lo, int hi) {
    return std::max(lo, std::min(hi, v));
}

static Color AdjustColor(Color c, int delta) {
    auto clamp01 = [](float v) { return std::max(0.0f, std::min(1.0f, v)); };
    float d = delta / 255.0f;
    c.r = clamp01(c.r + d);
    c.g = clamp01(c.g + d);
    c.b = clamp01(c.b + d);
    return c;
}

struct Ornament {
    float x = 0.0f;
    float y = 0.0f;
    float radius = 6.0f;
    Color colorA{};
    Color colorB{};
    bool on = true;
};

struct Snowflake {
    float x = 0.0f;
    float y = 0.0f;
    float speed = 0.8f;
    float drift = 0.0f;
    float radius = 2.0f;
};

struct TreeLayer {
    float y0 = 0.0f;
    float y1 = 0.0f;
    float halfW = 0.0f;
};

struct NeedleStroke {
    float x1 = 0.0f;
    float y1 = 0.0f;
    float x2 = 0.0f;
    float y2 = 0.0f;
    Color c{};
};

struct AppState {
    int width = 800;
    int height = 600;
    int blinkPhase = 0;
    int layerCount = 6;
    float treeCx = 400.0f;
    float treeTopY = 60.0f;
    float treeBottomY = 480.0f;
    float treeBaseHalfW = 200.0f;
    float layerHeight = 80.0f;
    float layerOverlap = 40.0f;
    std::vector<TreeLayer> layers;
    std::vector<NeedleStroke> needles;
    std::vector<Ornament> ornaments;
    std::vector<Snowflake> snowflakes;
    std::mt19937 rng{std::random_device{}()};
};

static AppState g_state{};
static bool g_clickThrough = false;
static bool g_dragging = false;
static double g_dragStartScreenX = 0.0;
static double g_dragStartScreenY = 0.0;
static int g_dragStartWinX = 0;
static int g_dragStartWinY = 0;

static void SetClickThrough(GLFWwindow* window, bool enabled) {
    g_clickThrough = enabled;
    glfwSetWindowAttrib(window, GLFW_MOUSE_PASSTHROUGH, enabled ? GLFW_TRUE : GLFW_FALSE);
    if (enabled) {
        g_dragging = false;
    }
}

#ifdef _WIN32
static GLFWwindow* g_overlayWindow = nullptr;
static HWND g_trayHwnd = nullptr;
static UINT g_taskbarCreatedMsg = 0;
static bool g_startupEnabled = false;

static constexpr UINT kTrayCallbackMsg = WM_APP + 42;
static constexpr UINT kTrayId = 1;

static constexpr UINT kMenuToggleShow = 1001;
static constexpr UINT kMenuToggleClickThrough = 1002;
static constexpr UINT kMenuToggleStartup = 1003;
static constexpr UINT kMenuExit = 1099;

static std::wstring GetExePath() {
    std::array<wchar_t, 4096> buf{};
    DWORD len = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
    if (len == 0 || len >= buf.size()) {
        return L"";
    }
    return std::wstring(buf.data(), buf.data() + len);
}

static bool IsStartupEnabled() {
    HKEY key{};
    if (RegOpenKeyExW(
            HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
            0,
            KEY_QUERY_VALUE,
            &key) != ERROR_SUCCESS) {
        return false;
    }

    DWORD type = 0;
    DWORD size = 0;
    LONG res = RegQueryValueExW(key, L"XmassTree", nullptr, &type, nullptr, &size);
    RegCloseKey(key);
    return res == ERROR_SUCCESS && type == REG_SZ;
}

static bool SetStartupEnabled(bool enabled) {
    HKEY key{};
    if (RegCreateKeyExW(
            HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
            0,
            nullptr,
            0,
            KEY_SET_VALUE,
            nullptr,
            &key,
            nullptr) != ERROR_SUCCESS) {
        return false;
    }

    LONG res = ERROR_SUCCESS;
    if (enabled) {
        std::wstring exe = GetExePath();
        if (exe.empty()) {
            RegCloseKey(key);
            return false;
        }
        std::wstring value = L"\"" + exe + L"\"";
        res = RegSetValueExW(
            key,
            L"XmassTree",
            0,
            REG_SZ,
            reinterpret_cast<const BYTE*>(value.c_str()),
            static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
    } else {
        res = RegDeleteValueW(key, L"XmassTree");
        if (res == ERROR_FILE_NOT_FOUND) {
            res = ERROR_SUCCESS;
        }
    }

    RegCloseKey(key);
    return res == ERROR_SUCCESS;
}

static void SetWindowToolStyle(GLFWwindow* window) {
    HWND hwnd = glfwGetWin32Window(window);
    LONG_PTR ex = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    ex |= WS_EX_TOOLWINDOW;
    ex &= ~WS_EX_APPWINDOW;
    SetWindowLongPtrW(hwnd, GWL_EXSTYLE, ex);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

static void ToggleOverlayVisible() {
    if (!g_overlayWindow) return;
    int visible = glfwGetWindowAttrib(g_overlayWindow, GLFW_VISIBLE);
    if (visible == GLFW_TRUE) {
        glfwHideWindow(g_overlayWindow);
        g_dragging = false;
    } else {
        glfwShowWindow(g_overlayWindow);
        glfwFocusWindow(g_overlayWindow);
    }
}

static void ShowTrayMenu(HWND hwnd) {
    HMENU menu = CreatePopupMenu();
    bool visible = g_overlayWindow && glfwGetWindowAttrib(g_overlayWindow, GLFW_VISIBLE) == GLFW_TRUE;
    AppendMenuW(menu, MF_STRING, kMenuToggleShow, visible ? L"Hide Overlay" : L"Show Overlay");

    UINT clickFlags = MF_STRING;
    if (g_clickThrough) clickFlags |= MF_CHECKED;
    AppendMenuW(menu, clickFlags, kMenuToggleClickThrough, L"Click-Through (C)");

    UINT startupFlags = MF_STRING;
    if (g_startupEnabled) startupFlags |= MF_CHECKED;
    AppendMenuW(menu, startupFlags, kMenuToggleStartup, L"Start On Startup");

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuExit, L"Exit");

    POINT pt{};
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
    PostMessageW(hwnd, WM_NULL, 0, 0);
    DestroyMenu(menu);
}

static void CreateOrUpdateTrayIcon(bool create) {
    if (!g_trayHwnd) return;

    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = g_trayHwnd;
    nid.uID = kTrayId;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = kTrayCallbackMsg;
    nid.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wcscpy_s(nid.szTip, L"Xmass Tree");

    if (create) {
        Shell_NotifyIconW(NIM_ADD, &nid);
        nid.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &nid);
    } else {
        Shell_NotifyIconW(NIM_MODIFY, &nid);
    }
}

static void DeleteTrayIcon() {
    if (!g_trayHwnd) return;
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = g_trayHwnd;
    nid.uID = kTrayId;
    Shell_NotifyIconW(NIM_DELETE, &nid);
}

static LRESULT CALLBACK TrayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == kTrayCallbackMsg) {
        switch (lParam) {
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
            ToggleOverlayVisible();
            return 0;
        case WM_RBUTTONUP:
        case WM_CONTEXTMENU:
            ShowTrayMenu(hwnd);
            return 0;
        default:
            return 0;
        }
    }

    if (msg == WM_COMMAND) {
        switch (LOWORD(wParam)) {
        case kMenuToggleShow:
            ToggleOverlayVisible();
            return 0;
        case kMenuToggleClickThrough:
            if (g_overlayWindow) {
                SetClickThrough(g_overlayWindow, !g_clickThrough);
            }
            return 0;
        case kMenuToggleStartup: {
            bool want = !g_startupEnabled;
            if (SetStartupEnabled(want)) {
                g_startupEnabled = want;
            }
            return 0;
        }
        case kMenuExit:
            if (g_overlayWindow) {
                glfwSetWindowShouldClose(g_overlayWindow, GLFW_TRUE);
            }
            return 0;
        default:
            return 0;
        }
    }

    if (g_taskbarCreatedMsg != 0 && msg == g_taskbarCreatedMsg) {
        CreateOrUpdateTrayIcon(true);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static bool InitTray(GLFWwindow* overlayWindow) {
    g_overlayWindow = overlayWindow;
    g_startupEnabled = IsStartupEnabled();
    if (!g_startupEnabled) {
        if (SetStartupEnabled(true)) {
            g_startupEnabled = true;
        }
    }
    g_taskbarCreatedMsg = RegisterWindowMessageW(L"TaskbarCreated");

    const wchar_t kClassName[] = L"XmassTreeTrayWindow";
    WNDCLASSW wc{};
    wc.lpfnWndProc = TrayWndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kClassName;

    if (!RegisterClassW(&wc)) {
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            return false;
        }
    }

    g_trayHwnd = CreateWindowExW(
        0,
        kClassName,
        L"",
        0,
        0,
        0,
        0,
        0,
        HWND_MESSAGE,
        nullptr,
        wc.hInstance,
        nullptr);

    if (!g_trayHwnd) {
        return false;
    }

    CreateOrUpdateTrayIcon(true);
    return true;
}
#endif

static float RandFloat(std::mt19937& rng, float lo, float hi) {
    std::uniform_real_distribution<float> dist(lo, hi);
    return dist(rng);
}

static int RandInt(std::mt19937& rng, int lo, int hi) {
    std::uniform_int_distribution<int> dist(lo, hi);
    return dist(rng);
}

static void RebuildTreeGeometry() {
    const int w = g_state.width;
    const int h = g_state.height;

    g_state.treeCx = w * 0.5f;
    g_state.treeTopY = h * 0.11f;
    g_state.treeBottomY = h * 0.80f;
    g_state.treeBaseHalfW = w * 0.30f;

    g_state.layerCount = ClampInt(w / 70, 5, 9);
    g_state.layerHeight = (g_state.treeBottomY - g_state.treeTopY) / static_cast<float>(g_state.layerCount);
    g_state.layerOverlap = g_state.layerHeight * 0.65f;

    g_state.layers.clear();
    g_state.layers.reserve(static_cast<size_t>(g_state.layerCount));

    for (int i = 0; i < g_state.layerCount; ++i) {
        float y0 = g_state.treeTopY + i * g_state.layerHeight;
        float y1 = (i == g_state.layerCount - 1) ? g_state.treeBottomY : (y0 + g_state.layerHeight + g_state.layerOverlap);
        float progress = static_cast<float>(i + 1) / static_cast<float>(g_state.layerCount);
        float halfW = g_state.treeBaseHalfW * std::pow(progress, 1.25f);
        g_state.layers.push_back({y0, y1, halfW});
    }
}

static float TreeHalfWidthAtY(float y) {
    float maxW = 0.0f;
    for (const auto& layer : g_state.layers) {
        if (y < layer.y0 || y > layer.y1) continue;
        float denom = std::max(1.0f, layer.y1 - layer.y0);
        float t = (y - layer.y0) / denom;
        float w = t * layer.halfW;
        maxW = std::max(maxW, w);
    }
    return maxW;
}

static void RegenerateScene(int w, int h) {
    g_state.width = std::max(200, w);
    g_state.height = std::max(200, h);

    const int width = g_state.width;
    const int height = g_state.height;

    RebuildTreeGeometry();

    const int ornamentCount = ClampInt((width * height) / 25000, 35, 140);
    g_state.ornaments.clear();
    g_state.ornaments.reserve(ornamentCount);

    std::array<Color, 6> palette = {
        FromRGB(255, 60, 60),   // red
        FromRGB(60, 220, 80),   // green
        FromRGB(255, 210, 60),  // gold
        FromRGB(80, 160, 255),  // blue
        FromRGB(255, 120, 240), // pink
        FromRGB(255, 255, 255), // white
    };

    for (int i = 0; i < ornamentCount; ++i) {
        float t = std::pow(RandFloat(g_state.rng, 0.0f, 1.0f), 0.70f);
        float y = g_state.treeTopY + t * (g_state.treeBottomY - g_state.treeTopY);
        float halfW = TreeHalfWidthAtY(y) * 0.92f;
        float x = g_state.treeCx + RandFloat(g_state.rng, -halfW, halfW);

        Ornament o;
        o.x = x;
        o.y = y;
        o.radius = static_cast<float>(RandInt(g_state.rng, 4, 9));
        int idxA = RandInt(g_state.rng, 0, static_cast<int>(palette.size() - 1));
        int idxB = RandInt(g_state.rng, 0, static_cast<int>(palette.size() - 1));
        o.colorA = palette[idxA];
        o.colorB = palette[idxB];
        o.on = RandInt(g_state.rng, 0, 1) == 1;
        g_state.ornaments.push_back(o);
    }

    const int needleCount = ClampInt((width * height) / 900, 300, 2000);
    g_state.needles.clear();
    g_state.needles.reserve(static_cast<size_t>(needleCount));
    for (int i = 0; i < needleCount; ++i) {
        float t = std::pow(RandFloat(g_state.rng, 0.0f, 1.0f), 0.85f);
        float y = g_state.treeTopY + t * (g_state.treeBottomY - g_state.treeTopY);
        float halfW = TreeHalfWidthAtY(y) * 0.95f;
        if (halfW < 6.0f) continue;
        float x = g_state.treeCx + RandFloat(g_state.rng, -halfW, halfW);

        float dir = (x < g_state.treeCx) ? -1.0f : 1.0f;
        float len = RandFloat(g_state.rng, 2.5f, 6.5f);
        float dy = RandFloat(g_state.rng, -1.4f, 1.4f);
        float dx = dir * len;

        NeedleStroke n;
        n.x1 = x;
        n.y1 = y;
        n.x2 = x + dx;
        n.y2 = y + dy;
        n.c = AdjustColor(FromRGB(8, 120, 45), RandInt(g_state.rng, -22, 26));
        n.c.a = 0.55f;
        g_state.needles.push_back(n);
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
        s.radius = static_cast<float>(RandInt(g_state.rng, 1, 3));
        g_state.snowflakes.push_back(s);
    }
}

static void SetColor(const Color& c) {
    glColor4f(c.r, c.g, c.b, c.a);
}

static void DrawCircle(float cx, float cy, float r, const Color& c, int segments = 28) {
    SetColor(c);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for (int i = 0; i <= segments; ++i) {
        float a = static_cast<float>(i) / segments * 2.0f * 3.1415926f;
        glVertex2f(cx + std::cos(a) * r, cy + std::sin(a) * r);
    }
    glEnd();
}

static void DrawStar(float cx, float cy, float rOuter, float rInner, const Color& c) {
    constexpr float pi = 3.1415926f;
    std::array<std::pair<float, float>, 10> pts{};
    for (int i = 0; i < 10; ++i) {
        float angle = (i * 36.0f - 90.0f) * pi / 180.0f;
        float r = (i % 2 == 0) ? rOuter : rInner;
        pts[i] = {cx + std::cos(angle) * r, cy + std::sin(angle) * r};
    }

    SetColor(c);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for (int i = 0; i < 10; ++i) {
        glVertex2f(pts[i].first, pts[i].second);
    }
    glVertex2f(pts[0].first, pts[0].second);
    glEnd();
}

static void DrawSolidTriangle(float x0, float y0, float x1, float y1, float x2, float y2, const Color& c) {
    SetColor(c);
    glBegin(GL_TRIANGLES);
    glVertex2f(x0, y0);
    glVertex2f(x1, y1);
    glVertex2f(x2, y2);
    glEnd();
}

static void DrawTriangleGradient(float x0, float y0, float x1, float y1, float x2, float y2, const Color& c0, const Color& c1, const Color& c2) {
    glBegin(GL_TRIANGLES);
    SetColor(c0);
    glVertex2f(x0, y0);
    SetColor(c1);
    glVertex2f(x1, y1);
    SetColor(c2);
    glVertex2f(x2, y2);
    glEnd();
}

static void DrawNeedles() {
    glLineWidth(1.0f);
    glBegin(GL_LINES);
    for (const auto& n : g_state.needles) {
        SetColor(n.c);
        glVertex2f(n.x1, n.y1);
        glVertex2f(n.x2, n.y2);
    }
    glEnd();
}

static void DrawLayerGarland(int layerIndex, float y0, float y1, float halfW) {
    float garlandY = y0 + (y1 - y0) * 0.72f;
    float t = (garlandY - y0) / std::max(1.0f, (y1 - y0));
    float garlandHalfW = t * halfW;
    int segments = ClampInt(static_cast<int>(halfW / 10.0f), 18, 32);
    std::array<std::pair<float, float>, 40> pts{};

    float phase = g_state.blinkPhase * 0.10f + layerIndex * 0.6f;
    for (int i = 0; i <= segments; ++i) {
        float u = static_cast<float>(i) / segments;
        float x = g_state.treeCx - garlandHalfW + u * garlandHalfW * 2.0f;
        float wave = std::sin(u * 3.1415926f * 2.0f + phase) * (g_state.layerHeight * 0.10f);
        pts[static_cast<size_t>(i)] = {x, garlandY + wave};
    }

    Color garlandColor = FromRGB(255, 210, 80);
    garlandColor.a = 0.9f;
    glLineWidth(2.0f);
    SetColor(garlandColor);
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i <= segments; ++i) {
        auto p = pts[static_cast<size_t>(i)];
        glVertex2f(p.first, p.second);
    }
    glEnd();

    for (int i = 0; i <= segments; i += 3) {
        auto p = pts[static_cast<size_t>(i)];
        float r = 2.7f + (i % 2);
        bool on = ((g_state.blinkPhase / 6 + i + layerIndex * 2) % 2) == 0;
        Color bead = on ? FromRGB(255, 80, 80) : FromRGB(240, 240, 255);
        bead.a = on ? 1.0f : 0.9f;
        DrawCircle(p.first, p.second, r, bead, 18);
    }
}

static void DrawTree() {
    const float cx = g_state.treeCx;
    const float topY = g_state.treeTopY;
    const float bottomY = g_state.treeBottomY;

    Color baseGreen = FromRGB(8, 120, 45);
    Color outline = FromRGB(5, 80, 30, 0.55f);

    // soft shadow behind the tree
    Color shadow = FromRGB(0, 0, 0, 0.16f);
    for (int i = g_state.layerCount - 1; i >= 0; --i) {
        const auto& layer = g_state.layers[static_cast<size_t>(i)];
        float y0 = layer.y0 + 5.0f;
        float y1 = layer.y1 + 5.0f;
        float hw = layer.halfW + 5.0f;
        DrawSolidTriangle(cx, y0, cx - hw, y1, cx + hw, y1, shadow);
    }

    // trunk behind branches
    float trunkW = g_state.treeBaseHalfW * 0.28f;
    float trunkH = (bottomY - topY) * 0.18f;
    float trunkTop = bottomY - trunkH * 0.15f;
    Color trunkTopC = FromRGB(150, 88, 38);
    Color trunkBottomC = FromRGB(92, 48, 18);
    glBegin(GL_QUADS);
    SetColor(trunkTopC);
    glVertex2f(cx - trunkW / 2.0f, trunkTop);
    glVertex2f(cx + trunkW / 2.0f, trunkTop);
    SetColor(trunkBottomC);
    glVertex2f(cx + trunkW / 2.0f, trunkTop + trunkH);
    glVertex2f(cx - trunkW / 2.0f, trunkTop + trunkH);
    glEnd();

    glLineWidth(2.0f);

    // layers from bottom -> top for correct overlap
    for (int i = g_state.layerCount - 1; i >= 0; --i) {
        const auto& layer = g_state.layers[static_cast<size_t>(i)];
        float y0 = layer.y0;
        float y1 = layer.y1;
        float hw = layer.halfW;

        float x0 = cx;
        float x1 = cx - hw;
        float x2 = cx + hw;

        Color topC = AdjustColor(baseGreen, 40 - i * 4);
        Color bottomC = AdjustColor(baseGreen, -18 - i * 3);

        DrawTriangleGradient(x0, y0, x1, y1, x2, y1, topC, bottomC, bottomC);

        // subtle depth: darker underside near the bottom edge
        float shadeH = std::max(10.0f, g_state.layerHeight * 0.28f);
        Color underside = FromRGB(0, 0, 0, 0.08f);
        DrawSolidTriangle(x0, y1 - shadeH * 0.55f, x1, y1, x2, y1, underside);

        // inner sheen to make it feel less flat
        Color sheen = AdjustColor(topC, 50);
        sheen.a = 0.10f;
        float innerScale = 0.55f;
        DrawTriangleGradient(
            x0,
            y0 + g_state.layerHeight * 0.10f,
            cx - hw * innerScale,
            y1 - g_state.layerHeight * 0.15f,
            cx + hw * innerScale,
            y1 - g_state.layerHeight * 0.15f,
            sheen,
            sheen,
            sheen);

        // branch fringe along the bottom edge for a more realistic silhouette
        int fringeCount = ClampInt(static_cast<int>(hw / 12.0f), 10, 26);
        float fringeAmp = std::max(8.0f, g_state.layerHeight * 0.22f);
        for (int j = 0; j < fringeCount; ++j) {
            float u0 = static_cast<float>(j) / fringeCount;
            float u2 = static_cast<float>(j + 1) / fringeCount;
            float u1 = (u0 + u2) * 0.5f;
            float bx0 = cx - hw + u0 * hw * 2.0f;
            float bx2 = cx - hw + u2 * hw * 2.0f;
            float bxc = cx - hw + u1 * hw * 2.0f;
            float baseY = y1 - 1.0f;
            float wobble = std::sin((u1 * 3.1415926f * 2.0f) + i * 0.8f) * (fringeAmp * 0.18f);
            float tipY = y1 + fringeAmp * (0.55f + 0.45f * std::sin(j * 0.9f + i * 0.7f)) + wobble;
            Color fringe = AdjustColor(bottomC, -10);
            fringe.a = 0.96f;
            DrawSolidTriangle(bx0, baseY, bxc, tipY, bx2, baseY, fringe);
        }

        // outline and highlights
        SetColor(outline);
        glBegin(GL_LINE_STRIP);
        glVertex2f(x1, y1);
        glVertex2f(x0, y0);
        glVertex2f(x2, y1);
        glEnd();

        Color highlight = AdjustColor(baseGreen, 85);
        highlight.a = 0.60f;
        SetColor(highlight);
        glBegin(GL_LINES);
        glVertex2f(x0, y0);
        glVertex2f(x1 + hw * 0.12f, y1 - g_state.layerHeight * 0.08f);
        glVertex2f(x0, y0);
        glVertex2f(x2 - hw * 0.12f, y1 - g_state.layerHeight * 0.08f);
        glEnd();
    }

    DrawNeedles();

    for (int i = g_state.layerCount - 1; i >= 0; --i) {
        const auto& layer = g_state.layers[static_cast<size_t>(i)];
        DrawLayerGarland(i, layer.y0, layer.y1, layer.halfW);
    }

    // star + glow
    float starY = topY - g_state.height * 0.03f;
    float outer = g_state.width * 0.040f;
    float inner = g_state.width * 0.019f;
    Color glow = AdjustColor(FromRGB(255, 220, 70), 25);
    glow.a = 0.40f;
    DrawStar(cx, starY, outer + 6.0f, inner + 3.0f, glow);

    Color star = FromRGB(255, 215, 60);
    DrawStar(cx, starY, outer, inner, star);
}

static void DrawOrnaments() {
    for (const auto& o : g_state.ornaments) {
        Color c = o.on ? o.colorA : o.colorB;
        float glowR = o.radius + (o.on ? 3.0f : 1.0f);
        Color glow = AdjustColor(c, 40);
        glow.a = o.on ? 0.40f : 0.22f;
        DrawCircle(o.x, o.y, glowR, glow);

        DrawCircle(o.x, o.y, o.radius, c);

        if (o.radius >= 5.0f) {
            float innerR = o.radius - 2.0f;
            Color inner = AdjustColor(c, 25);
            inner.a = 0.9f;
            DrawCircle(o.x, o.y, innerR, inner);
        }

        Color shine = FromRGB(255, 255, 255, 0.9f);
        DrawCircle(o.x - o.radius / 3.0f, o.y - o.radius / 3.0f, 1.5f, shine, 10);
    }
}

static void DrawSnow() {
    for (const auto& s : g_state.snowflakes) {
        Color c = (s.radius >= 3.0f) ? FromRGB(230, 240, 255) : FromRGB(255, 255, 255);
        c.a = 0.95f;
        DrawCircle(s.x, s.y, s.radius, c, 14);
    }
}

static void UpdateAnimationStep() {
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
            s.radius = static_cast<float>(RandInt(g_state.rng, 1, 3));
        }
        if (s.x < -10) s.x = static_cast<float>(g_state.width + 5);
        if (s.x > g_state.width + 10) s.x = -5.0f;
    }
}

static void FramebufferSizeCallback(GLFWwindow*, int w, int h) {
    RegenerateScene(w, h);
}

static void KeyCallback(GLFWwindow* window, int key, int, int action, int) {
    if (action != GLFW_PRESS) return;

    if (key == GLFW_KEY_ESCAPE || key == GLFW_KEY_Q) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
        return;
    }

    if (key == GLFW_KEY_C) {
        SetClickThrough(window, !g_clickThrough);
        return;
    }

    if (key == GLFW_KEY_R) {
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        RegenerateScene(w, h);
        return;
    }
}

static void MouseButtonCallback(GLFWwindow* window, int button, int action, int) {
    if (g_clickThrough) return;

    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            g_dragging = true;
            double cursorX = 0.0;
            double cursorY = 0.0;
            int winX = 0;
            int winY = 0;
            glfwGetCursorPos(window, &cursorX, &cursorY);
            glfwGetWindowPos(window, &winX, &winY);
            g_dragStartScreenX = winX + cursorX;
            g_dragStartScreenY = winY + cursorY;
            g_dragStartWinX = winX;
            g_dragStartWinY = winY;
        } else if (action == GLFW_RELEASE) {
            g_dragging = false;
        }
    }
}

static void CursorPosCallback(GLFWwindow* window, double x, double y) {
    if (!g_dragging || g_clickThrough) return;

    int winX = 0;
    int winY = 0;
    glfwGetWindowPos(window, &winX, &winY);

    double cursorScreenX = winX + x;
    double cursorScreenY = winY + y;
    int newWinX = g_dragStartWinX + static_cast<int>(std::lround(cursorScreenX - g_dragStartScreenX));
    int newWinY = g_dragStartWinY + static_cast<int>(std::lround(cursorScreenY - g_dragStartScreenY));
    if (newWinX == winX && newWinY == winY) {
        return;
    }
    glfwSetWindowPos(window, newWinX, newWinY);
}

static void WindowCloseCallback(GLFWwindow* window) {
#ifdef _WIN32
    // Keep running from tray; hide instead of exiting.
    if (g_trayHwnd) {
        glfwSetWindowShouldClose(window, GLFW_FALSE);
        glfwHideWindow(window);
        g_dragging = false;
        return;
    }
#endif
}

static void PositionBottomRight(GLFWwindow* window, int winW, int winH) {
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    int mx = 0, my = 0, mw = 0, mh = 0;
    glfwGetMonitorWorkarea(monitor, &mx, &my, &mw, &mh);
    int x = mx + mw - winW - 20;
    int y = my + mh - winH - 20;
    glfwSetWindowPos(window, x, y);
}

int main() {
    if (!glfwInit()) {
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    const int initialW = 420;
    const int initialH = 520;
    GLFWwindow* window = glfwCreateWindow(initialW, initialH, "Xmass Tree", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    glEnable(GL_MULTISAMPLE);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

    glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
    glfwSetKeyCallback(window, KeyCallback);
    glfwSetMouseButtonCallback(window, MouseButtonCallback);
    glfwSetCursorPosCallback(window, CursorPosCallback);
    glfwSetWindowCloseCallback(window, WindowCloseCallback);

    int fbW, fbH;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    RegenerateScene(fbW, fbH);
    PositionBottomRight(window, initialW, initialH);

    SetClickThrough(window, false);

#ifdef _WIN32
    SetWindowToolStyle(window);
    InitTray(window);
#endif

    double lastTime = glfwGetTime();
    double accumulator = 0.0;
    const double step = 1.0 / 30.0;

    while (!glfwWindowShouldClose(window)) {
        bool visible = glfwGetWindowAttrib(window, GLFW_VISIBLE) == GLFW_TRUE;
        if (!visible) {
            glfwWaitEventsTimeout(0.25);
        } else {
            glfwPollEvents();
        }

        double now = glfwGetTime();
        double dt = now - lastTime;
        lastTime = now;
        accumulator += dt;
        while (accumulator >= step) {
            UpdateAnimationStep();
            accumulator -= step;
        }

        if (visible) {
            int w, h;
            glfwGetFramebufferSize(window, &w, &h);
            glViewport(0, 0, w, h);
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            glOrtho(0, w, h, 0, -1, 1);
            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();

            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            DrawTree();
            DrawOrnaments();
            DrawSnow();

            glfwSwapBuffers(window);
        }
    }

#ifdef _WIN32
    DeleteTrayIcon();
    if (g_trayHwnd) {
        DestroyWindow(g_trayHwnd);
        g_trayHwnd = nullptr;
    }
#endif

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

#ifdef _WIN32
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    return main();
}
#endif
