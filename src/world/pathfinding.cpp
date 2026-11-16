#include "sovereignia/world/pathfinding.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <queue>
#include <vector>

namespace sovereignia::world {

namespace {

struct QueueNode {
    double f = 0.0;
    double g = 0.0;
    int idx = 0;

    bool operator>(const QueueNode& other) const noexcept {
        return f > other.f;
    }
};

double cellStepCost(const MapCell& cell,
                    const data::ConfigRegistry& config,
                    bool for_road_construction) {
    const auto& biome = config.biomeByIndex(cell.biome_index);
    if (!biome.walkable) {
        return std::numeric_limits<double>::infinity();
    }

    double cost = biome.move_cost;
    if (cell.has_river && !cell.has_bridge && cell.road_level == RoadLevel::None) {
        cost += for_road_construction ? 1.35 : 0.65;
    }

    if (cell.road_level == RoadLevel::ImperialHighway) {
        cost *= for_road_construction ? 0.28 : 0.40;
    } else if (cell.road_level == RoadLevel::DirtRoad) {
        cost *= for_road_construction ? 0.35 : 0.60;
    }
    return cost;
}

} // namespace

std::vector<GridPos> findPath(const WorldMap& map,
                              const data::ConfigRegistry& config,
                              GridPos start,
                              GridPos goal,
                              bool for_road_construction) {
    if (!map.inBounds(start) || !map.inBounds(goal)) {
        return {};
    }
    if (start == goal) {
        return {start};
    }

    const int w = map.width();
    const int h = map.height();
    const int n = w * h;
    auto toIdx = [w](GridPos p) { return p.y * w + p.x; };
    auto fromIdx = [w](int idx) { return GridPos{idx % w, idx / w}; };

    const int start_idx = toIdx(start);
    const int goal_idx = toIdx(goal);

    constexpr double kInf = std::numeric_limits<double>::infinity();
    std::vector<double> g_score(static_cast<std::size_t>(n), kInf);
    std::vector<int> came_from(static_cast<std::size_t>(n), -1);
    std::vector<bool> closed(static_cast<std::size_t>(n), false);

    auto heuristic = [goal](GridPos p) {
        return 0.45 * static_cast<double>(std::abs(p.x - goal.x) + std::abs(p.y - goal.y));
    };

    std::priority_queue<QueueNode, std::vector<QueueNode>, std::greater<QueueNode>> open;
    g_score[static_cast<std::size_t>(start_idx)] = 0.0;
    open.push(QueueNode{heuristic(start), 0.0, start_idx});

    constexpr std::array<GridPos, 4> kDirs{{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}};

    while (!open.empty()) {
        const QueueNode current = open.top();
        open.pop();

        if (closed[static_cast<std::size_t>(current.idx)]) {
            continue;
        }
        if (current.idx == goal_idx) {
            std::vector<GridPos> path;
            for (int cur = goal_idx; cur != -1; cur = came_from[static_cast<std::size_t>(cur)]) {
                path.push_back(fromIdx(cur));
                if (cur == start_idx) {
                    break;
                }
            }
            std::reverse(path.begin(), path.end());
            return path;
        }

        closed[static_cast<std::size_t>(current.idx)] = true;
        const GridPos cp = fromIdx(current.idx);

        for (const auto d : kDirs) {
            const GridPos np{cp.x + d.x, cp.y + d.y};
            if (!map.inBounds(np)) {
                continue;
            }
            const int n_idx = toIdx(np);
            if (closed[static_cast<std::size_t>(n_idx)]) {
                continue;
            }

            const double step = cellStepCost(map.at(np), config, for_road_construction);
            if (!std::isfinite(step)) {
                continue;
            }

            const double tentative_g = current.g + step;
            if (tentative_g < g_score[static_cast<std::size_t>(n_idx)]) {
                came_from[static_cast<std::size_t>(n_idx)] = current.idx;
                g_score[static_cast<std::size_t>(n_idx)] = tentative_g;
                open.push(QueueNode{tentative_g + heuristic(np), tentative_g, n_idx});
            }
        }
    }

    return {};
}

} // namespace sovereignia::world

