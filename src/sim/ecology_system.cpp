#include "sovereignia/sim/ecology_system.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <vector>

namespace sovereignia::sim {

namespace {

double policyDepletionMultiplier(ForestryPolicy policy) noexcept {
    switch (policy) {
        case ForestryPolicy::BalancedCoppice: return 0.85;
        case ForestryPolicy::IntensiveFelling: return 1.35;
        case ForestryPolicy::WardenReforestation: return 0.38;
    }
    return 0.85;
}

double policyFloor(ForestryPolicy policy) noexcept {
    switch (policy) {
        case ForestryPolicy::BalancedCoppice: return 0.16;
        case ForestryPolicy::IntensiveFelling: return 0.05;
        case ForestryPolicy::WardenReforestation: return 0.28;
    }
    return 0.16;
}

void computeCanopyStats(const world::WorldMap& map,
                        const data::ConfigRegistry& config,
                        ecs::EcologyState& state) {
    int forested = 0;
    int old_growth = 0;
    int capable_count = 0;
    double canopy_sum = 0.0;

    for (int y = 0; y < map.height(); ++y) {
        for (int x = 0; x < map.width(); ++x) {
            const auto& cell = map.at(x, y);
            const auto& biome = config.biomeByIndex(cell.biome_index);
            if (!biome.forest_capable) {
                continue;
            }
            ++capable_count;
            canopy_sum += cell.forest_cover;
            if (cell.forest_cover >= 0.30) {
                ++forested;
            }
            if (cell.forest_cover >= 0.85) {
                ++old_growth;
            }
        }
    }

    state.forested_tiles = forested;
    state.old_growth_tiles = old_growth;
    state.average_canopy = (capable_count > 0) ? (canopy_sum / static_cast<double>(capable_count)) : 0.0;
}

} // namespace

void EcologySystem::initializeMetrics(const world::WorldMap& map,
                                      const data::ConfigRegistry& config,
                                      ecs::EcologyState& state) const {
    state = ecs::EcologyState{};
    computeCanopyStats(map, config, state);
    state.initial_forested_tiles = state.forested_tiles;
}

void EcologySystem::tickSeasonal(entt::registry& registry,
                                 world::WorldMap& map,
                                 const data::ConfigRegistry& config,
                                 const core::SimClock& clock,
                                 core::Rng& rng,
                                 ecs::EcologyState& state,
                                 HistoryLog& log) {
    const int w = map.width();
    const int h = map.height();
    std::vector<double> old_covers(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), 0.0);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            old_covers[static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x)] =
                map.at(x, y).forest_cover;
        }
    }

    const int b_plain = config.biomeIndex("fertile_plain");
    const int b_meadow = config.biomeIndex("cleared_meadow");
    const int b_young = config.biomeIndex("young_woodland");
    const int b_forest = config.biomeIndex("boreal_forest");
    const int b_old = config.biomeIndex("old_growth_taiga");
    const int b_hills = config.biomeIndex("taiga_hills");

    // A. Slow Settlement Felling, Charcoal Depletion & Warden Planting
    auto s_view = registry.view<ecs::NameComponent,
                                ecs::SettlementComponent,
                                ecs::GridPosComponent,
                                ecs::DemographicsComponent>();

    for (const entt::entity s_ent : s_view) {
        const auto& s_name = s_view.get<ecs::NameComponent>(s_ent).value;
        auto& sett = s_view.get<ecs::SettlementComponent>(s_ent);
        const GridPos pos = s_view.get<ecs::GridPosComponent>(s_ent).pos;
        const auto& demo = s_view.get<ecs::DemographicsComponent>(s_ent);

        // Ruler Ecological Adaptation once per year in Spring
        if (clock.season() == Season::Spring) {
            std::string ruler_trait = "Pragmatic";
            if (sett.ruler_entity != entt::null && registry.valid(sett.ruler_entity) &&
                registry.all_of<ecs::CharacterComponent>(sett.ruler_entity)) {
                ruler_trait = registry.get<ecs::CharacterComponent>(sett.ruler_entity).trait;
            }
            const double cur_wood = map.localForestCover(pos, 5);
            if (cur_wood < 0.24 && sett.forestry_policy != ForestryPolicy::WardenReforestation &&
                ruler_trait != "Scheming") {
                sett.forestry_policy = ForestryPolicy::WardenReforestation;
                log.add(clock.year(), clock.season(), clock.month(), clock.day(),
                        LogCategory::Ecology,
                        std::format("{} enacted Warden Reforestation after local woods thinned to {:.0f}%.",
                                    s_name, cur_wood * 100.0));
            } else if (cur_wood > 0.54 && sett.forestry_policy == ForestryPolicy::WardenReforestation &&
                       sett.type == SettlementType::LumberCamp) {
                sett.forestry_policy = ForestryPolicy::BalancedCoppice;
                log.add(clock.year(), clock.season(), clock.month(), clock.day(),
                        LogCategory::Ecology,
                        std::format("{} returned to Balanced Coppice as canopy recovered to {:.0f}%.",
                                    s_name, cur_wood * 100.0));
            }
        }

        const int lumberjacks = demo.countOf(CohortType::Lumberjacks);
        const int miners = demo.countOf(CohortType::Miners);
        const int total_pop = demo.totalPopulation();

        const double pressure = (static_cast<double>(lumberjacks) * 0.00020 +
                                 static_cast<double>(miners) * 0.00018 +
                                 static_cast<double>(total_pop) * 0.000025)
                                * policyDepletionMultiplier(sett.forestry_policy);
        const double min_floor = policyFloor(sett.forestry_policy);

        struct CandCell {
            GridPos p;
            double weight;
        };
        std::vector<CandCell> cut_cands;
        std::vector<CandCell> plant_cands;

        for (int dy = -6; dy <= 6; ++dy) {
            for (int dx = -6; dx <= 6; ++dx) {
                if (dx == 0 && dy == 0) {
                    continue;
                }
                const GridPos cp{pos.x + dx, pos.y + dy};
                if (!map.inBounds(cp)) {
                    continue;
                }
                const double dist = std::hypot(static_cast<double>(dx), static_cast<double>(dy));
                if (dist > 6.2) {
                    continue;
                }
                const auto& cell = map.at(cp);
                const auto& biome = config.biomeByIndex(cell.biome_index);
                if (!biome.forest_capable || cell.settlement_entity != entt::null) {
                    continue;
                }
                if (cell.site_kind == MapSiteKind::SacredGrove) {
                    continue;
                }

                if (cell.forest_cover > min_floor) {
                    cut_cands.push_back(CandCell{cp, 1.0 / (dist + 0.6)});
                }
                if (sett.forestry_policy == ForestryPolicy::WardenReforestation &&
                    dist <= 5.2 && cell.forest_cover < 0.72) {
                    plant_cands.push_back(CandCell{cp, dist});
                }
            }
        }

        if (!cut_cands.empty() && pressure > 0.0) {
            double total_weight = 0.0;
            for (const auto& c : cut_cands) {
                total_weight += c.weight;
            }
            const double cut_budget = std::min(0.45, pressure * 2.2);
            if (total_weight > 0.0) {
                for (const auto& c : cut_cands) {
                    auto& cell = map.at(c.p);
                    const double share = cut_budget * (c.weight / total_weight);
                    const double actual = std::min(0.009, share);
                    cell.forest_cover = std::max(min_floor, cell.forest_cover - actual);
                }
            }
        }

        if (!plant_cands.empty() && sett.forestry_policy == ForestryPolicy::WardenReforestation) {
            std::sort(plant_cands.begin(), plant_cands.end(), [](const CandCell& a, const CandCell& b) {
                return a.weight < b.weight;
            });
            const std::size_t limit = std::min<std::size_t>(12, plant_cands.size());
            for (std::size_t i = 0; i < limit; ++i) {
                auto& cell = map.at(plant_cands[i].p);
                cell.forest_cover = std::min(0.75, cell.forest_cover + 0.0045);
            }
        }
    }

    // B. Natural Succession, Regrowth, Soil Feedback & Hysteresis Biome Transitions
    constexpr std::array<GridPos, 8> kNbrs{{
        {-1, -1}, {0, -1}, {1, -1},
        {-1,  0},          {1,  0},
        {-1,  1}, {0,  1}, {1,  1}
    }};

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            auto& cell = map.at(x, y);
            const auto& biome = config.biomeByIndex(cell.biome_index);
            const std::size_t idx = static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x);
            const double old_cover = old_covers[idx];

            if (!biome.forest_capable || cell.settlement_entity != entt::null) {
                continue;
            }

            int seed_neighbors = 0;
            for (const auto& d : kNbrs) {
                const GridPos np{x + d.x, y + d.y};
                if (!map.inBounds(np)) {
                    continue;
                }
                const std::size_t n_idx = static_cast<std::size_t>(np.y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(np.x);
                if (old_covers[n_idx] >= 0.32) {
                    ++seed_neighbors;
                }
            }

            const double max_canopy = (cell.site_kind == MapSiteKind::SacredGrove)
                ? 0.96
                : std::clamp(cell.moisture * 1.18 * (1.06 - 0.46 * cell.elevation), 0.18, 0.96);

            if (cell.forest_cover < max_canopy && (cell.forest_cover > 0.10 || seed_neighbors >= 2)) {
                double growth = 0.0;
                if (cell.nearest_settlement_dist <= 2) {
                    growth = 0.0003;
                } else {
                    growth = (0.0011 + 0.00045 * static_cast<double>(seed_neighbors))
                           * (0.5 + 0.7 * cell.moisture);
                }
                cell.forest_cover = std::min(max_canopy, cell.forest_cover + growth);
            }

            if (cell.elevation >= 0.52 && cell.forest_cover < 0.18) {
                cell.soil_fertility = std::max(0.65, cell.soil_fertility - 0.0015);
            } else if (cell.forest_cover >= 0.60) {
                cell.soil_fertility = std::min(1.25, cell.soil_fertility + 0.0008);
            }

            cell.forest_cover = std::clamp(cell.forest_cover, 0.0, 1.0);

            const bool was_forested = (old_cover >= 0.30);
            const bool now_forested = (cell.forest_cover >= 0.30);
            if (was_forested && !now_forested) {
                ++state.tiles_cleared_total;
            } else if (!was_forested && now_forested) {
                ++state.tiles_regrown_total;
            }

            if (!cell.has_river && cell.site_kind != MapSiteKind::SacredGrove) {
                if (cell.forest_cover >= 0.85 && cell.elevation < 0.58) {
                    cell.biome_index = b_old;
                } else if (cell.forest_cover >= 0.58 && cell.forest_cover < 0.82) {
                    cell.biome_index = b_forest;
                } else if (cell.forest_cover >= 0.30 && cell.forest_cover < 0.55) {
                    cell.biome_index = b_young;
                } else if (cell.forest_cover < 0.26) {
                    if (cell.elevation >= 0.54) {
                        cell.biome_index = b_hills;
                    } else if (cell.nearest_settlement_dist <= 2) {
                        cell.biome_index = b_plain;
                    } else if (cell.biome_index == b_young ||
                               cell.biome_index == b_forest ||
                               cell.biome_index == b_old ||
                               cell.biome_index == b_hills) {
                        cell.biome_index = b_meadow;
                    }
                }
            }
        }
    }

    // C. Update Settlement Local Woodland Cover & Kingdom Metrics
    for (const entt::entity s_ent : s_view) {
        auto& sett = s_view.get<ecs::SettlementComponent>(s_ent);
        const GridPos pos = s_view.get<ecs::GridPosComponent>(s_ent).pos;
        sett.local_woodland_cover = map.localForestCover(pos, 5);
    }

    computeCanopyStats(map, config, state);

    if (clock.season() == Season::Autumn && (clock.year() % 6 == 0) && rng.nextDouble() < 0.85 &&
        (state.tiles_cleared_total + state.tiles_regrown_total > 0)) {
        log.add(clock.year(), clock.season(), clock.month(), clock.day(),
                LogCategory::Ecology,
                std::format("Woodland Survey: Avg Canopy {:.0f}% ({} wooded, {} old-growth, {} cleared, {} regrown).",
                            state.average_canopy * 100.0,
                            state.forested_tiles,
                            state.old_growth_tiles,
                            state.tiles_cleared_total,
                            state.tiles_regrown_total));
    }
}

} // namespace sovereignia::sim

