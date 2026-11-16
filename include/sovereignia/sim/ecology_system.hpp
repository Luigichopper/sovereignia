#pragma once

#include "sovereignia/core/clock.hpp"
#include "sovereignia/core/rng.hpp"
#include "sovereignia/data/config_registry.hpp"
#include "sovereignia/ecs/components.hpp"
#include "sovereignia/sim/history_log.hpp"
#include "sovereignia/world/world_map.hpp"

#include <entt/entt.hpp>

namespace sovereignia::sim {

class EcologySystem {
public:
    void initializeMetrics(const world::WorldMap& map,
                           const data::ConfigRegistry& config,
                           ecs::EcologyState& state) const;

    void tickSeasonal(entt::registry& registry,
                      world::WorldMap& map,
                      const data::ConfigRegistry& config,
                      const core::SimClock& clock,
                      core::Rng& rng,
                      ecs::EcologyState& state,
                      HistoryLog& log);
};

} // namespace sovereignia::sim

