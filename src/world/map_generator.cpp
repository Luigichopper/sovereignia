#include "sovereignia/world/map_generator.hpp"

#include "sovereignia/ecs/components.hpp"
#include "sovereignia/world/pathfinding.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace sovereignia::world {

namespace {

double hash2D(int x, int y, std::uint64_t seed) noexcept {
    std::uint64_t h = static_cast<std::uint64_t>(x) * 374761393ULL
                    + static_cast<std::uint64_t>(y) * 668265263ULL
                    + seed * 1442695041ULL;
    h = (h ^ (h >> 13U)) * 1274126177ULL;
    h ^= (h >> 16U);
    return static_cast<double>(h & 0xFFFFFFULL) / static_cast<double>(0x1000000ULL);
}

double smoothNoise(double x, double y, std::uint64_t seed) noexcept {
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const double tx = x - static_cast<double>(x0);
    const double ty = y - static_cast<double>(y0);
    const double sx = tx * tx * (3.0 - 2.0 * tx);
    const double sy = ty * ty * (3.0 - 2.0 * ty);

    const double v00 = hash2D(x0, y0, seed);
    const double v10 = hash2D(x0 + 1, y0, seed);
    const double v01 = hash2D(x0, y0 + 1, seed);
    const double v11 = hash2D(x0 + 1, y0 + 1, seed);

    const double ix0 = v00 + sx * (v10 - v00);
    const double ix1 = v01 + sx * (v11 - v01);
    return ix0 + sy * (ix1 - ix0);
}

double fbmNoise(int x, int y, std::uint64_t seed) noexcept {
    const double dx = static_cast<double>(x);
    const double dy = static_cast<double>(y);
    double val = 0.0;
    val += 0.55 * smoothNoise(dx * 0.065, dy * 0.065, seed);
    val += 0.30 * smoothNoise(dx * 0.150, dy * 0.150, seed + 17ULL);
    val += 0.15 * smoothNoise(dx * 0.320, dy * 0.320, seed + 53ULL);
    return val;
}

struct RegionTarget {
    GridPos anchor;
    std::string preferred_biome;
    std::string fallback_biome;
};

RegionTarget targetForHint(std::string_view hint, int w, int h, int west_pass_y) {
    if (hint == "center") {
        return {{w * 46 / 100, h * 50 / 100}, "fertile_plain", "river_valley"};
    }
    if (hint == "north_peaks") {
        return {{w * 32 / 100, std::max(3, h * 18 / 100)}, "taiga_hills", "alpine_peak"};
    }
    if (hint == "northwest_march") {
        return {{w * 20 / 100, std::max(4, h * 34 / 100)}, "taiga_hills", "fertile_plain"};
    }
    if (hint == "highland_forest") {
        return {{w * 66 / 100, std::max(4, h * 26 / 100)}, "boreal_forest", "old_growth_taiga"};
    }
    if (hint == "central_plain") {
        return {{w * 58 / 100, h * 54 / 100}, "fertile_plain", "river_valley"};
    }
    if (hint == "east_coast") {
        return {{w * 82 / 100, h * 62 / 100}, "coast", "fertile_plain"};
    }
    if (hint == "south_border") {
        return {{w * 44 / 100, h - 5}, "fertile_plain", "taiga_hills"};
    }
    if (hint == "north_taiga") {
        return {{w * 48 / 100, std::max(3, h * 16 / 100)}, "boreal_forest", "old_growth_taiga"};
    }
    if (hint == "east_valley") {
        return {{w * 72 / 100, h * 56 / 100}, "river_valley", "fertile_plain"};
    }
    if (hint == "north_lake") {
        return {{w * 48 / 100 - 5, std::max(5, h * 30 / 100)}, "coast", "young_woodland"};
    }
    if (hint == "west_pass") {
        return {{std::max(5, w * 15 / 100), west_pass_y}, "taiga_hills", "fertile_plain"};
    }
    if (hint == "southeast_woods") {
        return {{w * 68 / 100, h * 74 / 100}, "boreal_forest", "young_woodland"};
    }
    return {{w / 2, h / 2}, "fertile_plain", "river_valley"};
}

GridPos pickSettlementCell(const WorldMap& map,
                           const data::ConfigRegistry& config,
                           const RegionTarget& target,
                           const std::vector<GridPos>& occupied,
                           bool require_adjacent_sea,
                           bool require_adjacent_lake) {
    const int pref_idx = config.biomeIndex(target.preferred_biome);
    const int fall_idx = config.biomeIndex(target.fallback_biome);
    const int sea_idx = config.biomeIndex("deep_sea");
    const int shallows_idx = config.biomeIndex("coastal_shallows");
    const int lake_idx = config.biomeIndex("inland_lake");

    GridPos best = target.anchor;
    double best_score = 1e9;

    for (int y = 2; y < map.height() - 2; ++y) {
        for (int x = 2; x < map.width() - 2; ++x) {
            const auto& cell = map.at(x, y);
            const auto& biome = config.biomeByIndex(cell.biome_index);
            if (!biome.walkable || cell.settlement_entity != entt::null) {
                continue;
            }

            // Keep settlements on the mainland rather than outer offshore islands
            if (x > (map.width() * 88) / 100 && map.at(x - 4, y).elevation <= 0.08) {
                continue;
            }

            bool too_close = false;
            for (const auto& op : occupied) {
                const int manhattan = std::abs(op.x - x) + std::abs(op.y - y);
                if (manhattan < 8) {
                    too_close = true;
                    break;
                }
            }
            if (too_close) {
                continue;
            }

            if (require_adjacent_sea) {
                bool near_sea = false;
                bool west_mainland = false;
                for (int dy = -2; dy <= 2; ++dy) {
                    for (int dx = -2; dx <= 2; ++dx) {
                        if (map.inBounds(x + dx, y + dy)) {
                            const int bi = map.at(x + dx, y + dy).biome_index;
                            if (bi == sea_idx || bi == shallows_idx) {
                                near_sea = true;
                            }
                        }
                    }
                }
                if (map.inBounds(x - 4, y) && config.biomeByIndex(map.at(x - 4, y).biome_index).walkable) {
                    west_mainland = true;
                }
                if (!near_sea || !west_mainland) {
                    continue;
                }
            }

            if (require_adjacent_lake) {
                bool near_lake = false;
                for (int dy = -2; dy <= 2; ++dy) {
                    for (int dx = -2; dx <= 2; ++dx) {
                        if (map.inBounds(x + dx, y + dy) &&
                            map.at(x + dx, y + dy).biome_index == lake_idx) {
                            near_lake = true;
                        }
                    }
                }
                if (!near_lake) {
                    continue;
                }
            }

            double score = std::hypot(static_cast<double>(x - target.anchor.x),
                                      static_cast<double>(y - target.anchor.y));
            if (cell.biome_index == pref_idx) {
                score -= 4.5;
            } else if (cell.biome_index == fall_idx) {
                score -= 2.0;
            } else {
                score += 4.0;
            }
            if (cell.has_river) {
                score -= 1.0;
            }

            if (score < best_score) {
                best_score = score;
                best = GridPos{x, y};
            }
        }
    }

    return best;
}

void carveRiver(WorldMap& map,
                const data::ConfigRegistry& config,
                GridPos start,
                std::uint64_t seed) {
    const int sea_idx = config.biomeIndex("deep_sea");
    const int shallows_idx = config.biomeIndex("coastal_shallows");
    const int lake_idx = config.biomeIndex("inland_lake");
    const int peak_idx = config.biomeIndex("alpine_peak");
    const int glacier_idx = config.biomeIndex("glacier_ridge");
    const int coast_idx = config.biomeIndex("coast");
    const int valley_idx = config.biomeIndex("river_valley");

    GridPos cur = start;
    bool major_stream = false;
    const int max_steps = map.width() + map.height();

    for (int step = 0; step < max_steps; ++step) {
        if (!map.inBounds(cur)) {
            break;
        }
        auto& cell = map.at(cur);
        if (cell.biome_index == sea_idx || cell.biome_index == shallows_idx) {
            break;
        }
        if (cell.biome_index != lake_idx) {
            if (cell.has_river) {
                major_stream = true;
            }
            cell.has_river = true;
            cell.river_width = major_stream ? 2 : std::max<std::uint8_t>(cell.river_width, 1);
            cell.elevation = std::max(0.12, cell.elevation);
            cell.moisture = std::max(cell.moisture, 0.72);
            cell.soil_fertility = std::clamp(cell.soil_fertility + 0.16, 0.90, 1.40);
            if (cell.biome_index != peak_idx && cell.biome_index != glacier_idx && cell.biome_index != coast_idx) {
                cell.biome_index = valley_idx;
                cell.forest_cover = std::clamp(cell.forest_cover, 0.10, 0.45);
            }
        }

        const std::array<GridPos, 4> candidates{{{cur.x + 1, cur.y},
                                                 {cur.x + 1, cur.y + 1},
                                                 {cur.x, cur.y + 1},
                                                 {cur.x + 1, cur.y - 1}}};
        GridPos next = cur;
        double best_cost = 1e9;
        for (const auto c : candidates) {
            if (!map.inBounds(c) || c == cur) {
                continue;
            }
            const auto& nc = map.at(c);
            if (nc.biome_index == glacier_idx) {
                continue;
            }
            const double jitter = (hash2D(c.x, c.y, seed + static_cast<std::uint64_t>(step)) - 0.5) * 0.12;
            const double cost = nc.elevation + jitter - 0.09 * static_cast<double>(c.x - cur.x);
            if (cost < best_cost) {
                best_cost = cost;
                next = c;
            }
        }
        if (next == cur) {
            break;
        }
        cur = next;
    }
}

void stampRoad(WorldMap& map, const std::vector<GridPos>& path, RoadLevel level) {
    for (const auto& p : path) {
        if (!map.inBounds(p)) {
            continue;
        }
        auto& cell = map.at(p);
        if (static_cast<int>(level) > static_cast<int>(cell.road_level)) {
            cell.road_level = level;
        }
        if (cell.has_river) {
            cell.has_bridge = true;
        }
        if (level == RoadLevel::ImperialHighway) {
            cell.forest_cover = std::min(cell.forest_cover, 0.52);
        }
    }
}

} // namespace

