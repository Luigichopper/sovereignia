#pragma once

#include "sovereignia/core/clock.hpp"
#include "sovereignia/core/rng.hpp"
#include "sovereignia/data/config_registry.hpp"
#include "sovereignia/ecs/components.hpp"
#include "sovereignia/sim/dynasty_system.hpp"
#include "sovereignia/sim/ecology_system.hpp"
#include "sovereignia/sim/economy_system.hpp"
#include "sovereignia/sim/family_tree.hpp"
#include "sovereignia/sim/governance_war_system.hpp"
#include "sovereignia/sim/history_log.hpp"
#include "sovereignia/sim/theology_system.hpp"
#include "sovereignia/world/map_generator.hpp"
#include "sovereignia/world/world_map.hpp"

#include <entt/entt.hpp>

#include <cstdint>
#include <filesystem>
#include <vector>

namespace sovereignia::sim {

class SimulationEngine {
public:
    SimulationEngine();

    void loadData(const std::filesystem::path& data_dir);
    void initializeWorld(std::uint64_t seed = 482ULL,
                         int start_year = 480,
                         int map_width = world::WorldMap::kDefaultWidth,
                         int map_height = world::WorldMap::kDefaultHeight);
    void runPreHistory(int years);

    core::TickFlags tickDay();
    void advanceDays(int days);

    bool issueEdictAt(entt::entity seat_settlement);
    bool dispatchRetinueAt(entt::entity seat_settlement);
    bool commissionBuildingAt(entt::entity seat_settlement);
    bool conductDiplomacyAt(entt::entity seat_settlement);
    void convenePlayerSynod(bool endorse_northern_rite);
    ForestryPolicy cycleForestryPolicyAt(entt::entity seat_settlement);

    const data::ConfigRegistry& config() const noexcept { return config_; }
    const world::WorldMap& map() const noexcept { return map_; }
    world::WorldMap& map() noexcept { return map_; }
    const entt::registry& registry() const noexcept { return registry_; }
    entt::registry& registry() noexcept { return registry_; }
    const core::SimClock& clock() const noexcept { return clock_; }
    const HistoryLog& historyLog() const noexcept { return history_log_; }
    const world::GeneratedWorldInfo& worldInfo() const noexcept { return world_info_; }
    const ecs::ChurchState& churchState() const noexcept { return church_state_; }
    const ecs::EcologyState& ecologyState() const noexcept { return ecology_state_; }
    const FamilyTree& familyTree() const noexcept { return family_tree_; }
    FamilyTree& familyTree() noexcept { return family_tree_; }
    const std::vector<ecs::MemorialEntry>& memorialArchive() const noexcept { return memorial_archive_; }
    GovernanceWarSystem& governanceWar() noexcept { return governance_war_; }

    std::vector<entt::entity> orderedSettlements() const;
    int totalKingdomPopulation() const;

private:
    data::ConfigRegistry config_;
    world::WorldMap map_;
    entt::registry registry_;
    core::SimClock clock_;
    core::Rng rng_;
    HistoryLog history_log_;
    EconomySystem economy_;
    EcologySystem ecology_;
    FamilyTree family_tree_;
    DynastySystem dynasty_;
    GovernanceWarSystem governance_war_;
    TheologySystem theology_;
    ecs::ChurchState church_state_;
    ecs::EcologyState ecology_state_;
    std::vector<ecs::MemorialEntry> memorial_archive_;
    world::GeneratedWorldInfo world_info_;
    std::vector<entt::entity> ordered_settlements_;
    bool data_loaded_ = false;
};

} // namespace sovereignia::sim

