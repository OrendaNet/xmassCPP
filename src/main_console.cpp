#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>

struct Light {
    int row = 0;
    int col = 0; // offset in [-row, row]
    int colorA = 31;
    int colorB = 33;
    bool on = true;
};

static void ClearScreen() {
    std::cout << "\x1b[2J\x1b[H";
}

static std::string Ansi(int code) {
    return "\x1b[" + std::to_string(code) + "m";
}

static void RenderTree(int height, const std::vector<Light>& lights, const std::vector<std::vector<size_t>>& lightsByRow) {
    const std::string reset = Ansi(0);
    const std::string green = Ansi(32);
    const std::string brightGreen = Ansi(92);
    const std::string yellow = Ansi(33);
    const std::string brown = Ansi(33) + Ansi(2);

    std::cout << std::string(height, ' ') << yellow << "*" << reset << "\n";

    for (int row = 0; row < height; ++row) {
        int indent = height - row;
        std::cout << std::string(indent, ' ');

        for (int col = -row; col <= row; ++col) {
            const Light* light = nullptr;
            for (size_t idx : lightsByRow[row]) {
                if (lights[idx].col == col) {
                    light = &lights[idx];
                    break;
                }
            }

            if (light) {
                int c = light->on ? light->colorA : light->colorB;
                std::cout << Ansi(c) << "o" << reset;
            } else {
                std::cout << ((row % 2 == 0) ? brightGreen : green) << "^" << reset;
            }
        }
        std::cout << "\n";
    }

    int trunkHeight = std::max(3, height / 5);
    int trunkWidth = 3;
    int indent = height - 1;
    for (int i = 0; i < trunkHeight; ++i) {
        std::cout << std::string(indent, ' ') << brown << std::string(trunkWidth, '#') << reset << "\n";
    }
}

int main() {
    const int height = 20;
    const int lightCount = 60;

    std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> rowDist(2, height - 2);
    std::uniform_int_distribution<int> toggleDist(0, 2);
    std::array<int, 5> palette = {31, 33, 34, 35, 97}; // red, yellow, blue, magenta, white

    std::vector<Light> lights;
    lights.reserve(lightCount);
    for (int i = 0; i < lightCount; ++i) {
        int row = rowDist(rng);
        std::uniform_int_distribution<int> colDist(-row, row);
        int col = colDist(rng);

        Light l;
        l.row = row;
        l.col = col;
        l.colorA = palette[i % palette.size()];
        l.colorB = palette[(i + 2) % palette.size()];
        l.on = (i % 2 == 0);
        lights.push_back(l);
    }

    std::vector<std::vector<size_t>> lightsByRow(height);
    for (size_t i = 0; i < lights.size(); ++i) {
        if (lights[i].row >= 0 && lights[i].row < height) {
            lightsByRow[lights[i].row].push_back(i);
        }
    }

    int phase = 0;
    while (true) {
        if (phase % 5 == 0) {
            for (auto& l : lights) {
                if (toggleDist(rng) == 0) {
                    l.on = !l.on;
                }
            }
        }

        ClearScreen();
        std::cout << "Xmass Tree (console edition) - Ctrl+C to exit\n\n";
        RenderTree(height, lights, lightsByRow);

        std::this_thread::sleep_for(std::chrono::milliseconds(120));
        ++phase;
    }
}
