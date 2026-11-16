#pragma once

#include "sovereignia/core/clock.hpp"
#include "sovereignia/core/rng.hpp"
#include "sovereignia/data/config_registry.hpp"
#include "sovereignia/ecs/components.hpp"
#include "sovereignia/sim/history_log.hpp"
#include "sovereignia/world/map_generator.hpp"
#include "sovereignia/world/world_map.hpp"

#include <entt/entt.hpp>

#include <string>

namespace sovereignia::sim {

class GovernanceWarSystem {
public:
    entt::entity mobilizeBanner(entt::registry& registry,
                                const world::WorldMap& map,
                                const data::ConfigRegistry& config,
                                entt::entity origin_settlement,
                                entt::entity target_settlement,
                                const std::string& banner_name,
                                int max_levy_fraction_pct = 35);

    void tickDaily(entt::registry& registry,
                   world::WorldMap& map,
                   const data::ConfigRegistry& config,
                   const world::GeneratedWorldInfo& world_info,
                   const core::SimClock& clock,
                   core::Rng& rng,
                   HistoryLog& log);

    void tickSeasonal(entt::registry& registry,
                      const world::WorldMap& map,
                      const data::ConfigRegistry& config,
                      const world::GeneratedWorldInfo& world_info,
                      const core::SimClock& clock,
                      core::Rng& rng,
                      HistoryLog& log);

    bool issuePlayerEdict(entt::registry& registry,
                          const data::ConfigRegistry& config,
                          entt::entity seat_settlement,
                          const core::SimClock& clock,
                          HistoryLog& log);

    bool dispatchPlayerRetinue(entt::registry& registry,
                               const world::WorldMap& map,
                               const data::ConfigRegistry& config,
                               const world::GeneratedWorldInfo& world_info,
                               entt::entity seat_settlement,
                               const core::SimClock& clock,
                               HistoryLog& log);

    bool conductDiplomacy(entt::registry& registry,
                          const world::WorldMap& map,
                          const data::ConfigRegistry& config,
                          const world::GeneratedWorldInfo& world_info,
                          entt::entity seat_settlement,
                          const core::SimClock& clock,
                          core::Rng& rng,
                          HistoryLog& log);
};

} // namespace sovereignia::sim

