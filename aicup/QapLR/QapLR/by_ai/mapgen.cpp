#include "mapgen.h"
#include <algorithm>
#include <iostream>

// --- Вспомогательные функции Perlin ---
static double fade(double t) {
    return t * t * t * (t * (t * 6 - 15) + 10);
}
static double lerp(double a, double b, double t) {
    return a + t * (b - a);
}
static double grad(int hash, double x, double y) {
    int h = hash & 7;
    double u = (h < 4) ? x : y;
    double v = (h < 4) ? y : x;
    return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
}

MapGenerator::MapGenerator() {
    std::random_device rd;
    rng_ = std::mt19937(rd());
}

std::vector<double> MapGenerator::generatePerlinNoise(int width, int height, double scale) {
    // Генерация перестановки
    std::vector<int> perm(512);
    for (int i = 0; i < 256; i++) perm[i] = i;
    for (int i = 255; i > 0; i--) {
        std::uniform_int_distribution<int> dist(0, i);
        int j = dist(rng_);
        std::swap(perm[i], perm[j]);
    }
    for (int i = 0; i < 256; i++) perm[i + 256] = perm[i];

    std::vector<double> noise(width * height);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            double xf = x * scale;
            double yf = y * scale;

            int xi = static_cast<int>(std::floor(xf)) & 255;
            int yi = static_cast<int>(std::floor(yf)) & 255;
            double tx = xf - std::floor(xf);
            double ty = yf - std::floor(yf);

            double u = fade(tx);
            double v = fade(ty);

            int a = perm[xi] + yi;
            int aa = perm[a];
            int ab = perm[a + 1];
            int b = perm[xi + 1] + yi;
            int ba = perm[b];
            int bb = perm[b + 1];

            double x1 = lerp(grad(perm[aa], tx, ty), grad(perm[ba], tx - 1, ty), u);
            double x2 = lerp(grad(perm[ab], tx, ty - 1), grad(perm[bb], tx - 1, ty - 1), u);
            double val = (lerp(x1, x2, v) + 1.0) / 2.0;
            noise[y * width + x] = val;
        }
    }
    return noise;
}

bool MapGenerator::pointInPolygon(double x, double y, const std::vector<std::pair<double, double>>& poly) {
    bool inside = false;
    size_t n = poly.size();
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        double xi = poly[i].first, yi = poly[i].second;
        double xj = poly[j].first, yj = poly[j].second;
        bool intersect = ((yi > y) != (yj > y)) &&
                         (x < (xj - xi) * (y - yi) / (yj - yi) + xi);
        if (intersect) inside = !inside;
    }
    return inside;
}

void MapGenerator::drawPolygonBlob(std::vector<std::vector<int>>& quarter, int cx, int cy, int minR, int maxR, int sides) {
    std::vector<double> angles;
    std::uniform_real_distribution<double> angleDist(0.0, 2.0 * Pi);
    for (int i = 0; i < sides; i++) {
        double base = 2.0 * Pi * i / sides;
        double noise = (std::uniform_real_distribution<double>(-0.4, 0.4)(rng_));
        angles.push_back(base + noise);
    }
    std::sort(angles.begin(), angles.end());

    std::vector<std::pair<double, double>> poly;
    std::uniform_real_distribution<double> rDist(minR, maxR);
    for (double a : angles) {
        double r = rDist(rng_);
        poly.emplace_back(cx + std::cos(a) * r, cy + std::sin(a) * r);
    }

    double minX = poly[0].first, maxX = poly[0].first;
    double minY = poly[0].second, maxY = poly[0].second;
    for (const auto& p : poly) {
        minX = std::min(minX, p.first);
        maxX = std::max(maxX, p.first);
        minY = std::min(minY, p.second);
        maxY = std::max(maxY, p.second);
    }

    int iMinX = std::max(0, static_cast<int>(std::floor(minX)) - 1);
    int iMaxX = std::min(HALF - 1, static_cast<int>(std::ceil(maxX)) + 1);
    int iMinY = std::max(0, static_cast<int>(std::floor(minY)) - 1);
    int iMaxY = std::min(HALF - 1, static_cast<int>(std::ceil(maxY)) + 1);

    for (int y = iMinY; y <= iMaxY; y++) {
        for (int x = iMinX; x <= iMaxX; x++) {
            if (pointInPolygon(x + 0.5, y + 0.5, poly)) {
                if (x >= 0 && x < HALF && y >= 0 && y < HALF) {
                    quarter[y][x] = 1;
                }
            }
        }
    }

    // Рваные края
    if (std::uniform_real_distribution<double>(0.0, 1.0)(rng_) < 0.5) {
        for (int y = iMinY; y <= iMaxY; y++) {
            for (int x = iMinX; x <= iMaxX; x++) {
                if (x < 0 || x >= HALF || y < 0 || y >= HALF) continue;
                if (quarter[y][x] == 1) {
                    int neighbors = 0;
                    for (int dy = -1; dy <= 1; dy++) {
                        for (int dx = -1; dx <= 1; dx++) {
                            int ny = y + dy, nx = x + dx;
                            if (ny >= 0 && ny < HALF && nx >= 0 && nx < HALF && quarter[ny][nx] == 1)
                                neighbors++;
                        }
                    }
                    if (neighbors < 5 && std::uniform_real_distribution<double>(0.0, 1.0)(rng_) < 0.2) {
                        quarter[y][x] = 0;
                    }
                }
            }
        }
    }
}

