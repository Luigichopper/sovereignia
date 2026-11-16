#pragma once

#include "sovereignia/data/config_registry.hpp"
#include "sovereignia/world/world_map.hpp"

#include <vector>

namespace sovereignia::world {

/// Weighted 4-connected A* pathfinding across the provincial grid.
/// When for_road_construction is true, existing roads are strongly preferred so
/// regional spurs merge naturally into trunk highways and crossroads.
std::vector<GridPos> findPath(const WorldMap& map,
                              const data::ConfigRegistry& config,
                              GridPos start,
                              GridPos goal,
                              bool for_road_construction = false);

} // namespace sovereignia::world

