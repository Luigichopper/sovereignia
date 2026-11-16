#include "sovereignia/sim/economy_system.hpp"

#include "sovereignia/ecs/components.hpp"
#include "sovereignia/world/pathfinding.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <span>
#include <string>
#include <vector>

namespace sovereignia::sim {

namespace {

struct BuildingSpec {
    BuildingType type = BuildingType::None;
    double timber = 0.0;
    double silver = 0.0;
    double iron = 0.0;
    double crystals = 0.0;
    int months = 3;
};

constexpr std::array<BuildingSpec, 5> kBuildingSpecs{{
    {BuildingType::StoneGranary,    35.0, 20.0,  0.0,  0.0, 3},
    {BuildingType::Scriptorium,     30.0, 25.0,  0.0,  6.0, 3},
    {BuildingType::KnightlyArmory,  35.0, 25.0, 30.0,  0.0, 4},
    {BuildingType::AetherWardTower, 35.0, 25.0, 15.0, 14.0, 4},
    {BuildingType::HighwayPaving,   45.0, 30.0, 15.0,  0.0, 3}
}};

const BuildingSpec* findBuildingSpec(BuildingType b) noexcept {
    for (const auto& spec : kBuildingSpecs) {
        if (spec.type == b) {
            return &spec;
        }
    }
    return nullptr;
}

const std::vector<GridPos>* findDirtRoadRoute(const entt::registry& registry,
                                              const world::WorldMap& map,
                                              entt::entity settlement) {
    if (settlement == entt::null || !registry.valid(settlement) ||
        !registry.all_of<ecs::GridPosComponent>(settlement)) {
        return nullptr;
    }
    const GridPos p_orig = registry.get<ecs::GridPosComponent>(settlement).pos;
    auto hasDirtRoad = [&](const std::vector<GridPos>* path) -> bool {
        if (path == nullptr || path->size() < 2) {
            return false;
        }
        for (const GridPos& p : *path) {
            if (map.inBounds(p) && map.at(p).settlement_entity == entt::null &&
                map.at(p).road_level == RoadLevel::DirtRoad) {
                return true;
            }
        }
        return false;
    };

    // Prioritize route toward liege/capital first
    if (registry.all_of<ecs::SettlementComponent>(settlement)) {
        const entt::entity liege = registry.get<ecs::SettlementComponent>(settlement).liege_settlement;
        if (liege != entt::null && liege != settlement && registry.valid(liege) &&
            registry.all_of<ecs::GridPosComponent>(liege)) {
            const GridPos p_liege = registry.get<ecs::GridPosComponent>(liege).pos;
            if (const auto* route = map.findCachedRoute(p_orig, p_liege); hasDirtRoad(route)) {
                return route;
            }
        }
    }

    const auto view = registry.view<ecs::SettlementComponent, ecs::GridPosComponent>();
    for (const entt::entity other : view) {
        if (other == settlement) {
            continue;
        }
        const GridPos p_other = view.get<ecs::GridPosComponent>(other).pos;
        if (const auto* route = map.findCachedRoute(p_orig, p_other); hasDirtRoad(route)) {
            return route;
        }
    }
    return nullptr;
}

bool isBuildingEligible(const entt::registry& registry,
                        const world::WorldMap& map,
                        entt::entity settlement,
                        const ecs::SettlementComponent& sett,
                        BuildingType b) {
    switch (b) {
        case BuildingType::StoneGranary: return sett.granary_level < 2;
        case BuildingType::Scriptorium: return sett.scriptorium_level < 2;
        case BuildingType::KnightlyArmory: return sett.armory_level < 2;
        case BuildingType::AetherWardTower: return sett.ward_tower_level < 2;
        case BuildingType::HighwayPaving: return findDirtRoadRoute(registry, map, settlement) != nullptr;
        case BuildingType::None: return false;
    }
    return false;
}

bool canAffordSpec(const ecs::ResourceStockpileComponent& stock, const BuildingSpec& spec) noexcept {
    return stock.get(ResourceType::Timber) >= spec.timber &&
           stock.get(ResourceType::Silver) >= spec.silver &&
           stock.get(ResourceType::Iron) >= spec.iron &&
           stock.get(ResourceType::MagicCrystals) >= spec.crystals;
}

BuildingType selectPriorityBuilding(const entt::registry& registry,
                                    const world::WorldMap& map,
                                    entt::entity settlement,
                                    const ecs::SettlementComponent& sett,
                                    const ecs::ResourceStockpileComponent& stock,
                                    bool allow_subsidy) {
    static constexpr std::array<BuildingType, 5> kBishopricOrder{
        BuildingType::Scriptorium, BuildingType::StoneGranary, BuildingType::HighwayPaving,
        BuildingType::AetherWardTower, BuildingType::KnightlyArmory};
    static constexpr std::array<BuildingType, 5> kCapitalOrder{
        BuildingType::StoneGranary, BuildingType::KnightlyArmory, BuildingType::Scriptorium,
        BuildingType::AetherWardTower, BuildingType::HighwayPaving};
    static constexpr std::array<BuildingType, 5> kFortressOrder{
        BuildingType::KnightlyArmory, BuildingType::AetherWardTower, BuildingType::StoneGranary,
        BuildingType::HighwayPaving, BuildingType::Scriptorium};
    static constexpr std::array<BuildingType, 5> kVillageOrder{
        BuildingType::StoneGranary, BuildingType::HighwayPaving, BuildingType::KnightlyArmory,
        BuildingType::Scriptorium, BuildingType::AetherWardTower};

    std::span<const BuildingType> order = kVillageOrder;
    if (sett.type == SettlementType::Bishopric) {
        order = kBishopricOrder;
    } else if (sett.type == SettlementType::Capital) {
        order = kCapitalOrder;
    } else if (sett.type == SettlementType::Fortress || sett.type == SettlementType::MiningCamp) {
        order = kFortressOrder;
    }

    for (const BuildingType cand : order) {
        if (!isBuildingEligible(registry, map, settlement, sett, cand)) {
            continue;
        }
        const BuildingSpec* spec = findBuildingSpec(cand);
        if (spec == nullptr) {
            continue;
        }
        if (allow_subsidy || canAffordSpec(stock, *spec)) {
            return cand;
        }
    }
    return BuildingType::None;
}

double seasonalFarmingMultiplier(Season s) noexcept {
    switch (s) {
        case Season::Spring: return 0.45;
        case Season::Summer: return 0.90;
        case Season::Autumn: return 3.60;
        case Season::Winter: return 0.15;
    }
    return 1.0;
}

double seasonalFirewoodBurnPerPop(Season s) noexcept {
    switch (s) {
        case Season::Spring: return 0.008;
        case Season::Summer: return 0.003;
        case Season::Autumn: return 0.009;
        case Season::Winter: return 0.025;
    }
    return 0.008;
}

} // namespace

bool EconomySystem::spawnCaravan(entt::registry& registry,
                                 const world::WorldMap& map,
                                 const data::ConfigRegistry& config,
                                 entt::entity origin,
                                 entt::entity destination,
                                 CaravanCargoKind kind,
                                 const std::array<double, kResourceCount>& cargo) {
    if (origin == entt::null || destination == entt::null || origin == destination) {
        return false;
    }
    if (!registry.valid(origin) || !registry.valid(destination)) {
        return false;
    }

    const auto& p_orig = registry.get<ecs::GridPosComponent>(origin).pos;
    const auto& p_dest = registry.get<ecs::GridPosComponent>(destination).pos;
    std::vector<GridPos> path;
    if (const auto* cached = map.findCachedRoute(p_orig, p_dest); cached != nullptr) {
        path = *cached;
    } else {
        path = world::findPath(map, config, p_orig, p_dest, false);
    }
    if (path.size() < 2) {
        return false;
    }

    auto& orig_stock = registry.get<ecs::ResourceStockpileComponent>(origin);
    for (std::size_t i = 0; i < kResourceCount; ++i) {
        if (cargo[i] > 0.0) {
            orig_stock.add(static_cast<ResourceType>(i), -cargo[i]);
        }
    }

    const entt::entity c_ent = registry.create();
    registry.emplace<ecs::NameComponent>(c_ent, std::string(toString(kind)));
    registry.emplace<ecs::GridPosComponent>(c_ent, p_orig);
    registry.emplace<ecs::CaravanComponent>(c_ent, ecs::CaravanComponent{
        origin,
        destination,
        kind,
        cargo,
        std::move(path),
        0
    });
    return true;
}

bool EconomySystem::startConstruction(entt::registry& registry,
                                      const world::WorldMap& map,
                                      entt::entity settlement,
                                      BuildingType preferred,
                                      const core::SimClock& clock,
                                      HistoryLog& log,
                                      bool allow_subsidy) {
    if (settlement == entt::null || !registry.valid(settlement) ||
        !registry.all_of<ecs::SettlementComponent, ecs::ResourceStockpileComponent, ecs::NameComponent>(settlement)) {
        return false;
    }
    auto& sett = registry.get<ecs::SettlementComponent>(settlement);
    auto& stock = registry.get<ecs::ResourceStockpileComponent>(settlement);
    const auto& s_name = registry.get<ecs::NameComponent>(settlement).value;

    if (sett.active_construction != BuildingType::None) {
        return false;
    }

    BuildingType chosen = preferred;
    if (chosen == BuildingType::None) {
        chosen = selectPriorityBuilding(registry, map, settlement, sett, stock, allow_subsidy);
    } else if (!isBuildingEligible(registry, map, settlement, sett, chosen)) {
        chosen = selectPriorityBuilding(registry, map, settlement, sett, stock, allow_subsidy);
    }

    if (chosen == BuildingType::None) {
        return false;
    }

    const BuildingSpec* spec = findBuildingSpec(chosen);
    if (spec == nullptr) {
        return false;
    }

    if (!allow_subsidy && !canAffordSpec(stock, *spec)) {
        return false;
    }

    if (spec->timber > 0.0) {
        stock.add(ResourceType::Timber, -std::min(stock.get(ResourceType::Timber), spec->timber));
    }
    if (spec->silver > 0.0) {
        stock.add(ResourceType::Silver, -std::min(stock.get(ResourceType::Silver), spec->silver));
    }
    if (spec->iron > 0.0) {
        stock.add(ResourceType::Iron, -std::min(stock.get(ResourceType::Iron), spec->iron));
    }
    if (spec->crystals > 0.0) {
        stock.add(ResourceType::MagicCrystals, -std::min(stock.get(ResourceType::MagicCrystals), spec->crystals));
    }

    sett.active_construction = chosen;
    sett.construction_months_left = spec->months;
    log.add(clock.year(), clock.season(), clock.month(), clock.day(),
            LogCategory::Economy,
            std::format("{} broke ground on {} ({} months).", s_name, toString(chosen), spec->months));
    return true;
}

DailyEconomyTotals EconomySystem::tickDaily(entt::registry& registry,
                                            const world::WorldMap& map,
                                            const data::ConfigRegistry& config,
                                            const core::SimClock& clock,
                                            HistoryLog& log) {
    DailyEconomyTotals totals{};
    const Season season = clock.season();
    const double farm_mult = seasonalFarmingMultiplier(season);
    const double wood_burn_per_pop = seasonalFirewoodBurnPerPop(season);
    constexpr double kFoodBurnPerPop = 0.018;

    auto view = registry.view<ecs::SettlementComponent,
                              ecs::GridPosComponent,
                              ecs::ResourceStockpileComponent,
                              ecs::DemographicsComponent>();

    for (const entt::entity s_ent : view) {
        auto& sett = view.get<ecs::SettlementComponent>(s_ent);
        const auto& pos = view.get<ecs::GridPosComponent>(s_ent).pos;
        auto& stock = view.get<ecs::ResourceStockpileComponent>(s_ent);
        auto& demo = view.get<ecs::DemographicsComponent>(s_ent);

        const auto& cell = map.at(pos);
        const auto& biome = config.biomeByIndex(cell.biome_index);
        const double river_bonus = cell.has_river ? 1.20 : 1.0;

        // 1. Primary cohort production
        for (const auto& cohort : demo.cohorts) {
            if (cohort.population <= 0) {
                continue;
            }
            const double pop = static_cast<double>(cohort.population);
            const double eff = 0.70 + 0.30 * cohort.fed_ratio;

            switch (cohort.type) {
                case CohortType::Farmers: {
                    const double prod = pop * 0.035 * biome.fertility * cell.soil_fertility * river_bonus * farm_mult * eff;
                    stock.add(ResourceType::Grain, prod);
                    totals.grain_produced += prod;
                    break;
                }
                case CohortType::Fishermen: {
                    const double fish_mult = (season == Season::Winter) ? 0.65 : 1.05;
                    const double site_yield = std::max(0.40, biome.fish_yield);
                    const double prod = pop * 0.034 * site_yield * fish_mult * eff;
                    stock.add(ResourceType::Fish, prod);
                    totals.fish_produced += prod;
                    break;
                }
                case CohortType::Miners: {
                    const double site_ore = std::max(0.55, biome.ore_yield);
                    const double iron = pop * 0.018 * site_ore * eff;
                    const double silver = pop * 0.0042 * site_ore * eff;
                    const double crystals = pop * 0.0018 * cell.mana_density * eff;
                    stock.add(ResourceType::Iron, iron);
                    stock.add(ResourceType::Silver, silver);
                    stock.add(ResourceType::MagicCrystals, crystals);
                    break;
                }
                case CohortType::Lumberjacks: {
                    const double site_wood = std::max(0.65, biome.timber_yield);
                    const double woodland_mult = std::clamp(0.65 + 0.85 * sett.local_woodland_cover, 0.65, 1.35);
                    const double policy_yield_mult =
                        (sett.forestry_policy == ForestryPolicy::IntensiveFelling) ? 1.15 :
                        (sett.forestry_policy == ForestryPolicy::WardenReforestation) ? 0.85 : 1.00;
                    const double wood_factor = site_wood * woodland_mult * policy_yield_mult * eff;
                    const double firewood = pop * 0.078 * wood_factor;
                    const double timber = pop * 0.015 * wood_factor;
                    stock.add(ResourceType::Firewood, firewood);
                    stock.add(ResourceType::Timber, timber);
                    totals.firewood_produced += firewood;
                    break;
                }
                case CohortType::Artisans: {
                    const double silver = pop * 0.0045 * (0.6 + 0.4 * sett.prosperity);
                    stock.add(ResourceType::Silver, silver);
                    break;
                }
                case CohortType::Count:
                    break;
            }
        }

        // 2. Daily food & firewood consumption
        const int total_pop = demo.totalPopulation();
        if (total_pop > 0) {
            if (season != Season::Winter) {
                const double deadfall = static_cast<double>(total_pop) * 0.0025;
                stock.add(ResourceType::Firewood, deadfall);
                totals.firewood_produced += deadfall;
            }

            const double food_need = static_cast<double>(total_pop) * kFoodBurnPerPop;
            const double wood_need = static_cast<double>(total_pop) * wood_burn_per_pop;

            // Consume Fish first (more perishable), then Grain
            const double fish_avail = stock.get(ResourceType::Fish);
            const double fish_used = std::min(fish_avail, food_need * 0.60);
            stock.add(ResourceType::Fish, -fish_used);

            const double rem_food_need = food_need - fish_used;
            const double grain_avail = stock.get(ResourceType::Grain);
            const double grain_used = std::min(grain_avail, rem_food_need);
            stock.add(ResourceType::Grain, -grain_used);

            const double food_supplied = fish_used + grain_used;
            totals.food_consumed += food_supplied;
            const double food_ratio = std::clamp(food_supplied / food_need, 0.0, 1.0);

            const double wood_avail = stock.get(ResourceType::Firewood);
            const double wood_used = std::min(wood_avail, wood_need);
            stock.add(ResourceType::Firewood, -wood_used);
            totals.firewood_consumed += wood_used;
            const double wood_ratio = std::clamp(wood_used / wood_need, 0.0, 1.0);

            for (auto& cohort : demo.cohorts) {
                cohort.fed_ratio = std::clamp(cohort.fed_ratio * 0.85 + food_ratio * 0.15, 0.0, 1.0);
                cohort.warmth_ratio = std::clamp(cohort.warmth_ratio * 0.85 + wood_ratio * 0.15, 0.0, 1.0);

                if (food_ratio < 0.75 || (season == Season::Winter && wood_ratio < 0.65)) {
                    cohort.unrest = std::min(1.0, cohort.unrest + 0.008);
                } else {
                    cohort.unrest = std::max(0.01, cohort.unrest - 0.003);
                }
            }
        }
    }

    // 3. Advance caravans along A* road paths
    std::vector<entt::entity> arrived;
    auto caravan_view = registry.view<ecs::CaravanComponent, ecs::GridPosComponent>();
    for (const entt::entity c_ent : caravan_view) {
        auto& car = caravan_view.get<ecs::CaravanComponent>(c_ent);
        auto& c_pos = caravan_view.get<ecs::GridPosComponent>(c_ent);

        if (car.path.empty()) {
            arrived.push_back(c_ent);
            continue;
        }

        const auto& cell = map.at(c_pos.pos);
        const int steps = (cell.road_level == RoadLevel::ImperialHighway && season != Season::Winter) ? 2 : 1;
        car.path_index = std::min(car.path.size() - 1, car.path_index + static_cast<std::size_t>(steps));
        c_pos.pos = car.path[car.path_index];

        if (car.path_index >= car.path.size() - 1) {
            arrived.push_back(c_ent);
        }
    }

    for (const entt::entity c_ent : arrived) {
        const auto& car = registry.get<ecs::CaravanComponent>(c_ent);
        if (registry.valid(car.destination) &&
            registry.all_of<ecs::ResourceStockpileComponent, ecs::NameComponent>(car.destination)) {
            auto& dest_stock = registry.get<ecs::ResourceStockpileComponent>(car.destination);
            for (std::size_t i = 0; i < kResourceCount; ++i) {
                if (car.cargo[i] > 0.0) {
                    dest_stock.add(static_cast<ResourceType>(i), car.cargo[i]);
                }
            }

            if (car.kind == CaravanCargoKind::GrainRelief || car.kind == CaravanCargoKind::TimberWagon) {
                const auto& dest_name = registry.get<ecs::NameComponent>(car.destination).value;
                const auto& orig_name = registry.valid(car.origin)
                    ? registry.get<ecs::NameComponent>(car.origin).value
                    : std::string("Royal Granary");
                const std::string res_desc = (car.kind == CaravanCargoKind::GrainRelief)
                    ? std::format("{:.0f}t grain", car.cargo[static_cast<std::size_t>(ResourceType::Grain)])
                    : std::format("{:.0f}t firewood", car.cargo[static_cast<std::size_t>(ResourceType::Firewood)]);
                log.add(clock.year(), clock.season(), clock.month(), clock.day(),
                        LogCategory::Economy,
                        std::format("{} relief convoy from {} arrived at {} ({}).",
                                    toString(car.kind), orig_name, dest_name, res_desc));
            } else if (car.kind == CaravanCargoKind::ImperialEnvoy) {
                for (const entt::entity ee : registry.view<ecs::OffMapEmpireComponent>()) {
                    auto& emp = registry.get<ecs::OffMapEmpireComponent>(ee);
                    if (emp.title.find("Empire") != std::string::npos) {
                        emp.favor = std::min(1.0, emp.favor + 0.07);
                        emp.relations = std::min(100, emp.relations + 7);
                        ++emp.envoys_exchanged;
                        std::string unlock_note;
                        if (emp.favor >= 0.70 && !emp.has_imperial_charter) {
                            emp.has_imperial_charter = true;
                            emp.tribute_owed_silver = std::max(80.0, emp.tribute_owed_silver * 0.85);
                            emp.stance = "Chartered Ally";
                            unlock_note = " (Imperial Charter granted!)";
                        }
                        if (!emp.has_trade_treaty && emp.relations >= 30) {
                            emp.has_trade_treaty = true;
                            if (unlock_note.empty()) {
                                unlock_note = " (Trade Treaty sealed!)";
                            }
                        }
                        log.add(clock.year(), clock.season(), clock.month(), clock.day(),
                                LogCategory::Diplomacy,
                                std::format("Nordmark envoy reached Heliodor's court at South Pass (Favor {:.0f}%){}.",
                                            emp.favor * 100.0, unlock_note));
                        break;
                    }
                }
            }
        }
        registry.destroy(c_ent);
    }

    return totals;
}

void EconomySystem::tickMonthly(entt::registry& registry,
                                world::WorldMap& map,
                                const data::ConfigRegistry& config,
                                const world::GeneratedWorldInfo& world_info,
                                const core::SimClock& clock,
                                HistoryLog& log) {
    // 1. Snapshot monthly deltas, advance buildings, pilgrimage revenue & armsmithing
    auto view = registry.view<ecs::NameComponent,
                              ecs::SettlementComponent,
                              ecs::ResourceStockpileComponent,
                              ecs::DemographicsComponent,
                              ecs::FeudalContractComponent>();

    std::vector<entt::entity> settlements;
    for (const entt::entity s_ent : view) {
        settlements.push_back(s_ent);
        const auto& s_name = view.get<ecs::NameComponent>(s_ent).value;
        auto& sett = view.get<ecs::SettlementComponent>(s_ent);
        auto& stock = view.get<ecs::ResourceStockpileComponent>(s_ent);
        auto& demo = view.get<ecs::DemographicsComponent>(s_ent);

        // Realistic granary spoilage & stockpile storage loss mitigated by Stone Granaries
        const double grain_cap = 2200.0 + static_cast<double>(sett.granary_level) * 800.0;
        const double fish_cap = 500.0 + static_cast<double>(sett.granary_level) * 250.0;
        const double spoil_mult = 1.0 / (1.0 + 0.45 * static_cast<double>(sett.granary_level));
        if (stock.get(ResourceType::Grain) > grain_cap) {
            stock.add(ResourceType::Grain, -(stock.get(ResourceType::Grain) - grain_cap) * 0.015 * spoil_mult);
        }
        if (stock.get(ResourceType::Fish) > fish_cap) {
            stock.add(ResourceType::Fish, -(stock.get(ResourceType::Fish) - fish_cap) * 0.025 * spoil_mult);
        }
        if (stock.get(ResourceType::Firewood) > 1800.0) {
            stock.add(ResourceType::Firewood, -(stock.get(ResourceType::Firewood) - 1800.0) * 0.010);
        }
        if (stock.get(ResourceType::Silver) > 1500.0) {
            stock.add(ResourceType::Silver, -(stock.get(ResourceType::Silver) - 1500.0) * 0.005);
        }

        // Pilgrimage site offerings & orthodoxy reinforcement
        if (sett.is_pilgrimage_site) {
            stock.add(ResourceType::Silver, 5.0);
            for (auto& cohort : demo.cohorts) {
                cohort.orthodoxy = std::min(1.0, cohort.orthodoxy + 0.012);
            }
        }

        // Advance active construction queue
        if (sett.active_construction != BuildingType::None) {
            --sett.construction_months_left;
            if (sett.construction_months_left <= 0) {
                const BuildingType done = sett.active_construction;
                sett.active_construction = BuildingType::None;
                sett.construction_months_left = 0;

                switch (done) {
                    case BuildingType::StoneGranary:
                        sett.granary_level = std::min(2, sett.granary_level + 1);
                        sett.prosperity = std::min(0.99, sett.prosperity + 0.03);
                        log.add(clock.year(), clock.season(), clock.month(), clock.day(),
                                LogCategory::Economy,
                                std::format("{} completed Stone Granary tier {} (reduced grain spoilage).",
                                            s_name, sett.granary_level));
                        break;
                    case BuildingType::Scriptorium:
                        sett.scriptorium_level = std::min(2, sett.scriptorium_level + 1);
                        for (auto& cohort : demo.cohorts) {
                            cohort.orthodoxy = std::min(1.0, cohort.orthodoxy + 0.04);
                        }
                        log.add(clock.year(), clock.season(), clock.month(), clock.day(),
                                LogCategory::Economy,
                                std::format("{} completed Monastic Scriptorium tier {} (strengthened parish orthodoxy).",
                                            s_name, sett.scriptorium_level));
                        break;
                    case BuildingType::KnightlyArmory:
                        sett.armory_level = std::min(2, sett.armory_level + 1);
                        sett.knights_ready += 3;
                        log.add(clock.year(), clock.season(), clock.month(), clock.day(),
                                LogCategory::Economy,
                                std::format("{} completed Knightly Armory tier {} (+3 Knights ready).",
                                            s_name, sett.armory_level));
                        break;
                    case BuildingType::AetherWardTower:
                        sett.ward_tower_level = std::min(2, sett.ward_tower_level + 1);
                        sett.mages_ready += 1;
                        sett.bandit_threat = std::max(0.01, sett.bandit_threat - 0.06);
                        log.add(clock.year(), clock.season(), clock.month(), clock.day(),
                                LogCategory::Economy,
                                std::format("{} consecrated Aether Ward Tower tier {} (+1 Court Mage).",
                                            s_name, sett.ward_tower_level));
                        break;
                    case BuildingType::HighwayPaving: {
                        int upgraded = 0;
                        if (const auto* route = findDirtRoadRoute(registry, map, s_ent)) {
                            for (const GridPos& p : *route) {
                                if (!map.inBounds(p)) continue;
                                auto& cell = map.at(p);
                                if (cell.settlement_entity == entt::null &&
                                    cell.road_level == RoadLevel::DirtRoad) {
                                    cell.road_level = RoadLevel::ImperialHighway;
                                    if (cell.has_river) {
                                        cell.has_bridge = true;
                                    }
                                    ++upgraded;
                                    if (upgraded >= 18) {
                                        break;
                                    }
                                }
                            }
                        }
                        log.add(clock.year(), clock.season(), clock.month(), clock.day(),
                                LogCategory::Economy,
                                std::format("{} paved {} tiles of Imperial Highway & bridges.", s_name, upgraded));
                        break;
                    }
                    case BuildingType::None:
                        break;
                }
            }
        } else if (stock.get(ResourceType::Timber) >= 55.0 && stock.get(ResourceType::Silver) >= 40.0) {
            // Autonomous lord construction using timber/iron/crystal/silver surpluses
            startConstruction(registry, map, s_ent, BuildingType::None, clock, log, false);
        }

        // Iron & MagicCrystal armsmithing sink
        if ((sett.armory_level >= 1 || sett.type == SettlementType::Fortress || sett.type == SettlementType::MiningCamp) &&
            stock.get(ResourceType::Iron) >= 32.0 && stock.get(ResourceType::Silver) >= 18.0) {
            stock.add(ResourceType::Iron, -10.0);
            stock.add(ResourceType::Silver, -4.0);
            if (stock.get(ResourceType::MagicCrystals) >= 8.0) {
                stock.add(ResourceType::MagicCrystals, -2.0);
            }
            ++sett.forged_arms_stock;
            if ((sett.forged_arms_stock % 3) == 0 && sett.knights_ready < 42) {
                ++sett.knights_ready;
            }
        }

        stock.updateMonthlyDelta();

        const double fed = demo.weightedFedRatio();
        const double unrest = demo.weightedUnrest();
        sett.prosperity = std::clamp(sett.prosperity * 0.90 + (fed * (1.0 - unrest * 0.6)) * 0.10, 0.15, 0.99);
        const double tower_reduction = static_cast<double>(sett.ward_tower_level) * 0.015;
        sett.bandit_threat = std::clamp(0.03 + unrest * 0.45 - (sett.type == SettlementType::Fortress ? 0.04 : 0.0) - tower_reduction,
                                        0.01, 0.85);

        const int farmers = demo.countOf(CohortType::Farmers);
        const int total_pop = demo.totalPopulation();
        sett.levy_spearmen_ready = std::max(10, static_cast<int>(
            (farmers * 0.45 + total_pop * 0.10) * (0.5 + 0.5 * fed)));
    }

    // Track active caravans so routine tribute/corvee pauses when saturated while vital relief continues
    std::size_t active_caravans = registry.view<ecs::CaravanComponent>().size();

    const entt::entity capital = world_info.capital_entity;
    const entt::entity monastery = world_info.monastery_entity;

    // 2. Feudal Corvee, Ecclesiastical Tithes & Inter-Settlement Market Trade
    for (const entt::entity s_ent : settlements) {
        const auto& name = view.get<ecs::NameComponent>(s_ent).value;
        const auto& stock = view.get<ecs::ResourceStockpileComponent>(s_ent);
        const auto& contract = view.get<ecs::FeudalContractComponent>(s_ent);
        const auto& demo = view.get<ecs::DemographicsComponent>(s_ent);

        const double pop = std::max(1.0, static_cast<double>(demo.totalPopulation()));
        const double daily_food = pop * 0.018;
        double food_days = (stock.get(ResourceType::Grain) + stock.get(ResourceType::Fish)) / daily_food;
        const double target_daily_wood = pop * std::max(seasonalFirewoodBurnPerPop(clock.season()), 0.014);
        const double effective_wood_days = stock.get(ResourceType::Firewood) / target_daily_wood;

        if (active_caravans < 36 && s_ent != capital && capital != entt::null) {
            std::array<double, kResourceCount> corvee{};
            bool send = false;
            if (stock.get(ResourceType::Grain) > 220.0 && food_days > 65.0) {
                corvee[static_cast<std::size_t>(ResourceType::Grain)] =
                    std::min(36.0, (stock.get(ResourceType::Grain) - 120.0) * contract.corvee_rate);
                send = true;
            }
            if (stock.get(ResourceType::Iron) > 85.0) {
                corvee[static_cast<std::size_t>(ResourceType::Iron)] =
                    std::min(22.0, (stock.get(ResourceType::Iron) - 60.0) * contract.corvee_rate);
                send = true;
            }
            if (stock.get(ResourceType::Silver) > 45.0) {
                corvee[static_cast<std::size_t>(ResourceType::Silver)] =
                    std::min(12.0, (stock.get(ResourceType::Silver) - 30.0) * contract.corvee_rate);
                send = true;
            }
            if (send && spawnCaravan(registry, map, config, s_ent, capital, CaravanCargoKind::Corvee, corvee)) {
                ++active_caravans;
            }
        }

        if (active_caravans < 36 && s_ent != monastery && monastery != entt::null &&
            (clock.month() % 2 == 1) && stock.get(ResourceType::Grain) > 200.0 && food_days > 65.0) {
            std::array<double, kResourceCount> tithe{};
            tithe[static_cast<std::size_t>(ResourceType::Grain)] =
                std::min(16.0, (stock.get(ResourceType::Grain) - 140.0) * contract.tithe_rate);
            if (stock.get(ResourceType::Fish) > 70.0) {
                tithe[static_cast<std::size_t>(ResourceType::Fish)] = 8.0;
            }
            if (spawnCaravan(registry, map, config, s_ent, monastery, CaravanCargoKind::Tithe, tithe)) {
                ++active_caravans;
            }
        }

        food_days = (stock.get(ResourceType::Grain) + stock.get(ResourceType::Fish)) / daily_food;

        // Inter-settlement market food trade when reserves dip below 65 days
        if (food_days < 65.0 && active_caravans < 84) {
            const double needed_food = std::clamp((70.0 - food_days) * daily_food, 75.0, 280.0);
            entt::entity best_grain_seller = entt::null;
            double best_grain = 260.0;
            entt::entity best_fish_seller = entt::null;
            double best_fish = 150.0;

            for (const entt::entity cand : settlements) {
                if (cand == s_ent) {
                    continue;
                }
                const auto& c_stock = view.get<ecs::ResourceStockpileComponent>(cand);
                if (c_stock.get(ResourceType::Grain) > best_grain) {
                    best_grain = c_stock.get(ResourceType::Grain);
                    best_grain_seller = cand;
                }
                if (c_stock.get(ResourceType::Fish) > best_fish) {
                    best_fish = c_stock.get(ResourceType::Fish);
                    best_fish_seller = cand;
                }
            }

            bool sent_grain = false;
            if (best_grain_seller != entt::null) {
                const double grain_qty = std::clamp(best_grain - 190.0, 60.0, needed_food);
                std::array<double, kResourceCount> cargo{};
                cargo[static_cast<std::size_t>(ResourceType::Grain)] = grain_qty;
                if (spawnCaravan(registry, map, config, best_grain_seller, s_ent, CaravanCargoKind::MarketTrade, cargo)) {
                    ++active_caravans;
                    sent_grain = true;
                    auto& buyer_stock = view.get<ecs::ResourceStockpileComponent>(s_ent);
                    auto& seller_stock = view.get<ecs::ResourceStockpileComponent>(best_grain_seller);
                    const double silver_paid = std::min(16.0, std::max(0.0, buyer_stock.get(ResourceType::Silver) - 10.0));
                    if (silver_paid > 0.0) {
                        buyer_stock.add(ResourceType::Silver, -silver_paid);
                        seller_stock.add(ResourceType::Silver, silver_paid);
                    }
                }
            }
            if ((!sent_grain || food_days < 40.0) && best_fish_seller != entt::null && active_caravans < 84) {
                const double fish_qty = std::clamp(best_fish - 100.0, 45.0, 150.0);
                std::array<double, kResourceCount> cargo{};
                cargo[static_cast<std::size_t>(ResourceType::Fish)] = fish_qty;
                if (spawnCaravan(registry, map, config, best_fish_seller, s_ent, CaravanCargoKind::MarketTrade, cargo)) {
                    ++active_caravans;
                    auto& buyer_stock = view.get<ecs::ResourceStockpileComponent>(s_ent);
                    auto& seller_stock = view.get<ecs::ResourceStockpileComponent>(best_fish_seller);
                    const double silver_paid = std::min(10.0, std::max(0.0, buyer_stock.get(ResourceType::Silver) - 10.0));
                    if (silver_paid > 0.0) {
                        buyer_stock.add(ResourceType::Silver, -silver_paid);
                        seller_stock.add(ResourceType::Silver, silver_paid);
                    }
                }
            }
        }

        // Emergency royal grain relief if critically low (<35 days)
        if (food_days < 35.0 && capital != entt::null && s_ent != capital && active_caravans < 84) {
            const auto& cap_stock = registry.get<ecs::ResourceStockpileComponent>(capital);
            if (cap_stock.get(ResourceType::Grain) > 260.0) {
                const double relief_qty = std::clamp(daily_food * 35.0, 75.0, 190.0);
                std::array<double, kResourceCount> relief{};
                relief[static_cast<std::size_t>(ResourceType::Grain)] = relief_qty;
                if (spawnCaravan(registry, map, config, capital, s_ent, CaravanCargoKind::GrainRelief, relief)) {
                    ++active_caravans;
                    log.add(clock.year(), clock.season(), clock.month(), clock.day(),
                            LogCategory::Feudal,
                            std::format("Margrave dispatched {:.0f}t emergency grain relief to {}.", relief_qty, name));
                }
            }
        }

        // Inter-settlement firewood sleds when wood drops below 55 days
        if (effective_wood_days < 55.0 && active_caravans < 84) {
            const double needed_wood = std::clamp((65.0 - effective_wood_days) * target_daily_wood, 70.0, 320.0);
            entt::entity best_wood_seller = entt::null;
            double best_wood = 220.0;
            for (const entt::entity cand : settlements) {
                if (cand == s_ent) {
                    continue;
                }
                const auto& c_stock = view.get<ecs::ResourceStockpileComponent>(cand);
                if (c_stock.get(ResourceType::Firewood) > best_wood) {
                    best_wood = c_stock.get(ResourceType::Firewood);
                    best_wood_seller = cand;
                }
            }
            if (best_wood_seller != entt::null) {
                const double wood_qty = std::clamp(best_wood - 170.0, 65.0, needed_wood);
                std::array<double, kResourceCount> wood_relief{};
                wood_relief[static_cast<std::size_t>(ResourceType::Firewood)] = wood_qty;
                if (spawnCaravan(registry, map, config, best_wood_seller, s_ent,
                                 CaravanCargoKind::TimberWagon, wood_relief)) {
                    ++active_caravans;
                    auto& buyer_stock = view.get<ecs::ResourceStockpileComponent>(s_ent);
                    auto& seller_stock = view.get<ecs::ResourceStockpileComponent>(best_wood_seller);
                    const double silver_paid = std::min(12.0, std::max(0.0, buyer_stock.get(ResourceType::Silver) - 10.0));
                    if (silver_paid > 0.0) {
                        buyer_stock.add(ResourceType::Silver, -silver_paid);
                        seller_stock.add(ResourceType::Silver, silver_paid);
                    }
                    if (effective_wood_days < 24.0) {
                        const auto& seller_name = view.get<ecs::NameComponent>(best_wood_seller).value;
                        log.add(clock.year(), clock.season(), clock.month(), clock.day(),
                                LogCategory::Economy,
                                std::format("{} timber sleds hauling {:.0f}t firewood to {}.", seller_name, wood_qty, name));
                    }
                }
            }
        }
    }

    // 3. Imperial Trade Treaty Convoy (Month 5) & Southern Empire Silver Tribute dispatch in Autumn (Month 9)
    if (clock.month() == 5 && capital != entt::null && world_info.south_pass_entity != entt::null &&
        world_info.empire_entity != entt::null && registry.valid(world_info.empire_entity)) {
        const auto& emp = registry.get<ecs::OffMapEmpireComponent>(world_info.empire_entity);
        if (emp.has_trade_treaty || emp.favor >= 0.65) {
            auto& sp_stock = registry.get<ecs::ResourceStockpileComponent>(world_info.south_pass_entity);
            sp_stock.add(ResourceType::Silver, 30.0);
            sp_stock.add(ResourceType::Grain, 60.0);
            std::array<double, kResourceCount> trade_cargo{};
            trade_cargo[static_cast<std::size_t>(ResourceType::Silver)] = 30.0;
            trade_cargo[static_cast<std::size_t>(ResourceType::Grain)] = 60.0;
            if (spawnCaravan(registry, map, config, world_info.south_pass_entity, capital,
                             CaravanCargoKind::MarketTrade, trade_cargo)) {
                log.add(clock.year(), clock.season(), clock.month(), clock.day(),
                        LogCategory::Diplomacy,
                        "Imperial Trade Treaty convoy departed South Pass for Vaelgard (60t Grain, 30 Ag).");
            }
        }
    }

    if (clock.month() == 9 && capital != entt::null && world_info.south_pass_entity != entt::null &&
        world_info.empire_entity != entt::null) {
        auto& cap_stock = registry.get<ecs::ResourceStockpileComponent>(capital);
        auto& emp = registry.get<ecs::OffMapEmpireComponent>(world_info.empire_entity);
        const double payment = std::min(emp.tribute_owed_silver * 0.5, std::max(0.0, cap_stock.get(ResourceType::Silver) - 25.0));
        if (payment >= 15.0) {
            std::array<double, kResourceCount> trib{};
            trib[static_cast<std::size_t>(ResourceType::Silver)] = payment;
            if (spawnCaravan(registry, map, config, capital, world_info.south_pass_entity,
                             CaravanCargoKind::ImperialTribute, trib)) {
                emp.tribute_paid_this_year += payment;
                emp.favor = std::min(1.0, emp.favor + 0.04);
                log.add(clock.year(), clock.season(), clock.month(), clock.day(),
                        LogCategory::Diplomacy,
                        std::format("Imperial tribute wagon ({:.0f} silver) departed Vaelgard for South Pass.", payment));
            }
        }
    }
}

void EconomySystem::tickSeasonal(entt::registry& registry,
                                 const core::SimClock& clock,
                                 core::Rng& rng,
                                 HistoryLog& log) {
    double total_grain = 0.0;
    double total_wood = 0.0;
    int total_pop = 0;

    auto view = registry.view<ecs::NameComponent,
                              ecs::ResourceStockpileComponent,
                              ecs::DemographicsComponent>();

    for (const entt::entity s_ent : view) {
        const auto& name = view.get<ecs::NameComponent>(s_ent).value;
        const auto& stock = view.get<ecs::ResourceStockpileComponent>(s_ent);
        auto& demo = view.get<ecs::DemographicsComponent>(s_ent);

        total_grain += stock.get(ResourceType::Grain);
        total_wood += stock.get(ResourceType::Firewood);

        int delta_sum = 0;
        const int prev_pop = demo.totalPopulation();
        const double crowd_factor = std::clamp(1.0 - static_cast<double>(std::max(0, prev_pop - 580)) / 280.0, 0.0, 1.0);
        for (auto& cohort : demo.cohorts) {
            if (cohort.population <= 0) {
                continue;
            }
            if (cohort.fed_ratio >= 0.80 && cohort.warmth_ratio >= 0.78 && prev_pop < 880) {
                const int raw_growth = std::max(1, static_cast<int>(cohort.population * rng.nextRange(0.004, 0.010)));
                const int growth = static_cast<int>(std::round(static_cast<double>(raw_growth) * crowd_factor));
                cohort.population += growth;
                delta_sum += growth;
            } else if (cohort.fed_ratio < 0.45 || cohort.warmth_ratio < 0.38) {
                const int min_floor = (cohort.population >= 25) ? 20 : 8;
                const int loss = std::max(1, static_cast<int>(cohort.population * rng.nextRange(0.010, 0.028)));
                const int new_pop = std::max(min_floor, cohort.population - loss);
                delta_sum -= (cohort.population - new_pop);
                cohort.population = new_pop;
            }
            total_pop += cohort.population;
        }

        if (delta_sum <= -8) {
            const Season prev_season = static_cast<Season>((static_cast<int>(clock.season()) + 3) % 4);
            log.add(clock.year(), clock.season(), clock.month(), clock.day(),
                    LogCategory::Season,
                    std::format("Hardship claimed {} souls in {} during {} shortages.",
                                -delta_sum, name, toString(prev_season)));
        }
    }

    log.add(clock.year(), clock.season(), clock.month(), clock.day(),
            LogCategory::Season,
            std::format("{} begins across Nordmark (Pop: {}, Grain: {:.0f}t, Firewood: {:.0f}t).",
                        toString(clock.season()), total_pop, total_grain, total_wood));
}

void EconomySystem::tickYearly(entt::registry& registry,
                               const world::GeneratedWorldInfo& world_info,
                               const core::SimClock& clock,
                               HistoryLog& log) {
    // Decrement off-map truces
    for (const entt::entity ee : registry.view<ecs::OffMapEmpireComponent>()) {
        auto& emp = registry.get<ecs::OffMapEmpireComponent>(ee);
        if (emp.truce_years_left > 0) {
            --emp.truce_years_left;
            if (emp.truce_years_left == 0 && emp.title.find("Ostmark") != std::string::npos) {
                emp.stance = "Testing Northern Passes";
            }
        }
    }

    // Audit Imperial Tribute & Favor (aging, dynasties, laws, and theology live in dedicated systems)
    if (world_info.empire_entity != entt::null) {
        auto& emp = registry.get<ecs::OffMapEmpireComponent>(world_info.empire_entity);
        emp.favor += (0.55 - emp.favor) * 0.15;
        if (emp.tribute_paid_this_year >= emp.tribute_owed_silver * 0.45) {
            emp.relations = std::min(100, emp.relations + 3);
            emp.stance = emp.has_imperial_charter ? "Chartered Ally" : "Contented Suzerain";
        } else {
            emp.favor = std::max(0.05, emp.favor - 0.06);
            emp.relations = std::max(-100, emp.relations - 6);
            emp.stance = "Displeased over Arrears";
            log.add(clock.year(), clock.season(), clock.month(), clock.day(),
                    LogCategory::Diplomacy,
                    std::format("Southern Empire envoy rebuked Nordmark over silver tribute arrears ({:.0f}/{:.0f}).",
                                emp.tribute_paid_this_year, emp.tribute_owed_silver));
        }
        emp.tribute_paid_this_year = 0.0;
    }
}

} // namespace sovereignia::sim

