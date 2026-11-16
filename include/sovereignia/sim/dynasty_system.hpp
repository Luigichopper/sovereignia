#pragma once

#include "sovereignia/core/clock.hpp"
#include "sovereignia/core/rng.hpp"
#include "sovereignia/data/config_registry.hpp"
#include "sovereignia/ecs/components.hpp"
#include "sovereignia/sim/family_tree.hpp"
#include "sovereignia/sim/history_log.hpp"
#include "sovereignia/world/map_generator.hpp"

#include <entt/entt.hpp>

#include <vector>

namespace sovereignia::sim {

class DynastySystem {
public:
    void refreshCourtOffices(entt::registry& registry,
                             const core::SimClock& clock,
                             HistoryLog& log);

    void tickDaily(entt::registry& registry,
                   const data::ConfigRegistry& config,
                   const core::SimClock& clock,
                   core::Rng& rng,
                   FamilyTree& family_tree,
                   HistoryLog& log);

    void tickMonthly(entt::registry& registry,
                     const data::ConfigRegistry& config,
                     const core::SimClock& clock,
                     core::Rng& rng,
                     FamilyTree& family_tree,
                     HistoryLog& log);

    void tickYearly(entt::registry& registry,
                    const data::ConfigRegistry& config,
                    const world::GeneratedWorldInfo& world_info,
                    const core::SimClock& clock,
                    core::Rng& rng,
                    FamilyTree& family_tree,
                    std::vector<ecs::MemorialEntry>& memorial_archive,
                    HistoryLog& log);
};

} // namespace sovereignia::sim