void MapGenerator::drawOrganicPath(std::vector<std::vector<int>>& quarter, int startX, int startY) {
    int x = startX, y = startY;
    std::uniform_int_distribution<int> stepsDist(15, 29);
    int steps = stepsDist(rng_);
    std::uniform_real_distribution<double> rand01(0.0, 1.0);
    std::uniform_int_distribution<int> dirDist(0, 3);
    const int dx4[4] = {0, 1, 0, -1};
    const int dy4[4] = {1, 0, -1, 0};

    for (int i = 0; i < steps; i++) {
        if (x >= 0 && x < HALF && y >= 0 && y < HALF) {
            quarter[y][x] = 0;
        }
        int dx = 0, dy = 0;
        if (x < HALF - 5 && rand01(rng_) < 0.6) dx = 1;
        else if (x > 5) dx = -1;
        if (y < HALF - 5 && rand01(rng_) < 0.6) dy = 1;
        else if (y > 5) dy = -1;

        if (rand01(rng_) < 0.3) {
            int d = dirDist(rng_);
            x += dx4[d];
            y += dy4[d];
        } else {
            x += dx;
            y += dy;
        }
        if (x < 0 || x >= HALF || y < 0 || y >= HALF) break;
    }
}

std::vector<int> MapGenerator::applySymmetry(const std::vector<std::vector<int>>& quarter, Symmetry sym) {
    std::vector<int> grid(SIZE * SIZE, 0);
    if (sym == Symmetry::Mirror) {
        for (int y = 0; y < HALF; y++) {
            for (int x = 0; x < HALF; x++) {
                int v = quarter[y][x];
                grid[y * SIZE + x] = v;
                grid[y * SIZE + (N - x)] = v;
                grid[(N - y) * SIZE + x] = v;
                grid[(N - y) * SIZE + (N - x)] = v;
            }
        }
    } else { // Rotate
        for (int y = 0; y < HALF; y++) {
            for (int x = 0; x < HALF; x++) {
                int v = quarter[y][x];
                grid[y * SIZE + x] = v;
                grid[(N - x) * SIZE + y] = v;
                grid[(N - y) * SIZE + (N - x)] = v;
                grid[x * SIZE + (N - y)] = v;
            }
        }
    }
    return grid;
}

std::vector<int> MapGenerator::generateMap(const Options& opts) {
    std::vector<std::vector<int>> quarter(HALF, std::vector<int>(HALF, 0));

    // Perlin noise
    if (opts.usePerlin) {
        auto perlin = generatePerlinNoise(HALF, HALF, 0.08);
        for (int y = 0; y < HALF; y++) {
            for (int x = 0; x < HALF; x++) {
                if (perlin[y * HALF + x] > 0.62) {
                    quarter[y][x] = 1;
                }
            }
        }
    }

    // Многоугольные острова
    std::uniform_int_distribution<int> clustersDist(8, 17);
    int clusters = clustersDist(rng_);
    std::uniform_int_distribution<int> coordDist(0, HALF - 1);
    for (int i = 0; i < clusters; i++) {
        int cx = coordDist(rng_);
        int cy = coordDist(rng_);
        if (std::abs(cx - cy) < 4 && std::uniform_real_distribution<double>(0.0, 1.0)(rng_) < 0.6) continue;
        int minR = 2 + std::uniform_int_distribution<int>(0, 2)(rng_);
        int maxR = minR + 3 + std::uniform_int_distribution<int>(0, 3)(rng_);
        int sides = 5 + std::uniform_int_distribution<int>(0, 4)(rng_);
        drawPolygonBlob(quarter, cx, cy, minR, maxR, sides);
    }

    // Фоновый шум
    for (int y = 0; y < HALF; y++) {
        for (int x = 0; x < HALF; x++) {
            if (std::uniform_real_distribution<double>(0.0, 1.0)(rng_) < 0.025) {
                quarter[y][x] = 1 - quarter[y][x];
            }
        }
    }

    // Тропинки (75%)
    if (std::uniform_real_distribution<double>(0.0, 1.0)(rng_) < USE_PATHS_PROB) {
        std::uniform_int_distribution<int> centerDist(18, 22);
        int cx = centerDist(rng_);
        int cy = centerDist(rng_);
        int paths = 1 + (std::uniform_real_distribution<double>(0.0, 1.0)(rng_) < 0.3 ? 1 : 0);
        for (int p = 0; p < paths; p++) {
            drawOrganicPath(quarter, cx, cy);
        }
    }

    // Выбор симметрии
    Symmetry sym;
    if (opts.forceSymmetry) {
        sym = opts.symmetry;
    } else {
        sym = (std::uniform_real_distribution<double>(0.0, 1.0)(rng_) < 0.5) ? Symmetry::Mirror : Symmetry::Rotate;
    }

    return applySymmetry(quarter, sym);
}