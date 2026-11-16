#include "sovereignia/world/world_map.hpp"

#include <stdexcept>

namespace sovereignia::world {

WorldMap::WorldMap(int width, int height) {
    resize(width, height);
}

void WorldMap::resize(int width, int height) {
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("WorldMap dimensions must be positive");
    }
    width_ = width;
    height_ = height;
    cells_.assign(static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_), MapCell{});
    route_cache_.clear();
}

std::uint64_t WorldMap::packRouteKey(GridPos a, GridPos b) noexcept {
    return (static_cast<std::uint64_t>(static_cast<std::uint16_t>(a.x)) << 48U) |
           (static_cast<std::uint64_t>(static_cast<std::uint16_t>(a.y)) << 32U) |
           (static_cast<std::uint64_t>(static_cast<std::uint16_t>(b.x)) << 16U) |
           static_cast<std::uint64_t>(static_cast<std::uint16_t>(b.y));
}

void WorldMap::cacheRoute(GridPos a, GridPos b, std::vector<GridPos> path) {
    route_cache_[packRouteKey(a, b)] = std::move(path);
}

const std::vector<GridPos>* WorldMap::findCachedRoute(GridPos a, GridPos b) const noexcept {
    const auto it = route_cache_.find(packRouteKey(a, b));
    if (it == route_cache_.end()) {
        return nullptr;
    }
    return &it->second;
}

MapCell& WorldMap::at(int x, int y) {
    if (!inBounds(x, y)) {
        throw std::out_of_range("WorldMap coordinate out of bounds");
    }
    return cells_[static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) + static_cast<std::size_t>(x)];
}

const MapCell& WorldMap::at(int x, int y) const {
    if (!inBounds(x, y)) {
        throw std::out_of_range("WorldMap coordinate out of bounds");
    }
    return cells_[static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) + static_cast<std::size_t>(x)];
}

double WorldMap::localForestCover(GridPos center, int radius) const noexcept {
    double sum = 0.0;
    int count = 0;
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            const GridPos p{center.x + dx, center.y + dy};
            if (!inBounds(p)) {
                continue;
            }
            const auto& cell = at(p);
            if (cell.elevation > 0.08 && !cell.has_river) {
                sum += cell.forest_cover;
                ++count;
            }
        }
    }
    return count > 0 ? (sum / static_cast<double>(count)) : 0.0;
}

} // namespace sovereignia::world

