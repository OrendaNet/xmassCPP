#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#endif

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <GLFW/glfw3.h>

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

struct AppState {
    int width = 800;
    int height = 600;
    int blinkPhase = 0;
    std::vector<Ornament> ornaments;
    std::vector<Snowflake> snowflakes;
    std::mt19937 rng{std::random_device{}()};
};

static AppState g_state{};
static bool g_clickThrough = false;
static bool g_dragging = false;
static double g_dragStartCursorX = 0.0;
static double g_dragStartCursorY = 0.0;
static int g_dragStartWinX = 0;
static int g_dragStartWinY = 0;

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

    std::array<Color, 6> palette = {
        FromRGB(255, 60, 60),   // red
        FromRGB(60, 220, 80),   // green
        FromRGB(255, 210, 60),  // gold
        FromRGB(80, 160, 255),  // blue
        FromRGB(255, 120, 240), // pink
        FromRGB(255, 255, 255), // white
    };

    for (int i = 0; i < ornamentCount; ++i) {
        float t = RandFloat(g_state.rng, 0.05f, 1.0f);
        int y = treeTopY + static_cast<int>(t * (treeBaseY - treeTopY));
        float halfW = t * treeHalfWidthBase;
        int x = treeCenterX + static_cast<int>(RandFloat(g_state.rng, -halfW, halfW));

        Ornament o;
        o.x = static_cast<float>(x);
        o.y = static_cast<float>(y);
        o.radius = static_cast<float>(RandInt(g_state.rng, 4, 9));
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

static void DrawTree() {
    const int w = g_state.width;
    const int h = g_state.height;
    const float cx = w / 2.0f;

    const float topY = h * 0.10f;
    const float baseY = h * 0.80f;
    const float tierHeight = (baseY - topY) / 3.0f;

    Color baseGreen = FromRGB(8, 120, 45);
    Color outline = FromRGB(5, 80, 30);

    glLineWidth(2.0f);

    for (int tier = 0; tier < 3; ++tier) {
        float y0 = topY + tier * tierHeight;
        float y1 = y0 + tierHeight + tierHeight * 0.4f;
        float halfW1 = w * (0.22f + tier * 0.12f);

        float triX0 = cx;
        float triY0 = y0;
        float triX1 = cx - halfW1;
        float triY1 = y1;
        float triX2 = cx + halfW1;
        float triY2 = y1;

        Color topC = AdjustColor(baseGreen, 35 - tier * 8);
        Color bottomC = AdjustColor(baseGreen, -10 - tier * 6);

        glBegin(GL_TRIANGLES);
        SetColor(topC);
        glVertex2f(triX0, triY0);
        SetColor(bottomC);
        glVertex2f(triX1, triY1);
        glVertex2f(triX2, triY2);
        glEnd();

        SetColor(outline);
        glBegin(GL_LINE_LOOP);
        glVertex2f(triX0, triY0);
        glVertex2f(triX1, triY1);
        glVertex2f(triX2, triY2);
        glEnd();

        // subtle highlight lines
        Color highlight = AdjustColor(baseGreen, 70);
        highlight.a = 0.8f;
        SetColor(highlight);
        glBegin(GL_LINES);
        glVertex2f(triX0, triY0);
        glVertex2f(triX1, triY1);
        glVertex2f(triX0, triY0);
        glVertex2f(triX2, triY2);
        glEnd();

        // garland
        float garlandY = y0 + (y1 - y0) * 0.60f;
        float t = (garlandY - y0) / (y1 - y0);
        float garlandHalfW = t * halfW1;
        constexpr int segments = 24;
        std::array<std::pair<float, float>, segments + 1> garlandPts{};
        float phase = g_state.blinkPhase * 0.10f + tier * 0.8f;
        for (int i = 0; i <= segments; ++i) {
            float u = static_cast<float>(i) / segments;
            float x = cx - garlandHalfW + u * garlandHalfW * 2.0f;
            float wave = std::sin(u * 3.1415926f * 2.0f + phase) * (tierHeight * 0.07f);
            garlandPts[i] = {x, garlandY + wave};
        }
        Color garlandColor = FromRGB(255, 210, 80);
        garlandColor.a = 0.9f;
        glLineWidth(2.0f);
        SetColor(garlandColor);
        glBegin(GL_LINE_STRIP);
        for (auto& p : garlandPts) {
            glVertex2f(p.first, p.second);
        }
        glEnd();

        for (int i = 0; i <= segments; i += 3) {
            auto p = garlandPts[i];
            float r = 3.0f + (i % 2);
            bool on = ((g_state.blinkPhase / 6 + i) % 2) == 0;
            Color bead = on ? FromRGB(255, 80, 80) : FromRGB(240, 240, 255);
            bead.a = on ? 1.0f : 0.9f;
            DrawCircle(p.first, p.second, r, bead);
        }
    }

    // trunk
    float trunkW = w * 0.06f;
    float trunkH = h * 0.10f;
    Color trunk = FromRGB(120, 65, 28);
    SetColor(trunk);
    glBegin(GL_QUADS);
    glVertex2f(cx - trunkW / 2.0f, baseY);
    glVertex2f(cx + trunkW / 2.0f, baseY);
    glVertex2f(cx + trunkW / 2.0f, baseY + trunkH);
    glVertex2f(cx - trunkW / 2.0f, baseY + trunkH);
    glEnd();

    // star + glow
    float starY = topY - h * 0.02f;
    float outer = w * 0.035f;
    float inner = w * 0.017f;
    Color glow = AdjustColor(FromRGB(255, 220, 70), 25);
    glow.a = 0.45f;
    DrawStar(cx, starY, outer + 4.0f, inner + 2.0f, glow);

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
        g_clickThrough = !g_clickThrough;
        glfwSetWindowAttrib(window, GLFW_MOUSE_PASSTHROUGH, g_clickThrough ? GLFW_TRUE : GLFW_FALSE);
        if (g_clickThrough) {
            g_dragging = false;
        }
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
            glfwGetCursorPos(window, &g_dragStartCursorX, &g_dragStartCursorY);
            glfwGetWindowPos(window, &g_dragStartWinX, &g_dragStartWinY);
        } else if (action == GLFW_RELEASE) {
            g_dragging = false;
        }
    }
}

static void CursorPosCallback(GLFWwindow* window, double x, double y) {
    if (!g_dragging || g_clickThrough) return;

    int dx = static_cast<int>(x - g_dragStartCursorX);
    int dy = static_cast<int>(y - g_dragStartCursorY);
    glfwSetWindowPos(window, g_dragStartWinX + dx, g_dragStartWinY + dy);
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

    glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
    glfwSetKeyCallback(window, KeyCallback);
    glfwSetMouseButtonCallback(window, MouseButtonCallback);
    glfwSetCursorPosCallback(window, CursorPosCallback);

    int fbW, fbH;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    RegenerateScene(fbW, fbH);
    PositionBottomRight(window, initialW, initialH);

    glfwSetWindowAttrib(window, GLFW_MOUSE_PASSTHROUGH, GLFW_FALSE);

    double lastTime = glfwGetTime();
    double accumulator = 0.0;
    const double step = 1.0 / 30.0;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        double now = glfwGetTime();
        double dt = now - lastTime;
        lastTime = now;
        accumulator += dt;
        while (accumulator >= step) {
            UpdateAnimationStep();
            accumulator -= step;
        }

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

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

#ifdef _WIN32
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    return main();
}
#endif
