#include "wind_tunnel/game/wind_tunnel_sim.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

// Smoke: one band in the middle, or a rake of thin streaks across the whole height.
constexpr float BAND_HEIGHT = 0.1f;
constexpr float RAKE_STREAKS = 15.0f;
constexpr float RAKE_STREAK_HEIGHT = 0.014f;

}  // namespace

bool Samples::WindTunnelSim::Init(uint32_t rows, float aspect) {
    if (rows < MIN_ROWS || !(aspect > 0.0f)) {
        return false;
    }
    numY = rows;
    h = DOMAIN_HEIGHT / static_cast<float>(rows);
    numX = std::max<uint32_t>(MIN_ROWS, static_cast<uint32_t>(std::lround(aspect * static_cast<float>(rows))));

    const size_t cells = static_cast<size_t>(numX) * numY;
    u.assign(cells, 0.0f);
    v.assign(cells, 0.0f);
    newU.assign(cells, 0.0f);
    newV.assign(cells, 0.0f);
    p.assign(cells, 0.0f);
    s.assign(cells, 1.0f);
    m.assign(cells, 0.0f);
    newM.assign(cells, 0.0f);
    curl.assign(cells, 0.0f);
    relax.assign(cells, 0.0f);
    ResetFlow();
    return true;
}

void Samples::WindTunnelSim::ResetFlow() {
    std::fill(u.begin(), u.end(), 0.0f);
    std::fill(v.begin(), v.end(), 0.0f);
    std::fill(p.begin(), p.end(), 0.0f);
    std::fill(m.begin(), m.end(), 0.0f);
    for (uint32_t i = 0; i < numX; ++i) {
        for (uint32_t j = 0; j < numY; ++j) {
            s[i * numY + j] = IsWall(i, j) ? 0.0f : 1.0f;
        }
    }
    obstacleVX = 0.0f;
    obstacleVY = 0.0f;
    MarkObstacle();
    FeedInflow();
}

void Samples::WindTunnelSim::Step(float dt) {
    if (numX == 0 || !(dt > 0.0f)) {
        return;
    }
    MarkObstacle();
    FeedInflow();
    if (vorticity > 0.0f) {
        ApplyVorticity();
    }
    Project(dt);
    MeasureForce();
    AdvectVelocity(dt);
    AdvectSmoke(dt);
}

void Samples::WindTunnelSim::Project(float dt) {
    std::fill(p.begin(), p.end(), 0.0f);
    SolveIncompressibility(dt);
    Extrapolate();
}

void Samples::WindTunnelSim::SetObstacle(float x, float y, float angle, float vx, float vy) {
    const float margin = 2.0f * h;
    obstacleX = std::clamp(x, margin, GetWidth() - margin);
    obstacleY = std::clamp(y, margin, DOMAIN_HEIGHT - margin);
    obstacleAngle = angle;
    obstacleVX = vx;
    obstacleVY = vy;
    MarkObstacle();
}

void Samples::WindTunnelSim::SetSmokeSource(SmokeSource source) {
    smokeSource = source;
    FeedInflow();
}

void Samples::WindTunnelSim::SetInflowSpeed(float speed) {
    inflowSpeed = speed;
    FeedInflow();
}

float Samples::WindTunnelSim::SampleU(float x, float y) const {
    return Sample(x, y, u, Field::U);
}

float Samples::WindTunnelSim::SampleV(float x, float y) const {
    return Sample(x, y, v, Field::V);
}

float Samples::WindTunnelSim::GetSpeed(uint32_t i, uint32_t j) const {
    const uint32_t i1 = std::min(i + 1, numX - 1);
    const uint32_t j1 = std::min(j + 1, numY - 1);
    const float cu = 0.5f * (u[i * numY + j] + u[i1 * numY + j]);
    const float cv = 0.5f * (v[i * numY + j] + v[i * numY + j1]);
    return std::sqrt(cu * cu + cv * cv);
}

float Samples::WindTunnelSim::MaxDivergence() const {
    float worst = 0.0f;
    for (uint32_t i = 1; i + 1 < numX; ++i) {
        for (uint32_t j = 1; j + 1 < numY; ++j) {
            if (s[i * numY + j] == 0.0f) {
                continue;
            }
            const float div = u[(i + 1) * numY + j] - u[i * numY + j] + v[i * numY + j + 1] - v[i * numY + j];
            worst = std::max(worst, std::fabs(div));
        }
    }
    return worst;
}

