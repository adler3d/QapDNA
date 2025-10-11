#pragma once
#include <vector>
#include <random>
#include <cmath>

class MapGenerator {
public:
    static constexpr int SIZE = 80;
    static constexpr int HALF = 40;
    static constexpr int N = SIZE - 1;
    static constexpr double USE_PATHS_PROB = 0.75;

    enum class Symmetry { Mirror, Rotate };

    struct Options {
        bool usePerlin = false;
        bool forceSymmetry = false; // если true — использовать указанную симметрию
        Symmetry symmetry = Symmetry::Mirror;
    };

    MapGenerator();
    std::vector<int> generateMap(const Options& opts);

public:
    std::mt19937 rng_;

    // Perlin noise
    std::vector<double> generatePerlinNoise(int width, int height, double scale = 0.08);

    // Вспомогательные функции
    bool pointInPolygon(double x, double y, const std::vector<std::pair<double, double>>& poly);
    void drawPolygonBlob(std::vector<std::vector<int>>& quarter, int cx, int cy, int minR, int maxR, int sides);
    void drawOrganicPath(std::vector<std::vector<int>>& quarter, int startX, int startY);
    std::vector<int> applySymmetry(const std::vector<std::vector<int>>& quarter, Symmetry sym);
};