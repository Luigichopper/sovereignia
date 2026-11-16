#pragma once

#include "sovereignia/core/rng.hpp"
#include "sovereignia/data/config_registry.hpp"
#include "sovereignia/sim/family_tree.hpp"
#include "sovereignia/world/world_map.hpp"

#include <entt/entt.hpp>

#include <string>
#include <unordered_map>

namespace sovereignia::world {

struct GeneratedWorldInfo {
    entt::entity capital_entity = entt::null;
    entt::entity monastery_entity = entt::null;
    entt::entity south_pass_entity = entt::null;
    entt::entity empire_entity = entt::null;
    entt::entity rival_duchy_entity = entt::null;
    GridPos west_pass_entry{2, 20};
    std::unordered_map<std::string, entt::entity> settlement_by_id;
};

/// Generates the Northern Kingdom terrain, rivers, settlements, A* road network,
/// named rulers, off-map empire nodes, active laws, canonical texts, and family tree lineages.
GeneratedWorldInfo generateNorthernKingdom(WorldMap& map,
                                           entt::registry& registry,
                                           const data::ConfigRegistry& config,
                                           core::Rng& rng,
                                           sim::FamilyTree& family_tree,
                                           int start_year = 480);

} // namespace sovereignia::world