GeneratedWorldInfo generateNorthernKingdom(WorldMap& map,
                                           entt::registry& registry,
                                           const data::ConfigRegistry& config,
                                           core::Rng& rng,
                                           sim::FamilyTree& family_tree,
                                           int start_year) {
    const std::uint64_t terrain_seed = rng.nextU64();
    const int w = map.width();
    const int h = map.height();

    const int b_sea = config.biomeIndex("deep_sea");
    const int b_shallows = config.biomeIndex("coastal_shallows");
    const int b_lake = config.biomeIndex("inland_lake");
    const int b_coast = config.biomeIndex("coast");
    const int b_plain = config.biomeIndex("fertile_plain");
    const int b_meadow = config.biomeIndex("cleared_meadow");
    const int b_young = config.biomeIndex("young_woodland");
    const int b_forest = config.biomeIndex("boreal_forest");
    const int b_old = config.biomeIndex("old_growth_taiga");
    const int b_marsh = config.biomeIndex("marshland");
    const int b_hills = config.biomeIndex("taiga_hills");
    const int b_tundra = config.biomeIndex("tundra_steppe");
    const int b_peak = config.biomeIndex("alpine_peak");
    const int b_glacier = config.biomeIndex("glacier_ridge");

    const int south_pass_x = std::max(6, (w * 44) / 100);
    const int west_pass_y = std::clamp((h * 42) / 100, 6, h - 7);

    const double mirren_cx = static_cast<double>(w) * 0.48;
    const double mirren_cy = static_cast<double>(h) * 0.30;
    const double mirren_rx = std::max(2.6, static_cast<double>(w) * 0.036);
    const double mirren_ry = std::max(1.8, static_cast<double>(h) * 0.052);

    const double tarn_cx = static_cast<double>(w) * 0.64;
    const double tarn_cy = static_cast<double>(h) * 0.22;
    const double tarn_rx = std::max(2.0, static_cast<double>(w) * 0.024);
    const double tarn_ry = std::max(1.4, static_cast<double>(h) * 0.038);

    // 1. Base geography: Northern/Western Boreas Range, Eastern Fjord Sea, Rain Shadow, Glacial Lakes & Two Mountain Passes
    std::vector<int> coast_x_by_y(static_cast<std::size_t>(h), w - 10);
    for (int y = 0; y < h; ++y) {
        const double coast_wave = std::sin(static_cast<double>(y) * 0.22) * (static_cast<double>(w) * 0.035)
                                + (fbmNoise(50, y, terrain_seed + 101ULL) - 0.5) * (static_cast<double>(w) * 0.08);
        const int coast_x = static_cast<int>(std::round(static_cast<double>(w) * 0.84 + coast_wave));
        coast_x_by_y[static_cast<std::size_t>(y)] = coast_x;

        for (int x = 0; x < w; ++x) {
            auto& cell = map.at(x, y);
            const double nx = static_cast<double>(x) / std::max(1, w - 1);
            const double ny = static_cast<double>(y) / std::max(1, h - 1);

            const double n_elev = fbmNoise(x, y, terrain_seed);
            const double n_moist = fbmNoise(x + 31, y + 71, terrain_seed + 991ULL);
            const double n_temp = fbmNoise(x + 113, y + 197, terrain_seed + 2203ULL);

            // Elevation sculpting
            double elev = 0.24 + 0.36 * n_elev;
            if (ny < 0.16) {
                elev += 0.48 * (1.0 - ny / 0.16);
            }
            if (nx < 0.12) {
                elev += 0.38 * (1.0 - nx / 0.12);
            }
            if (ny > 0.88) {
                elev += 0.40 * ((ny - 0.88) / 0.12);
            }

            // Carve walkable South Pass Gorge & Westmarch Pass
            if (y >= h - 8 && std::abs(x - south_pass_x) <= 3) {
                elev = std::min(elev, 0.36 + 0.02 * static_cast<double>(std::abs(x - south_pass_x)));
            }
            if (x <= (w * 17) / 100 && std::abs(y - west_pass_y) <= 2) {
                elev = std::min(elev, 0.38 + 0.02 * static_cast<double>(std::abs(y - west_pass_y)));
            }

            // Coastal drop toward Eastern Sea
            if (x >= coast_x - 3) {
                elev -= 0.12 * static_cast<double>(x - (coast_x - 3));
            }
            elev = std::clamp(elev, 0.12, 1.0);

            // Moisture with maritime east wind + orographic lift - NW rain-shadow
            double moist = n_moist + 0.08 * nx + 0.10 * std::min(0.6, elev);
            if (nx < 0.30 && ny < 0.40) {
                moist -= 0.15;
            }

            const double d_mirren = std::hypot((static_cast<double>(x) - mirren_cx) / mirren_rx,
                                               (static_cast<double>(y) - mirren_cy) / mirren_ry)
                                  + (hash2D(x, y, terrain_seed + 409ULL) - 0.5) * 0.25;
            const double d_tarn = std::hypot((static_cast<double>(x) - tarn_cx) / tarn_rx,
                                             (static_cast<double>(y) - tarn_cy) / tarn_ry)
                                + (hash2D(x, y, terrain_seed + 601ULL) - 0.5) * 0.25;

            if (d_mirren <= 1.45 || d_tarn <= 1.45) {
                moist = std::max(moist, 0.68);
            }
            moist = std::clamp(moist, 0.05, 0.96);

            const double temp = std::clamp(0.66 * ny + 0.42 * (1.0 - elev) - 0.08 + (n_temp - 0.5) * 0.18,
                                           0.05, 0.95);

            cell.elevation = elev;
            cell.moisture = moist;
            cell.temperature = temp;
            cell.has_river = false;
            cell.has_bridge = false;
            cell.river_width = 0;
            cell.road_level = RoadLevel::None;
            cell.settlement_entity = entt::null;
            cell.site_kind = MapSiteKind::None;
            cell.site_name.clear();

            // Small Boreal Sea island near east central waters
            const double d_island = std::hypot((static_cast<double>(x) - static_cast<double>(w) * 0.94) / 2.6,
                                               (static_cast<double>(y) - static_cast<double>(h) * 0.48) / 2.2);

            if (d_island <= 1.0 && x >= coast_x + 3) {
                cell.biome_index = (d_island <= 0.45) ? b_forest : b_coast;
                cell.elevation = 0.20;
                cell.forest_cover = (d_island <= 0.45) ? 0.65 : 0.04;
            } else if (x >= coast_x + 3) {
                cell.biome_index = b_sea;
                cell.elevation = 0.01;
                cell.forest_cover = 0.0;
            } else if (x >= coast_x + 1) {
                cell.biome_index = b_shallows;
                cell.elevation = 0.05;
                cell.forest_cover = 0.0;
            } else if (x >= coast_x) {
                cell.biome_index = b_coast;
                cell.elevation = std::max(0.14, elev);
                cell.forest_cover = 0.03;
            } else if (d_mirren <= 1.0) {
                cell.biome_index = b_lake;
                cell.elevation = 0.03;
                cell.moisture = 0.92;
                cell.forest_cover = 0.0;
            } else if (d_tarn <= 1.0) {
                cell.biome_index = b_lake;
                cell.elevation = 0.03;
                cell.moisture = 0.90;
                cell.forest_cover = 0.0;
            } else if (elev >= 0.86) {
                cell.biome_index = b_glacier;
                cell.forest_cover = 0.0;
            } else if (elev >= 0.72) {
                cell.biome_index = b_peak;
                cell.forest_cover = 0.03;
            } else if (elev >= 0.56) {
                cell.biome_index = b_hills;
                cell.forest_cover = 0.20 + 0.22 * moist;
            } else if (temp < 0.36 && moist < 0.44) {
                cell.biome_index = b_tundra;
                cell.forest_cover = 0.04 + 0.04 * moist;
            } else if (moist >= 0.73 && n_moist >= 0.60 && elev <= 0.38 && ny >= 0.54 && nx >= 0.30) {
                cell.biome_index = b_marsh;
                cell.forest_cover = 0.06 + 0.06 * (1.0 - elev);
            } else if (moist >= 0.65 && temp >= 0.26 && (ny <= 0.42 || ny >= 0.66)) {
                cell.biome_index = b_old;
                cell.forest_cover = 0.87 + 0.09 * hash2D(x, y, terrain_seed + 71ULL);
            } else if ((moist >= 0.47 && ny <= 0.56) || moist >= 0.55) {
                cell.biome_index = b_forest;
                cell.forest_cover = 0.62 + 0.18 * hash2D(x, y, terrain_seed + 83ULL);
            } else if (moist >= 0.42) {
                cell.biome_index = b_young;
                cell.forest_cover = 0.34 + 0.18 * hash2D(x, y, terrain_seed + 97ULL);
            } else {
                cell.biome_index = b_plain;
                cell.forest_cover = 0.06 + 0.10 * moist;
            }

            cell.soil_fertility = std::clamp(0.76 + 0.42 * cell.moisture - 0.30 * std::max(0.0, cell.elevation - 0.40),
                                             0.55, 1.35);

            const auto& biome = config.biomeByIndex(cell.biome_index);
            const double mana_spike = fbmNoise(x + 9, y + 19, terrain_seed + 3333ULL);
            cell.mana_density = std::clamp(biome.base_mana + (mana_spike - 0.4) * 0.65, 0.10, 1.85);
        }
    }

    // 2. Carve four northern rivers draining toward the eastern coast
    carveRiver(map, config, GridPos{std::max(8, (w * 19) / 100), std::max(5, (h * 17) / 100)}, terrain_seed + 11ULL);
    carveRiver(map, config, GridPos{static_cast<int>(mirren_cx + mirren_rx + 1.0), static_cast<int>(mirren_cy)}, terrain_seed + 29ULL);
    carveRiver(map, config, GridPos{static_cast<int>(tarn_cx + tarn_rx + 1.0), static_cast<int>(tarn_cy + 1.0)}, terrain_seed + 47ULL);
    carveRiver(map, config, GridPos{std::max(10, (w * 22) / 100), (h * 64) / 100}, terrain_seed + 79ULL);

    // 3. Place settlements & create ECS entities
    GeneratedWorldInfo info;
    info.west_pass_entry = GridPos{2, west_pass_y};
    for (int x = 1; x <= (w * 18) / 100; ++x) {
        auto& wc = map.at(x, west_pass_y);
        if (!config.biomeByIndex(wc.biome_index).walkable) {
            wc.biome_index = b_hills;
            wc.elevation = 0.42;
        }
    }

    std::vector<GridPos> occupied_positions;
    std::vector<entt::entity> settlement_entities;

    for (const auto& seed : config.settlementSeeds()) {
        const RegionTarget target = targetForHint(seed.region_hint, w, h, west_pass_y);
        const bool need_sea = (seed.region_hint == "east_coast");
        const bool need_lake = (seed.region_hint == "north_lake");
        const GridPos pos = pickSettlementCell(map, config, target, occupied_positions, need_sea, need_lake);
        occupied_positions.push_back(pos);

        auto& cell = map.at(pos);
        if (!config.biomeByIndex(cell.biome_index).walkable) {
            cell.biome_index = b_plain;
        }
        cell.forest_cover = 0.08;

        const entt::entity s_ent = registry.create();
        cell.settlement_entity = s_ent;
        settlement_entities.push_back(s_ent);
        info.settlement_by_id[seed.id] = s_ent;

        registry.emplace<ecs::NameComponent>(s_ent, seed.name);
        registry.emplace<ecs::GridPosComponent>(s_ent, pos);

        ecs::SettlementComponent s_comp;
        s_comp.id = seed.id;
        s_comp.type = seed.type;
        s_comp.dominant_text = seed.dominant_text;
        s_comp.active_law = seed.active_law;
        s_comp.prosperity = (seed.type == SettlementType::Capital) ? 0.84 : 0.72;
        s_comp.bandit_threat = (seed.type == SettlementType::Fortress) ? 0.04 : 0.09;
        s_comp.forestry_policy = (seed.type == SettlementType::Bishopric)
            ? ForestryPolicy::WardenReforestation
            : ForestryPolicy::BalancedCoppice;

        ecs::ResourceStockpileComponent stock;
        stock.current = seed.stockpiles;
        stock.snapshotMonthStart();
        registry.emplace<ecs::ResourceStockpileComponent>(s_ent, stock);

        ecs::DemographicsComponent demo;
        for (std::size_t ci = 0; ci < kCohortCount; ++ci) {
            const int pop = seed.cohorts[ci];
            if (pop <= 0) {
                continue;
            }
            ecs::PopCohort cohort;
            cohort.type = static_cast<CohortType>(ci);
            cohort.population = pop;
            cohort.fed_ratio = 0.94;
            cohort.warmth_ratio = 0.96;
            cohort.orthodoxy = (seed.id == "south_pass") ? 0.70 : 0.91;
            cohort.unrest = 0.05;
            demo.cohorts.push_back(cohort);
        }
        registry.emplace<ecs::DemographicsComponent>(s_ent, demo);

        ecs::FeudalContractComponent contract;
        contract.corvee_rate = seed.corvee_rate;
        contract.tithe_rate = seed.tithe_rate;
        contract.exempt_from_levy = (seed.type == SettlementType::Bishopric);
        registry.emplace<ecs::FeudalContractComponent>(s_ent, contract);

        const int r_bmonth = rng.nextInt(1, 12);
        const int r_bday = rng.nextInt(2, 29);
        const entt::entity r_ent = registry.create();
        registry.emplace<ecs::NameComponent>(r_ent, seed.ruler.name);
        registry.emplace<ecs::CharacterComponent>(r_ent, ecs::CharacterComponent{
            seed.ruler.age,
            seed.ruler.female,
            true,
            seed.ruler.trait,
            s_ent,
            r_bmonth,
            r_bday
        });
        registry.emplace<ecs::SocialStatusComponent>(r_ent, ecs::SocialStatusComponent{
            seed.ruler.title,
            seed.ruler.house,
            seed.type == SettlementType::Bishopric,
            seed.type == SettlementType::Capital ? 92 : 60
        });
        registry.emplace<ecs::MartialMagicComponent>(r_ent, ecs::MartialMagicComponent{
            seed.ruler.sword_style,
            seed.ruler.sword_rank,
            seed.ruler.magic_school,
            seed.ruler.magic_rank,
            seed.ruler.mana_capacity,
            seed.ruler.incantationless
        });
        registry.emplace<ecs::FaithComponent>(r_ent, ecs::FaithComponent{
            seed.ruler.rite,
            seed.ruler.zeal,
            seed.ruler.rite == RiteType::ImperialOrthodox ? 0.78 : 0.32
        });

        const std::string ruler_given = sim::FamilyTree::extractGivenName(seed.ruler.name);

        sim::PersonNode ruler_node;
        ruler_node.live_entity = r_ent;
        ruler_node.given_name = ruler_given;
        ruler_node.house = seed.ruler.house;
        ruler_node.birth_house = seed.ruler.house;
        ruler_node.title = seed.ruler.title;
        ruler_node.seat_name = seed.name;
        ruler_node.regnal_number = 1;
        ruler_node.generation = 1;
        ruler_node.is_female = seed.ruler.female;
        ruler_node.alive = true;
        ruler_node.is_bastard = false;
        ruler_node.was_ruler = true;
        ruler_node.birth_year = start_year - seed.ruler.age;
        ruler_node.birth_month = r_bmonth;
        ruler_node.birth_day = r_bday;
        ruler_node.current_age = seed.ruler.age;
        ruler_node.trait = seed.ruler.trait;
        ruler_node.sword_style = seed.ruler.sword_style;
        ruler_node.sword_rank = seed.ruler.sword_rank;
        ruler_node.magic_school = seed.ruler.magic_school;
        ruler_node.magic_rank = seed.ruler.magic_rank;
        ruler_node.mana_capacity = seed.ruler.mana_capacity;
        ruler_node.incantationless = seed.ruler.incantationless;
        const sim::PersonId ruler_pid = family_tree.registerPerson(std::move(ruler_node));

        ecs::FamilyComponent ruler_fam;
        ruler_fam.head_entity = r_ent;
        ruler_fam.role = DynastyRole::HeadOfHouse;
        ruler_fam.ambition = (seed.ruler.trait == "Ambitious" || seed.ruler.trait == "Scheming") ? 0.78 : 0.48;
        ruler_fam.faction_backing = seed.type == SettlementType::Capital ? 85 : 55;
        ruler_fam.note = seed.ruler.trait;
        ruler_fam.person_id = ruler_pid;
        ruler_fam.regnal_number = 1;
        ruler_fam.given_name = ruler_given;
        registry.emplace<ecs::FamilyComponent>(r_ent, ruler_fam);

        ecs::HouseholdComponent hh;
        entt::entity primary_spouse = entt::null;
        sim::PersonId primary_spouse_pid = sim::kInvalidPersonId;
        for (const auto& hm : seed.ruler.household) {
            const entt::entity m_ent = registry.create();
            registry.emplace<ecs::NameComponent>(m_ent, hm.name);

            DynastyRole d_role = DynastyRole::RetainerTutor;
            bool m_female = false;
            bool m_clergy = false;
            double m_ambition = 0.35;
            int m_backing = 30;
            SkillRank m_sword = SkillRank::None;
            std::string m_sword_style = "None";
            SkillRank m_magic = SkillRank::None;
            std::string m_magic_school = "None";
            double m_mana = 24.0;
            std::string m_trait = "Diligent";

            if (hm.role == "Legitimate Wife" || hm.role == "Husband") {
                d_role = DynastyRole::LegitimateSpouse;
                m_female = (hm.role == "Legitimate Wife");
                m_ambition = (hm.note.find("Scheming") != std::string::npos) ? 0.78 : 0.45;
                m_backing = 60;
                m_trait = (hm.note.find("Scheming") != std::string::npos) ? "Scheming" : "Pragmatic";
                primary_spouse = m_ent;
            } else if (hm.role == "Concubine") {
                d_role = DynastyRole::Concubine;
                m_female = true;
                m_ambition = 0.72;
                m_backing = (hm.note.find("guild") != std::string::npos) ? 74 : 48;
                m_trait = "Mercantile";
            } else if (hm.role == "Legitimate Heir" || hm.role == "Heir" || hm.role == "Son" ||
                       hm.role == "Daughter" || hm.role == "Ward") {
                d_role = DynastyRole::LegitimateChild;
                m_female = (hm.role == "Daughter" || hm.role == "Ward");
                m_mana = (hm.note.find("142") != std::string::npos) ? 142.0
                       : (hm.note.find("attuned") != std::string::npos || hm.note.find("magic") != std::string::npos) ? 78.0
                       : (hm.note.find("healer") != std::string::npos) ? 68.0 : 34.0;
                if (m_mana >= 65.0) {
                    m_magic_school = "Frost/Water";
                    m_magic = SkillRank::Beginner;
                }
                if (hm.age >= 14) {
                    m_sword_style = seed.ruler.sword_style == "None" ? "Water-Guard" : seed.ruler.sword_style;
                    m_sword = SkillRank::Beginner;
                }
            } else if (hm.role == "Sword Tutor") {
                d_role = DynastyRole::RetainerTutor;
                m_sword_style = "Water-Guard";
                m_sword = SkillRank::Saint;
                m_trait = "Vigilant";
            } else {
                d_role = DynastyRole::ClergyMember;
                m_clergy = true;
                m_female = (hm.role == "Infirmarian");
                if (hm.note.find("Restoration") != std::string::npos) {
                    m_magic_school = "Healing/Restoration";
                    m_magic = SkillRank::Advanced;
                    m_mana = 86.0;
                }
                m_trait = "Ascetic";
            }

            const std::string m_given = sim::FamilyTree::extractGivenName(hm.name);
            const std::string m_pat = (d_role == DynastyRole::LegitimateChild)
                ? sim::FamilyTree::makePatronymic(ruler_given, m_female, seed.ruler.house)
                : std::string{};

            const int m_bmonth = rng.nextInt(1, 12);
            const int m_bday = rng.nextInt(2, 29);

            sim::PersonNode m_node;
            m_node.live_entity = m_ent;
            m_node.given_name = m_given;
            m_node.patronymic = m_pat;
            m_node.house = seed.ruler.house;
            m_node.birth_house = seed.ruler.house;
            m_node.title = hm.role;
            m_node.seat_name = seed.name;
            m_node.generation = (d_role == DynastyRole::LegitimateChild) ? 2 : 1;
            m_node.is_female = m_female;
            m_node.alive = true;
            m_node.is_bastard = false;
            m_node.was_ruler = false;
            m_node.birth_year = start_year - hm.age;
            m_node.birth_month = m_bmonth;
            m_node.birth_day = m_bday;
            m_node.current_age = hm.age;
            m_node.trait = m_trait;
            m_node.sword_style = m_sword_style;
            m_node.sword_rank = m_sword;
            m_node.magic_school = m_magic_school;
            m_node.magic_rank = m_magic;
            m_node.mana_capacity = m_mana;
            m_node.father_id = (d_role == DynastyRole::LegitimateChild) ? ruler_pid : sim::kInvalidPersonId;
            m_node.mother_id = (d_role == DynastyRole::LegitimateChild) ? primary_spouse_pid : sim::kInvalidPersonId;
            const sim::PersonId m_pid = family_tree.registerPerson(std::move(m_node));

            if (d_role == DynastyRole::LegitimateSpouse) {
                primary_spouse_pid = m_pid;
                family_tree.addSpouse(ruler_pid, m_pid);
            } else if (d_role == DynastyRole::Concubine) {
                family_tree.addConcubine(ruler_pid, m_pid);
            } else if (d_role == DynastyRole::LegitimateChild) {
                family_tree.addChild(ruler_pid, primary_spouse_pid, m_pid);
            }

            registry.emplace<ecs::CharacterComponent>(m_ent, ecs::CharacterComponent{
                hm.age,
                m_female,
                true,
                m_trait,
                s_ent,
                m_bmonth,
                m_bday
            });
            registry.emplace<ecs::SocialStatusComponent>(m_ent, ecs::SocialStatusComponent{
                hm.role,
                seed.ruler.house,
                m_clergy,
                d_role == DynastyRole::LegitimateChild ? 40 : 28
            });
            registry.emplace<ecs::MartialMagicComponent>(m_ent, ecs::MartialMagicComponent{
                m_sword_style,
                m_sword,
                m_magic_school,
                m_magic,
                m_mana,
                false
            });
            registry.emplace<ecs::FaithComponent>(m_ent, ecs::FaithComponent{
                seed.ruler.rite,
                0.70,
                seed.ruler.rite == RiteType::ImperialOrthodox ? 0.72 : 0.30
            });
            registry.emplace<ecs::FamilyComponent>(m_ent, ecs::FamilyComponent{
                r_ent,
                (d_role == DynastyRole::LegitimateChild) ? primary_spouse : entt::null,
                (d_role == DynastyRole::LegitimateChild) ? r_ent : entt::null,
                d_role,
                false,
                m_ambition,
                m_backing,
                0,
                15,
                hm.note,
                m_pid,
                0,
                m_given,
                m_pat,
                ""
            });
            hh.member_entities.push_back(m_ent);
        }
        registry.emplace<ecs::HouseholdComponent>(r_ent, std::move(hh));

        s_comp.ruler_entity = r_ent;
        const int farmers = demo.countOf(CohortType::Farmers);
        s_comp.levy_spearmen_ready = std::max(15, farmers / 2 + demo.totalPopulation() / 8);
        s_comp.knights_ready = (seed.type == SettlementType::Capital) ? 30
                             : (seed.type == SettlementType::Fortress) ? 18
                             : (seed.type == SettlementType::Bishopric) ? 6 : 5;
        s_comp.mages_ready = (seed.type == SettlementType::Capital) ? 4
                           : (seed.type == SettlementType::Bishopric) ? 5
                           : (seed.ruler.magic_rank >= SkillRank::Intermediate ? 2 : 1);

        registry.emplace<ecs::SettlementComponent>(s_ent, std::move(s_comp));

        if (seed.type == SettlementType::Capital) {
            info.capital_entity = s_ent;
        } else if (seed.type == SettlementType::Bishopric) {
            info.monastery_entity = s_ent;
        } else if (seed.id == "south_pass") {
            info.south_pass_entity = s_ent;
        }
    }

    for (const entt::entity s_ent : settlement_entities) {
        auto& s_comp = registry.get<ecs::SettlementComponent>(s_ent);
        if (s_ent != info.capital_entity) {
            s_comp.liege_settlement = info.capital_entity;
        }
        s_comp.ecclesiastic_see = info.monastery_entity;
    }

    // 4. Precompute settlement distance field, local woodland cover & regional names
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            auto& cell = map.at(x, y);
            int min_dist = 255;
            for (const auto& sp : occupied_positions) {
                min_dist = std::min(min_dist, std::abs(sp.x - x) + std::abs(sp.y - y));
            }
            cell.nearest_settlement_dist = static_cast<std::uint8_t>(std::min(255, min_dist));

            const double nx = static_cast<double>(x) / std::max(1, w - 1);
            const double ny = static_cast<double>(y) / std::max(1, h - 1);

            if (cell.biome_index == b_lake) {
                cell.region_name = (nx > 0.56) ? "Cyril's Tarn" : "Lake Mirren";
            } else if (cell.biome_index == b_sea || cell.biome_index == b_shallows) {
                cell.region_name = "Boreal Sea";
            } else if (cell.biome_index == b_coast) {
                cell.region_name = "Shinglefjord Coast";
            } else if (y >= h - 8 && std::abs(x - south_pass_x) <= 5) {
                cell.region_name = "South Pass Gorge";
            } else if (nx <= 0.18 && std::abs(y - west_pass_y) <= 4) {
                cell.region_name = "Westmarch Pass";
            } else if (cell.elevation >= 0.64) {
                cell.region_name = "Boreas Spine";
            } else if (cell.biome_index == b_marsh) {
                cell.region_name = "Frostfen Moors";
            } else if (cell.biome_index == b_tundra) {
                cell.region_name = "Rimewind Steppe";
            } else if (ny <= 0.36 && nx >= 0.34) {
                cell.region_name = "Frostwood Taiga";
            } else if (ny >= 0.56 && nx >= 0.55) {
                cell.region_name = "Birchhollow Woods";
            } else if (cell.has_river) {
                cell.region_name = "Silverflow Valley";
            } else {
                cell.region_name = "Vaelen Heartland";
            }
        }
    }

    for (const entt::entity s_ent : settlement_entities) {
        const GridPos pos = registry.get<ecs::GridPosComponent>(s_ent).pos;
        registry.get<ecs::SettlementComponent>(s_ent).local_woodland_cover = map.localForestCover(pos, 5);
    }

    // 5. Construct A* Road Network & River Bridges
    auto connect = [&](std::string_view id_a, std::string_view id_b, RoadLevel level) {
        const auto ita = info.settlement_by_id.find(std::string(id_a));
        const auto itb = info.settlement_by_id.find(std::string(id_b));
        if (ita == info.settlement_by_id.end() || itb == info.settlement_by_id.end()) {
            return;
        }
        const GridPos pa = registry.get<ecs::GridPosComponent>(ita->second).pos;
        const GridPos pb = registry.get<ecs::GridPosComponent>(itb->second).pos;
        const auto path = findPath(map, config, pa, pb, true);
        if (path.empty()) {
            throw std::runtime_error("Failed to build road between " + std::string(id_a) + " and " + std::string(id_b));
        }
        stampRoad(map, path, level);
    };

    connect("south_pass", "vaelgard", RoadLevel::ImperialHighway);
    connect("vaelgard", "fort_vael", RoadLevel::ImperialHighway);
    connect("fort_vael", "westwatch", RoadLevel::ImperialHighway);
    if (const auto it_w = info.settlement_by_id.find("westwatch"); it_w != info.settlement_by_id.end()) {
        const GridPos wp = registry.get<ecs::GridPosComponent>(it_w->second).pos;
        const auto p = findPath(map, config, wp, info.west_pass_entry, true);
        if (!p.empty()) {
            stampRoad(map, p, RoadLevel::ImperialHighway);
        }
    }

    connect("vaelgard", "oakhaven", RoadLevel::DirtRoad);
    connect("oakhaven", "rotherfield", RoadLevel::DirtRoad);
    connect("rotherfield", "saltmere", RoadLevel::DirtRoad);
    connect("oakhaven", "birchhollow", RoadLevel::DirtRoad);
    connect("birchhollow", "saltmere", RoadLevel::DirtRoad);
    connect("birchhollow", "south_pass", RoadLevel::DirtRoad);
    connect("vaelgard", "mirrenstead", RoadLevel::DirtRoad);
    connect("mirrenstead", "frostholm", RoadLevel::DirtRoad);
    connect("frostholm", "kaldheim", RoadLevel::DirtRoad);
    connect("fort_vael", "kaldheim", RoadLevel::DirtRoad);
    connect("mirrenstead", "st_cyril", RoadLevel::DirtRoad);
    connect("st_cyril", "oakhaven", RoadLevel::DirtRoad);
    connect("frostholm", "st_cyril", RoadLevel::DirtRoad);

    // 6. Off-Map Empire & Rival Duchy Entities
    {
        const entt::entity emp = registry.create();
        registry.emplace<ecs::NameComponent>(emp, "Southern Empire of Heliodor");
        registry.emplace<ecs::OffMapEmpireComponent>(emp, ecs::OffMapEmpireComponent{
            "Southern Empire of Heliodor",
            "Basileus Aurelius IV",
            0.62,
            135.0,
            0.0,
            24,
            "Demanding Tribute & Orthodoxy"
        });
        info.empire_entity = emp;

        const entt::entity rival = registry.create();
        registry.emplace<ecs::NameComponent>(rival, "Rival Duchy of Ostmark");
        registry.emplace<ecs::OffMapEmpireComponent>(rival, ecs::OffMapEmpireComponent{
            "Rival Duchy of Ostmark",
            "Duke Vladislav Ost",
            0.35,
            0.0,
            0.0,
            -18,
            "Testing Northern Passes"
        });
        info.rival_duchy_entity = rival;
    }

    // 7. Seed Active Laws & Literary Works from JSON
    for (const auto& jl : config.initialLaws()) {
        const entt::entity le = registry.create();
        registry.emplace<ecs::NameComponent>(le, jl.name);
        registry.emplace<ecs::ActiveLawComponent>(le, ecs::ActiveLawComponent{
            jl.name,
            jl.author,
            jl.year,
            jl.seat,
            jl.summary,
            "charter"
        });
    }

    auto findSettlementByName = [&](std::string_view name) -> entt::entity {
        for (const entt::entity se : settlement_entities) {
            if (registry.get<ecs::NameComponent>(se).value == name) {
                return se;
            }
        }
        return info.monastery_entity;
    };

    for (const auto& jt : config.initialTexts()) {
        const entt::entity te = registry.create();
        registry.emplace<ecs::NameComponent>(te, jt.title);
        std::string trait = "Pragmatic";
        if (jt.title.find("Frost") != std::string::npos) {
            trait = "Hardy";
        } else if (jt.title.find("Homilies") != std::string::npos) {
            trait = "Charitable";
        } else if (jt.title.find("Uncreated") != std::string::npos) {
            trait = "Ascetic";
        }
        registry.emplace<ecs::LiteraryWorkComponent>(te, ecs::LiteraryWorkComponent{
            jt.title,
            jt.author,
            jt.kind,
            jt.year,
            jt.location,
            jt.doctrine,
            findSettlementByName(jt.location),
            trait,
            jt.title.find("Uncreated") != std::string::npos,
            true
        });
    }

    // 8. Seed Natural & Historic Map Landmarks (Ruins, Sacred Groves, Watchtowers, Hermitages, Mana Wells)
    auto placeSite = [&](GridPos hint, MapSiteKind kind, std::string site_name) {
        for (int rad = 0; rad <= 12; ++rad) {
            for (int dy = -rad; dy <= rad; ++dy) {
                for (int dx = -rad; dx <= rad; ++dx) {
                    const GridPos p{hint.x + dx, hint.y + dy};
                    if (!map.inBounds(p)) {
                        continue;
                    }
                    auto& c = map.at(p);
                    if (config.biomeByIndex(c.biome_index).walkable &&
                        c.settlement_entity == entt::null &&
                        c.road_level == RoadLevel::None &&
                        c.site_kind == MapSiteKind::None) {
                        c.site_kind = kind;
                        c.site_name = std::move(site_name);
                        if (kind == MapSiteKind::SacredGrove) {
                            c.biome_index = b_old;
                            c.forest_cover = 0.94;
                        } else if (kind == MapSiteKind::ManaWell) {
                            c.mana_density = std::max(c.mana_density, 1.65);
                        }
                        return;
                    }
                }
            }
        }
    };

    placeSite(GridPos{(w * 26) / 100, (h * 26) / 100}, MapSiteKind::AncientRuin, "Ruins of Old Kaldholt (Year 214)");
    placeSite(GridPos{(w * 56) / 100, (h * 40) / 100}, MapSiteKind::AncientRuin, "Watchtower of the First Frost");
    placeSite(GridPos{(w * 32) / 100, (h * 74) / 100}, MapSiteKind::AncientRuin, "Sunken Barrow of Aurelia's March");

    placeSite(GridPos{(w * 63) / 100, (h * 28) / 100}, MapSiteKind::SacredGrove, "Sacred Grove of St. Cyril");
    placeSite(GridPos{(w * 51) / 100, (h * 19) / 100}, MapSiteKind::SacredGrove, "Elder Pine Sanctuary");

    placeSite(GridPos{south_pass_x + 4, h - 7}, MapSiteKind::Watchtower, "Southmarch Stone Beacon");
    placeSite(GridPos{(w * 19) / 100, west_pass_y - 3}, MapSiteKind::Watchtower, "Westdefilade Signal Tower");
    placeSite(GridPos{(w * 35) / 100, (h * 22) / 100}, MapSiteKind::Watchtower, "Kaldheim Crag Watch");

    placeSite(GridPos{static_cast<int>(tarn_cx) + 3, static_cast<int>(tarn_cy) - 1}, MapSiteKind::Hermitage, "Cell of St. Nikon on the Tarn");
    placeSite(GridPos{(w * 74) / 100, (h * 72) / 100}, MapSiteKind::Hermitage, "Frostfen Anchorite Hut");

    placeSite(GridPos{(w * 24) / 100, (h * 15) / 100}, MapSiteKind::ManaWell, "Boreal Ley Spring");
    placeSite(GridPos{(w * 42) / 100, (h * 28) / 100}, MapSiteKind::ManaWell, "Rime-Crystal Cairn");
    placeSite(GridPos{(w * 76) / 100, (h * 42) / 100}, MapSiteKind::ManaWell, "Sunken Aether Well");

    (void)b_meadow;
    (void)b_hills;

    // 9. Precompute A* settlement-to-settlement routes for fast multi-century caravans & armies
    for (std::size_t i = 0; i < settlement_entities.size(); ++i) {
        for (std::size_t j = 0; j < settlement_entities.size(); ++j) {
            if (i == j) {
                continue;
            }
            const GridPos pa = registry.get<ecs::GridPosComponent>(settlement_entities[i]).pos;
            const GridPos pb = registry.get<ecs::GridPosComponent>(settlement_entities[j]).pos;
            auto cached = findPath(map, config, pa, pb, false);
            if (!cached.empty()) {
                map.cacheRoute(pa, pb, std::move(cached));
            }
        }
    }

    return info;
}

} // namespace sovereignia::world
