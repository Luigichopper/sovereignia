#pragma once

#include "sovereignia/core/clock.hpp"
#include "sovereignia/core/rng.hpp"
#include "sovereignia/data/config_registry.hpp"
#include "sovereignia/sim/history_log.hpp"
#include "sovereignia/world/map_generator.hpp"
#include "sovereignia/world/world_map.hpp"

#include <entt/entt.hpp>

namespace sovereignia::sim {

struct DailyEconomyTotals {
    double grain_produced = 0.0;
    double fish_produced = 0.0;
    double firewood_produced = 0.0;
    double firewood_consumed = 0.0;
    double food_consumed = 0.0;
};

class EconomySystem {
public:
    static bool spawnCaravan(entt::registry& registry,
                             const world::WorldMap& map,
                             const data::ConfigRegistry& config,
                             entt::entity origin,
                             entt::entity destination,
                             CaravanCargoKind kind,
                             const std::array<double, kResourceCount>& cargo);

    bool startConstruction(entt::registry& registry,
                           const world::WorldMap& map,
                           entt::entity settlement,
                           BuildingType preferred,
                           const core::SimClock& clock,
                           HistoryLog& log,
                           bool allow_subsidy = false);

    DailyEconomyTotals tickDaily(entt::registry& registry,
                                 const world::WorldMap& map,
                                 const data::ConfigRegistry& config,
                                 const core::SimClock& clock,
                                 HistoryLog& log);

    void tickMonthly(entt::registry& registry,
                     world::WorldMap& map,
                     const data::ConfigRegistry& config,
                     const world::GeneratedWorldInfo& world_info,
                     const core::SimClock& clock,
                     HistoryLog& log);

    void tickSeasonal(entt::registry& registry,
                      const core::SimClock& clock,
                      core::Rng& rng,
                      HistoryLog& log);

    void tickYearly(entt::registry& registry,
                    const world::GeneratedWorldInfo& world_info,
                    const core::SimClock& clock,
                    HistoryLog& log);
};

} // namespace sovereignia::sim

