#pragma once

#include "sovereignia/core/clock.hpp"
#include "sovereignia/core/rng.hpp"
#include "sovereignia/data/config_registry.hpp"
#include "sovereignia/ecs/components.hpp"
#include "sovereignia/sim/history_log.hpp"
#include "sovereignia/world/map_generator.hpp"

#include <entt/entt.hpp>

namespace sovereignia::sim {

class TheologySystem {
public:
    void tickYearly(entt::registry& registry,
                    const data::ConfigRegistry& config,
                    const world::GeneratedWorldInfo& world_info,
                    const core::SimClock& clock,
                    core::Rng& rng,
                    ecs::ChurchState& church_state,
                    HistoryLog& log);

    void conveneSynod(entt::registry& registry,
                      const world::GeneratedWorldInfo& world_info,
                      const core::SimClock& clock,
                      ecs::ChurchState& church_state,
                      bool endorse_northern_rite,
                      HistoryLog& log);
};

} // namespace sovereignia::sim