uint32_t Samples::WindTunnelSim::Checksum() const {
    // FNV-1a over the bit patterns.
    uint32_t hash = 2166136261u;
    const std::vector<float>* const fields[] = {&u, &v, &m};
    for (const std::vector<float>* field : fields) {
        for (float value : *field) {
            uint32_t bits = 0;
            std::memcpy(&bits, &value, sizeof(bits));
            for (int b = 0; b < 4; ++b) {
                hash ^= (bits >> (8 * b)) & 0xFFu;
                hash *= 16777619u;
            }
        }
    }
    return hash;
}

void Samples::WindTunnelSim::ApplyVorticity() {
    // Curl at cell centres, from the centre velocities around each cell.
    const uint32_t n = numY;
    auto centreU = [&](uint32_t i, uint32_t j) { return 0.5f * (u[i * n + j] + u[(i + 1) * n + j]); };
    auto centreV = [&](uint32_t i, uint32_t j) { return 0.5f * (v[i * n + j] + v[i * n + j + 1]); };
    const float inv2h = 0.5f / h;
    for (uint32_t i = 1; i + 2 < numX; ++i) {
        for (uint32_t j = 1; j + 2 < numY; ++j) {
            curl[i * n + j] = ((centreV(i + 1, j) - centreV(i - 1, j)) - (centreU(i, j + 1) - centreU(i, j - 1))) * inv2h;
        }
    }

    // Push along N x curl, where N points toward stronger swirl: it winds the vortices back up. The push is per step,
    // scaled by the cell size, so it looks alike at every resolution.
    const float scale = vorticity * h;
    for (uint32_t i = 2; i + 3 < numX; ++i) {
        for (uint32_t j = 2; j + 3 < numY; ++j) {
            const uint32_t c = i * n + j;
            if (s[c] == 0.0f) {
                continue;
            }
            const float gx = (std::fabs(curl[c + n]) - std::fabs(curl[c - n])) * inv2h;
            const float gy = (std::fabs(curl[c + 1]) - std::fabs(curl[c - 1])) * inv2h;
            const float length = std::sqrt(gx * gx + gy * gy) + 1e-5f;
            const float w = curl[c];
            const float fx = scale * (gy / length) * w;
            const float fy = -scale * (gx / length) * w;
            // Spread over the cell's faces, but never onto a solid's.
            if (s[c - n] != 0.0f) {
                u[c] += 0.5f * fx;
            }
            if (s[c + n] != 0.0f) {
                u[c + n] += 0.5f * fx;
            }
            if (s[c - 1] != 0.0f) {
                v[c] += 0.5f * fy;
            }
            if (s[c + 1] != 0.0f) {
                v[c + 1] += 0.5f * fy;
            }
        }
    }
}

void Samples::WindTunnelSim::SolveIncompressibility(float dt) {
    const uint32_t n = numY;

    // Each fluid cell's share of its divergence: overrelaxation over its open faces (0 skips the cell).
    for (uint32_t i = 1; i + 1 < numX; ++i) {
        for (uint32_t j = 1; j + 1 < numY; ++j) {
            const uint32_t c = i * n + j;
            const float sum = s[c - n] + s[c + n] + s[c - 1] + s[c + 1];
            relax[c] = s[c] != 0.0f && sum > 0.0f ? OVER_RELAXATION / sum : 0.0f;
        }
    }

    // Red-black Gauss-Seidel: cells of one colour share no faces, so a sweep over one colour has no chain of reads
    // after writes and the CPU can overlap them. Pressure sums the pushes and is scaled once at the end.
    for (uint32_t iter = 0; iter < PRESSURE_ITERATIONS; ++iter) {
        for (uint32_t colour = 0; colour < 2; ++colour) {
            for (uint32_t i = 1; i + 1 < numX; ++i) {
                for (uint32_t j = 1 + ((i + colour) & 1u); j + 1 < numY; j += 2) {
                    const uint32_t c = i * n + j;
                    const float k = relax[c];
                    if (k == 0.0f) {
                        continue;
                    }
                    const float push = -(u[c + n] - u[c] + v[c + 1] - v[c]) * k;
                    p[c] += push;
                    u[c] -= s[c - n] * push;
                    u[c + n] += s[c + n] * push;
                    v[c] -= s[c - 1] * push;
                    v[c + 1] += s[c + 1] * push;
                }
            }
        }
    }

    const float cp = DENSITY * h / dt;
    for (float& pressure : p) {
        pressure *= cp;
    }
}

void Samples::WindTunnelSim::Extrapolate() {
    const uint32_t n = numY;
    for (uint32_t i = 0; i < numX; ++i) {
        u[i * n] = u[i * n + 1];
        u[i * n + n - 1] = u[i * n + n - 2];
    }
    for (uint32_t j = 0; j < numY; ++j) {
        v[j] = v[n + j];
        v[(numX - 1) * n + j] = v[(numX - 2) * n + j];
    }
}

