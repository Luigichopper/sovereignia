#pragma once

#include "sovereignia/core/types.hpp"

#include <entt/entt.hpp>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace sovereignia::world {

struct MapCell {
    int biome_index = 0;
    double elevation = 0.3;          // [0.0, 1.0]
    double moisture = 0.5;           // [0.0, 1.0]
    double temperature = 0.5;        // [0.0, 1.0]
    double forest_cover = 0.0;       // [0.0, 1.0]
    double soil_fertility = 1.0;     // [0.5, 1.4]
    double mana_density = 0.4;       // [0.0, 2.0]
    bool has_river = false;
    bool has_bridge = false;
    std::uint8_t river_width = 0;    // 0 = none, 1 = stream, 2 = major river
    std::uint8_t nearest_settlement_dist = 255;
    RoadLevel road_level = RoadLevel::None;
    entt::entity settlement_entity = entt::null;
    MapSiteKind site_kind = MapSiteKind::None;
    std::string site_name;
    std::string region_name;
};

class WorldMap {
public:
    static constexpr int kDefaultWidth = 108;
    static constexpr int kDefaultHeight = 48;

    WorldMap(int width = kDefaultWidth, int height = kDefaultHeight);

    int width() const noexcept { return width_; }
    int height() const noexcept { return height_; }

    bool inBounds(int x, int y) const noexcept {
        return x >= 0 && x < width_ && y >= 0 && y < height_;
    }

    bool inBounds(GridPos p) const noexcept { return inBounds(p.x, p.y); }

    MapCell& at(int x, int y);
    const MapCell& at(int x, int y) const;
    MapCell& at(GridPos p) { return at(p.x, p.y); }
    const MapCell& at(GridPos p) const { return at(p.x, p.y); }

    void resize(int width, int height);

    double localForestCover(GridPos center, int radius = 5) const noexcept;

    void cacheRoute(GridPos a, GridPos b, std::vector<GridPos> path);
    const std::vector<GridPos>* findCachedRoute(GridPos a, GridPos b) const noexcept;
    std::size_t cachedRouteCount() const noexcept { return route_cache_.size(); }

private:
    static std::uint64_t packRouteKey(GridPos a, GridPos b) noexcept;

    int width_ = kDefaultWidth;
    int height_ = kDefaultHeight;
    std::vector<MapCell> cells_;
    std::unordered_map<std::uint64_t, std::vector<GridPos>> route_cache_;
};

} // namespace sovereignia::world