float Samples::WindTunnelSim::Sample(float x, float y, const std::vector<float>& field, Field kind) const {
    const uint32_t n = numY;
    const float h1 = 1.0f / h;
    const float h2 = 0.5f * h;
    x = std::clamp(x, h, static_cast<float>(numX) * h);
    y = std::clamp(y, h, static_cast<float>(numY) * h);

    float dx = 0.0f;
    float dy = 0.0f;
    switch (kind) {
        case Field::U:
            dy = h2;
            break;
        case Field::V:
            dx = h2;
            break;
        case Field::Cell:
            dx = h2;
            dy = h2;
            break;
    }

    const uint32_t x0 = std::min(static_cast<uint32_t>(std::floor((x - dx) * h1)), numX - 1);
    const float tx = ((x - dx) - static_cast<float>(x0) * h) * h1;
    const uint32_t x1 = std::min(x0 + 1, numX - 1);
    const uint32_t y0 = std::min(static_cast<uint32_t>(std::floor((y - dy) * h1)), numY - 1);
    const float ty = ((y - dy) - static_cast<float>(y0) * h) * h1;
    const uint32_t y1 = std::min(y0 + 1, numY - 1);
    const float sx = 1.0f - tx;
    const float sy = 1.0f - ty;

    return sx * sy * field[x0 * n + y0] + tx * sy * field[x1 * n + y0] + tx * ty * field[x1 * n + y1] +
           sx * ty * field[x0 * n + y1];
}

float Samples::WindTunnelSim::AverageU(uint32_t i, uint32_t j) const {
    const uint32_t n = numY;
    return 0.25f * (u[i * n + j - 1] + u[i * n + j] + u[(i + 1) * n + j - 1] + u[(i + 1) * n + j]);
}

float Samples::WindTunnelSim::AverageV(uint32_t i, uint32_t j) const {
    const uint32_t n = numY;
    return 0.25f * (v[(i - 1) * n + j] + v[i * n + j] + v[(i - 1) * n + j + 1] + v[i * n + j + 1]);
}

void Samples::WindTunnelSim::AdvectVelocity(float dt) {
    newU = u;
    newV = v;
    const uint32_t n = numY;
    const float h2 = 0.5f * h;
    for (uint32_t i = 1; i < numX; ++i) {
        for (uint32_t j = 1; j < numY; ++j) {
            const uint32_t c = i * n + j;
            if (s[c] != 0.0f && s[c - n] != 0.0f && j + 1 < numY) {
                const float x = static_cast<float>(i) * h - dt * u[c];
                const float y = static_cast<float>(j) * h + h2 - dt * AverageV(i, j);
                newU[c] = Sample(x, y, u, Field::U);
            }
            if (s[c] != 0.0f && s[c - 1] != 0.0f && i + 1 < numX) {
                const float x = static_cast<float>(i) * h + h2 - dt * AverageU(i, j);
                const float y = static_cast<float>(j) * h - dt * v[c];
                newV[c] = Sample(x, y, v, Field::V);
            }
        }
    }
    u.swap(newU);
    v.swap(newV);
}

void Samples::WindTunnelSim::AdvectSmoke(float dt) {
    newM = m;
    const uint32_t n = numY;
    const float h2 = 0.5f * h;
    for (uint32_t i = 1; i + 1 < numX; ++i) {
        for (uint32_t j = 1; j + 1 < numY; ++j) {
            const uint32_t c = i * n + j;
            if (s[c] == 0.0f) {
                continue;
            }
            const float cu = 0.5f * (u[c] + u[c + n]);
            const float cv = 0.5f * (v[c] + v[c + 1]);
            const float x = static_cast<float>(i) * h + h2 - dt * cu;
            const float y = static_cast<float>(j) * h + h2 - dt * cv;
            newM[c] = Sample(x, y, m, Field::Cell);
        }
    }
    m.swap(newM);
}

void Samples::WindTunnelSim::FeedInflow() {
    if (numX < 2) {
        return;
    }
    const uint32_t n = numY;
    for (uint32_t j = 1; j + 1 < numY; ++j) {
        u[n + j] = inflowSpeed;  // the face between the inlet column and the first fluid column
        const float y = (static_cast<float>(j) + 0.5f) * h;
        float smoke = 0.0f;
        if (smokeSource == SmokeSource::Band) {
            smoke = std::fabs(y - 0.5f * DOMAIN_HEIGHT) < 0.5f * BAND_HEIGHT ? 1.0f : 0.0f;
        } else {
            const float phase = y * RAKE_STREAKS - std::floor(y * RAKE_STREAKS);
            // At least one cell per streak, so a coarse grid keeps every streak.
            const float halfWidth = 0.5f * std::max(RAKE_STREAK_HEIGHT, h) * RAKE_STREAKS;
            smoke = std::fabs(phase - 0.5f) < halfWidth ? 1.0f : 0.0f;
        }
        m[j] = smoke;
    }
}


void Samples::WindTunnelSim::SetOutline(const Outline& newOutline, float newReferenceLength) {
    outline = newOutline;
    referenceLength = newReferenceLength > 0.0f ? newReferenceLength : 1.0f;
    MarkObstacle();
}

uint32_t Samples::WindTunnelSim::CountObstacleCells() const {
    uint32_t count = 0;
    for (uint32_t i = 1; i + 1 < numX; ++i) {
        for (uint32_t j = 1; j + 1 < numY; ++j) {
            count += s[i * numY + j] == 0.0f ? 1u : 0u;
        }
    }
    return count;
}

void Samples::WindTunnelSim::MarkObstacle() {
    if (numX < 3) {
        return;
    }
    const uint32_t n = numY;
    for (uint32_t i = 1; i + 1 < numX; ++i) {
        for (uint32_t j = 1; j + 1 < numY; ++j) {
            s[i * n + j] = 1.0f;
        }
    }

    const float c = std::cos(obstacleAngle);
    const float sn = std::sin(obstacleAngle);
    const float invH = 1.0f / h;
    for (const std::vector<Point>& polygon : outline) {
        if (polygon.size() < 3) {
            continue;
        }
        // Into domain units, and the rows the polygon spans.
        placed.clear();
        float low = 1e30f;
        float high = -1e30f;
        for (const Point& local : polygon) {
            const Point world{obstacleX + c * local.x - sn * local.y, obstacleY + sn * local.x + c * local.y};
            low = std::min(low, world.y);
            high = std::max(high, world.y);
            placed.push_back(world);
        }
        const int firstRow = std::max(1, static_cast<int>(std::ceil(low * invH - 0.5f)));
        const int lastRow = std::min(static_cast<int>(numY) - 2, static_cast<int>(std::floor(high * invH - 0.5f)));

        // Each row through cell centres: fill between pairs of edge crossings (even-odd).
        for (int j = firstRow; j <= lastRow; ++j) {
            const float y = (static_cast<float>(j) + 0.5f) * h;
            crossings.clear();
            for (size_t k = 0; k < placed.size(); ++k) {
                const Point& a = placed[k];
                const Point& b = placed[(k + 1) % placed.size()];
                if ((a.y > y) != (b.y > y)) {
                    crossings.push_back(a.x + (y - a.y) * (b.x - a.x) / (b.y - a.y));
                }
            }
            std::sort(crossings.begin(), crossings.end());
            for (size_t k = 0; k + 1 < crossings.size(); k += 2) {
                const int first = std::max(1, static_cast<int>(std::ceil(crossings[k] * invH - 0.5f)));
                const int last =
                    std::min(static_cast<int>(numX) - 2, static_cast<int>(std::ceil(crossings[k + 1] * invH - 0.5f)) - 1);
                for (int i = first; i <= last; ++i) {
                    FillCell(static_cast<uint32_t>(i), static_cast<uint32_t>(j));
                }
            }
        }
    }
}

void Samples::WindTunnelSim::FillCell(uint32_t i, uint32_t j) {
    const uint32_t n = numY;
    const uint32_t c = i * n + j;
    s[c] = 0.0f;
    m[c] = 0.0f;
    u[c] = obstacleVX;
    u[c + n] = obstacleVX;
    v[c] = obstacleVY;
    v[c + 1] = obstacleVY;
}

void Samples::WindTunnelSim::MeasureForce() {
    // Pressure on every face between the obstacle and the fluid, pushing into the obstacle.
    const uint32_t n = numY;
    float fx = 0.0f;
    float fy = 0.0f;
    for (uint32_t i = 1; i + 1 < numX; ++i) {
        for (uint32_t j = 1; j + 1 < numY; ++j) {
            const uint32_t c = i * n + j;
            if (s[c] != 0.0f) {
                continue;
            }
            if (s[c - n] != 0.0f) {
                fx += p[c - n];
            }
            if (s[c + n] != 0.0f) {
                fx -= p[c + n];
            }
            if (s[c - 1] != 0.0f) {
                fy += p[c - 1];
            }
            if (s[c + 1] != 0.0f) {
                fy -= p[c + 1];
            }
        }
    }
    const float dynamic = 0.5f * DENSITY * inflowSpeed * inflowSpeed * referenceLength;
    lift = dynamic > 0.0f ? fy * h / dynamic : 0.0f;
    drag = dynamic > 0.0f ? fx * h / dynamic : 0.0f;
}
