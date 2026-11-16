#include "sovereignia/sim/governance_war_system.hpp"

#include "sovereignia/sim/economy_system.hpp"
#include "sovereignia/world/pathfinding.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <span>
#include <vector>

namespace sovereignia::sim {

namespace {

double swordMultiplier(SkillRank rank) noexcept {
    switch (rank) {
        case SkillRank::None: return 1.00;
        case SkillRank::Beginner: return 1.05;
        case SkillRank::Intermediate: return 1.15;
        case SkillRank::Advanced: return 1.25;
        case SkillRank::Saint: return 1.40;
        case SkillRank::King: return 1.70;
        case SkillRank::Emperor: return 2.10;
    }
    return 1.0;
}

std::string replaceToken(std::string text, std::string_view token, std::string_view value) {
    std::size_t pos = 0;
    while ((pos = text.find(token, pos)) != std::string::npos) {
        text.replace(pos, token.size(), value);
        pos += value.size();
    }
    return text;
}

void capLaws(entt::registry& registry) {
    std::vector<entt::entity> non_charters;
    auto view = registry.view<ecs::ActiveLawComponent>();
    for (const entt::entity le : view) {
        if (view.get<ecs::ActiveLawComponent>(le).kind != "charter") {
            non_charters.push_back(le);
        }
    }
    if (non_charters.size() <= 26) {
        return;
    }
    std::sort(non_charters.begin(), non_charters.end(), [&](entt::entity a, entt::entity b) {
        return view.get<ecs::ActiveLawComponent>(a).enacted_year <
               view.get<ecs::ActiveLawComponent>(b).enacted_year;
    });
    const std::size_t remove_count = non_charters.size() - 26;
    for (std::size_t i = 0; i < remove_count; ++i) {
        registry.destroy(non_charters[i]);
    }
}

void stampBattleSite(world::WorldMap& map,
                     const data::ConfigRegistry& config,
                     GridPos center,
                     MapSiteKind kind,
                     const std::string& label) {
    int existing_sites = 0;
    for (int y = 0; y < map.height(); ++y) {
        for (int x = 0; x < map.width(); ++x) {
            if (map.at(x, y).site_kind == MapSiteKind::BattleSite) {
                ++existing_sites;
            }
        }
    }
    if (existing_sites >= 14) {
        return;
    }

    for (int rad = 1; rad <= 4; ++rad) {
        for (int dy = -rad; dy <= rad; ++dy) {
            for (int dx = -rad; dx <= rad; ++dx) {
                const GridPos p{center.x + dx, center.y + dy};
                if (!map.inBounds(p)) {
                    continue;
                }
                auto& c = map.at(p);
                if (config.biomeByIndex(c.biome_index).walkable &&
                    c.settlement_entity == entt::null &&
                    c.site_kind == MapSiteKind::None) {
                    c.site_kind = kind;
                    c.site_name = label;
                    return;
                }
            }
        }
    }
}

std::vector<GridPos> computeOrCachedPath(const world::WorldMap& map,
                                         const data::ConfigRegistry& config,
                                         GridPos start,
                                         GridPos goal) {
    if (start == goal) {
        return {start};
    }
    if (const auto* cached = map.findCachedRoute(start, goal); cached != nullptr && !cached->empty()) {
        return *cached;
    }
    return world::findPath(map, config, start, goal, false);
}

void demobilizeArmy(entt::registry& registry, const ecs::ArmyComponent& army) {
    if (army.origin_settlement == entt::null || !registry.valid(army.origin_settlement)) {
        return;
    }
    if (registry.all_of<ecs::DemographicsComponent>(army.origin_settlement) && army.levied_spearmen > 0) {
        auto& demo = registry.get<ecs::DemographicsComponent>(army.origin_settlement);
        for (auto& cohort : demo.cohorts) {
            if (cohort.type == CohortType::Farmers) {
                cohort.population += army.levied_spearmen;
                break;
            }
        }
    }
    if (registry.all_of<ecs::ResourceStockpileComponent>(army.origin_settlement)) {
        auto& orig_stock = registry.get<ecs::ResourceStockpileComponent>(army.origin_settlement);
        if (army.loot_silver > 0.0) {
            orig_stock.add(ResourceType::Silver, army.loot_silver);
        }
        if (army.is_counter_raid) {
            orig_stock.add(ResourceType::Iron, 18.0);
        }
    }
}

} // namespace

entt::entity GovernanceWarSystem::mobilizeBanner(entt::registry& registry,
                                                 const world::WorldMap& map,
                                                 const data::ConfigRegistry& config,
                                                 entt::entity origin_settlement,
                                                 entt::entity target_settlement,
                                                 const std::string& banner_name,
                                                 int max_levy_fraction_pct) {
    if (origin_settlement == entt::null || !registry.valid(origin_settlement) ||
        !registry.all_of<ecs::SettlementComponent, ecs::DemographicsComponent, ecs::GridPosComponent>(origin_settlement)) {
        return entt::null;
    }

    auto& sett = registry.get<ecs::SettlementComponent>(origin_settlement);
    auto& demo = registry.get<ecs::DemographicsComponent>(origin_settlement);
    const auto& contract = registry.get<ecs::FeudalContractComponent>(origin_settlement);
    const GridPos orig_pos = registry.get<ecs::GridPosComponent>(origin_settlement).pos;

    int levied_spears = 0;
    if (!contract.exempt_from_levy) {
        for (auto& cohort : demo.cohorts) {
            if (cohort.type == CohortType::Farmers && cohort.population > 25) {
                const int cap = (cohort.population * std::clamp(max_levy_fraction_pct, 10, 50)) / 100;
                const int pull = std::min({sett.levy_spearmen_ready, cap, cohort.population - 15});
                if (pull > 0) {
                    cohort.population -= pull;
                    levied_spears = pull;
                }
                break;
            }
        }
    }

    if (levied_spears == 0 && sett.knights_ready <= 0) {
        return entt::null;
    }

    SkillRank cap_rank = SkillRank::Beginner;
    bool healer = false;
    bool silent_mage = false;
    if (sett.ruler_entity != entt::null && registry.valid(sett.ruler_entity) &&
        registry.all_of<ecs::MartialMagicComponent>(sett.ruler_entity)) {
        const auto& mm = registry.get<ecs::MartialMagicComponent>(sett.ruler_entity);
        cap_rank = std::max(cap_rank, mm.sword_rank);
        if (mm.magic_school.find("Healing") != std::string::npos && mm.magic_rank >= SkillRank::Intermediate) {
            healer = true;
        }
        if (mm.incantationless) {
            silent_mage = true;
        }
        if (registry.all_of<ecs::HouseholdComponent>(sett.ruler_entity)) {
            for (const entt::entity me : registry.get<ecs::HouseholdComponent>(sett.ruler_entity).member_entities) {
                if (registry.valid(me) && registry.all_of<ecs::MartialMagicComponent>(me)) {
                    const auto& m_mm = registry.get<ecs::MartialMagicComponent>(me);
                    cap_rank = std::max(cap_rank, m_mm.sword_rank);
                    if (m_mm.magic_school.find("Healing") != std::string::npos && m_mm.magic_rank >= SkillRank::Intermediate) {
                        healer = true;
                    }
                    if (m_mm.incantationless) {
                        silent_mage = true;
                    }
                }
            }
        }
    }

    GridPos dest_pos = orig_pos;
    if (target_settlement != entt::null && registry.valid(target_settlement) &&
        registry.all_of<ecs::GridPosComponent>(target_settlement)) {
        dest_pos = registry.get<ecs::GridPosComponent>(target_settlement).pos;
    }

    std::vector<GridPos> path;
    if (orig_pos == dest_pos) {
        // Patrol step onto adjacent walkable cell and back
        const GridPos cand{std::min(map.width() - 2, orig_pos.x + 1), orig_pos.y};
        if (map.inBounds(cand) && config.biomeByIndex(map.at(cand).biome_index).walkable) {
            path = {orig_pos, cand, orig_pos};
        } else {
            path = {orig_pos, orig_pos};
        }
    } else {
        path = computeOrCachedPath(map, config, orig_pos, dest_pos);
        if (path.empty()) {
            path = {orig_pos};
        }
    }

    const entt::entity a_ent = registry.create();
    registry.emplace<ecs::NameComponent>(a_ent, banner_name);
    registry.emplace<ecs::GridPosComponent>(a_ent, orig_pos);
    registry.emplace<ecs::ArmyComponent>(a_ent, ecs::ArmyComponent{
        banner_name,
        sett.ruler_entity,
        origin_settlement,
        target_settlement != entt::null ? target_settlement : origin_settlement,
        false,
        false,
        levied_spears,
        std::max(2, sett.knights_ready + sett.armory_level * 2),
        std::max(0, sett.mages_ready),
        cap_rank,
        healer,
        silent_mage,
        0.0,
        std::move(path),
        0
    });
    return a_ent;
}

void GovernanceWarSystem::tickDaily(entt::registry& registry,
                                    world::WorldMap& map,
                                    const data::ConfigRegistry& config,
                                    const world::GeneratedWorldInfo& world_info,
                                    const core::SimClock& clock,
                                    core::Rng& rng,
                                    HistoryLog& log) {
    std::vector<entt::entity> armies;
    for (const entt::entity ae : registry.view<ecs::ArmyComponent>()) {
        armies.push_back(ae);
    }
    if (armies.empty()) {
        return;
    }

    std::vector<entt::entity> to_destroy;

    // 1. Advance active armies
    for (const entt::entity ae : armies) {
        if (!registry.valid(ae)) continue;
        auto& army = registry.get<ecs::ArmyComponent>(ae);
        auto& pos = registry.get<ecs::GridPosComponent>(ae).pos;

        if (!army.path.empty() && army.path_index + 1 < army.path.size()) {
            const auto& cell = map.at(pos);
            const int step = (cell.road_level == RoadLevel::ImperialHighway && clock.season() != Season::Winter) ? 2 : 1;
            army.path_index = std::min(army.path.size() - 1, army.path_index + static_cast<std::size_t>(step));
            pos = army.path[army.path_index];
        }
    }

    // 2. Check field interceptions between Nordmark banners and foreign raiders
    for (std::size_t i = 0; i < armies.size(); ++i) {
        const entt::entity a1 = armies[i];
        if (!registry.valid(a1)) continue;
        auto& arm1 = registry.get<ecs::ArmyComponent>(a1);
        if (!arm1.is_foreign_raider || arm1.levied_spearmen <= 0) continue;
        const GridPos p1 = registry.get<ecs::GridPosComponent>(a1).pos;

        for (std::size_t j = 0; j < armies.size(); ++j) {
            if (i == j) continue;
            const entt::entity a2 = armies[j];
            if (!registry.valid(a2)) continue;
            auto& arm2 = registry.get<ecs::ArmyComponent>(a2);
            if (arm2.is_foreign_raider || arm2.returning_home || arm2.levied_spearmen <= 0) continue;
            const GridPos p2 = registry.get<ecs::GridPosComponent>(a2).pos;

            if (std::abs(p1.x - p2.x) + std::abs(p1.y - p2.y) <= 2) {
                const double pow1 = (arm1.levied_spearmen * 1.0 + arm1.armored_knights * 6.5 + arm1.court_mages * 15.0)
                                    * swordMultiplier(arm1.captain_sword_rank);
                const double pow2 = (arm2.levied_spearmen * 1.0 + arm2.armored_knights * 7.0 + arm2.court_mages * 16.0)
                                    * swordMultiplier(arm2.captain_sword_rank)
                                    * (arm2.has_incantationless_mage ? 1.25 : 1.0);
                const bool nordmark_won = (pow2 * rng.nextRange(0.88, 1.15)) >= pow1;
                const double casualty_rate2 = (nordmark_won ? 0.06 : 0.16) * (arm2.has_healer_mage ? 0.60 : 1.0);
                arm2.levied_spearmen = std::max(5, static_cast<int>(arm2.levied_spearmen * (1.0 - casualty_rate2)));

                if (nordmark_won) {
                    arm2.loot_silver += rng.nextRange(18.0, 42.0);
                    arm2.returning_home = true;
                    if (arm2.path_index + 1 < arm2.path.size()) {
                        arm2.path.resize(arm2.path_index + 1);
                    }
                    std::reverse(arm2.path.begin(), arm2.path.end());
                    arm2.path_index = 0;
                    stampBattleSite(map, config, p1, MapSiteKind::BattleSite,
                                    std::format("Battle vs {} ({})", arm1.banner_name, clock.year()));
                    log.add(clock.year(), clock.season(), clock.month(), clock.day(),
                            LogCategory::War,
                            std::format("{} routed {} in the field ({} spears returning)!",
                                        arm2.banner_name, arm1.banner_name, arm2.levied_spearmen));
                    to_destroy.push_back(a1);
                    arm1.levied_spearmen = 0;
                } else {
                    arm1.levied_spearmen = std::max(20, static_cast<int>(arm1.levied_spearmen * 0.72));
                    arm2.returning_home = true;
                    if (arm2.path_index + 1 < arm2.path.size()) {
                        arm2.path.resize(arm2.path_index + 1);
                    }
                    std::reverse(arm2.path.begin(), arm2.path.end());
                    arm2.path_index = 0;
                    log.add(clock.year(), clock.season(), clock.month(), clock.day(),
                            LogCategory::War,
                            std::format("{} clashed with {} and withdrew toward home.",
                                        arm2.banner_name, arm1.banner_name));
                }
                break;
            }
        }
    }

    // 3. Resolve arrivals at destination or return home
    for (const entt::entity ae : armies) {
        if (!registry.valid(ae)) continue;
        if (std::find(to_destroy.begin(), to_destroy.end(), ae) != to_destroy.end()) continue;

        auto& army = registry.get<ecs::ArmyComponent>(ae);
        const GridPos pos = registry.get<ecs::GridPosComponent>(ae).pos;
        const bool at_end = army.path.empty() || army.path_index + 1 >= army.path.size();
        if (!at_end) {
            continue;
        }

        if (army.returning_home) {
            demobilizeArmy(registry, army);
            log.add(clock.year(), clock.season(), clock.month(), clock.day(),
                    LogCategory::War,
                    std::format("{} demobilized ({} farmers returned to the plow).",
                                army.banner_name, army.levied_spearmen));
            to_destroy.push_back(ae);
            continue;
        }

        if (army.is_foreign_raider) {
            // Assault target settlement garrison
            const entt::entity target = army.target_settlement;
            if (target != entt::null && registry.valid(target) &&
                registry.all_of<ecs::SettlementComponent, ecs::ResourceStockpileComponent>(target)) {
                auto& t_sett = registry.get<ecs::SettlementComponent>(target);
                auto& t_stock = registry.get<ecs::ResourceStockpileComponent>(target);
                const auto& t_name = registry.get<ecs::NameComponent>(target).value;

                SkillRank def_rank = SkillRank::Beginner;
                if (t_sett.ruler_entity != entt::null && registry.valid(t_sett.ruler_entity)) {
                    def_rank = registry.get<ecs::MartialMagicComponent>(t_sett.ruler_entity).sword_rank;
                }
                const double fort_mult = ((t_sett.type == SettlementType::Fortress || t_sett.type == SettlementType::Capital) ? 1.45 : 1.05) +
                                         static_cast<double>(t_sett.armory_level) * 0.12 +
                                         static_cast<double>(t_sett.ward_tower_level) * 0.15;
                const double def_pow = (t_sett.levy_spearmen_ready * 0.75 + t_sett.knights_ready * 7.0 + t_sett.mages_ready * 15.0)
                                       * fort_mult * swordMultiplier(def_rank);
                const double atk_pow = (army.levied_spearmen * 1.0 + army.armored_knights * 6.5 + army.court_mages * 14.0)
                                       * swordMultiplier(army.captain_sword_rank);

                const bool raiders_won = (atk_pow * rng.nextRange(0.90, 1.12)) > def_pow;
                stampBattleSite(map, config, pos, MapSiteKind::BattleSite,
                                std::format("Siege of {} ({})", t_name, clock.year()));

                if (raiders_won) {
                    const double stolen = std::min(t_stock.get(ResourceType::Silver) * 0.35, 45.0);
                    t_stock.add(ResourceType::Silver, -stolen);
                    t_sett.prosperity = std::max(0.20, t_sett.prosperity - 0.08);
                    t_sett.bandit_threat = std::min(0.65, t_sett.bandit_threat + 0.12);

                    // 25% chance to loot a literary work in war
                    std::string looted_book;
                    if (rng.nextDouble() < 0.25) {
                        for (const entt::entity le : registry.view<ecs::LiteraryWorkComponent>()) {
                            auto& lw = registry.get<ecs::LiteraryWorkComponent>(le);
                            if (lw.holder_settlement == target) {
                                lw.holder_settlement = world_info.rival_duchy_entity;
                                lw.location = "Looted to Ostmark";
                                looted_book = lw.title;
                                break;
                            }
                        }
                    }

                    log.add(clock.year(), clock.season(), clock.month(), clock.day(),
                            LogCategory::War,
                            looted_book.empty()
                                ? std::format("{} raided {} ({:.0f} silver plundered)!", army.banner_name, t_name, stolen)
                                : std::format("{} raided {} and looted '{}'!", army.banner_name, t_name, looted_book));
                } else {
                    t_sett.prosperity = std::min(0.99, t_sett.prosperity + 0.03);
                    log.add(clock.year(), clock.season(), clock.month(), clock.day(),
                            LogCategory::War,
                            std::format("{} garrison repelled {} at the gates!", t_name, army.banner_name));
                }
            }
            to_destroy.push_back(ae);
        } else if (army.is_counter_raid) {
            army.loot_silver += rng.nextRange(35.0, 65.0);
            if (world_info.rival_duchy_entity != entt::null && registry.valid(world_info.rival_duchy_entity)) {
                auto& rival = registry.get<ecs::OffMapEmpireComponent>(world_info.rival_duchy_entity);
                rival.truce_years_left = 3;
                rival.relations = std::min(100, rival.relations + 15);
                rival.stance = "Humbled by Counter-Raid";
            }
            stampBattleSite(map, config, pos, MapSiteKind::BattleSite,
                            std::format("Ostmark March Raid ({})", clock.year()));
            log.add(clock.year(), clock.season(), clock.month(), clock.day(),
                    LogCategory::Diplomacy,
                    std::format("{} struck across Westmarch Pass, humbling Ostmark and seizing {:.0f} silver!",
                                army.banner_name, army.loot_silver));
            army.returning_home = true;
            std::reverse(army.path.begin(), army.path.end());
            army.path_index = 0;
        } else {
            // Nordmark banner cleared bandit/monster threat and starts return march
            if (army.target_settlement != entt::null && registry.valid(army.target_settlement) &&
                registry.all_of<ecs::SettlementComponent>(army.target_settlement)) {
                auto& t_sett = registry.get<ecs::SettlementComponent>(army.target_settlement);
                t_sett.bandit_threat = std::max(0.02, t_sett.bandit_threat - 0.18);
                if (registry.all_of<ecs::DemographicsComponent>(army.target_settlement)) {
                    for (auto& c : registry.get<ecs::DemographicsComponent>(army.target_settlement).cohorts) {
                        c.unrest = std::max(0.01, c.unrest - 0.06);
                    }
                }
                const auto& t_name = registry.get<ecs::NameComponent>(army.target_settlement).value;
                army.loot_silver += rng.nextRange(8.0, 22.0);
                log.add(clock.year(), clock.season(), clock.month(), clock.day(),
                        LogCategory::War,
                        std::format("{} purged brigands & frost beasts around {}.", army.banner_name, t_name));
            }
            army.returning_home = true;
            std::reverse(army.path.begin(), army.path.end());
            army.path_index = 0;
        }
    }

    for (const entt::entity ae : to_destroy) {
        if (registry.valid(ae)) {
            registry.destroy(ae);
        }
    }
}

void GovernanceWarSystem::tickSeasonal(entt::registry& registry,
                                       const world::WorldMap& map,
                                       const data::ConfigRegistry& config,
                                       const world::GeneratedWorldInfo& world_info,
                                       const core::SimClock& clock,
                                       core::Rng& rng,
                                       HistoryLog& log) {
    std::vector<entt::entity> settlements;
    for (const entt::entity se : registry.view<ecs::SettlementComponent>()) {
        settlements.push_back(se);
    }

    // Track whether a settlement already has an active banner in the field
    auto hasActiveBanner = [&](entt::entity s_ent) -> bool {
        for (const entt::entity ae : registry.view<ecs::ArmyComponent>()) {
            if (registry.get<ecs::ArmyComponent>(ae).origin_settlement == s_ent) {
                return true;
            }
        }
        return false;
    };

    for (const entt::entity s_ent : settlements) {
        auto& sett = registry.get<ecs::SettlementComponent>(s_ent);
        const auto& s_name = registry.get<ecs::NameComponent>(s_ent).value;
        const auto& pos = registry.get<ecs::GridPosComponent>(s_ent).pos;
        auto& stock = registry.get<ecs::ResourceStockpileComponent>(s_ent);
        auto& demo = registry.get<ecs::DemographicsComponent>(s_ent);

        // Mana-dense wilderness slowly breeds magical beasts & banditry; watchtowers patrol approaches
        const double mana = map.at(pos).mana_density;
        bool has_watchtower = false;
        for (int dy = -5; dy <= 5 && !has_watchtower; ++dy) {
            for (int dx = -5; dx <= 5; ++dx) {
                const GridPos wp{pos.x + dx, pos.y + dy};
                if (map.inBounds(wp) && map.at(wp).site_kind == MapSiteKind::Watchtower) {
                    has_watchtower = true;
                    break;
                }
            }
        }
        const double threat_gain = (mana > 0.70 ? 0.024 : 0.012) -
                                   (has_watchtower ? 0.006 : 0.0) -
                                   static_cast<double>(sett.ward_tower_level) * 0.004;
        sett.bandit_threat = std::clamp(sett.bandit_threat + std::max(0.002, threat_gain), 0.01, 0.85);

        if (sett.ruler_entity == entt::null || !registry.valid(sett.ruler_entity)) {
            continue;
        }
        const auto& r_name = registry.get<ecs::NameComponent>(sett.ruler_entity).value;
        const auto& r_ch = registry.get<ecs::CharacterComponent>(sett.ruler_entity);
        const auto& r_soc = registry.get<ecs::SocialStatusComponent>(sett.ruler_entity);

        const double fed = demo.weightedFedRatio();
        const double unrest = demo.weightedUnrest();
        const double grain = stock.get(ResourceType::Grain);
        const double silver = stock.get(ResourceType::Silver);

        // Ruler Utility-AI scoring
        const double score_ration =
            ((fed < 0.62 || (grain < 110.0 && clock.season() == Season::Winter)) && (clock.year() % 3 == 0))
                ? 0.82
                : 0.15;
        const double score_feast = (clock.season() == Season::Winter && unrest > 0.12 && grain > 260.0 && silver > 35.0) ? 0.76 : 0.10;
        const double score_sweep = (sett.bandit_threat > 0.20 && !hasActiveBanner(s_ent) &&
                                    !registry.get<ecs::FeudalContractComponent>(s_ent).exempt_from_levy) ? 0.78 : 0.12;
        const double score_embezzle = ((r_ch.trait == "Scheming" || r_ch.trait == "Ambitious") &&
                                       silver < 65.0 && rng.nextDouble() < 0.25) ? 0.62 : 0.05;

        const double best = std::max({score_ration, score_feast, score_sweep, score_embezzle});
        if (best < 0.55) {
            continue;
        }

        if (best == score_sweep) {
            const std::string b_name = std::format("{} Retinue", s_name);
            if (mobilizeBanner(registry, map, config, s_ent, s_ent, b_name, 20) != entt::null) {
                log.add(clock.year(), clock.season(), clock.month(), clock.day(),
                        LogCategory::War,
                        std::format("{} raised {} to clear bandits & beasts.", r_name, b_name));
            }
        } else if (best == score_feast) {
            stock.add(ResourceType::Grain, -32.0);
            stock.add(ResourceType::Silver, -12.0);
            sett.prosperity = std::min(0.99, sett.prosperity + 0.04);
            for (auto& c : demo.cohorts) {
                c.unrest = std::max(0.01, c.unrest - 0.10);
            }
            log.add(clock.year(), clock.season(), clock.month(), clock.day(),
                    LogCategory::Feudal,
                    std::format("{} hosted a Winter Feast at {} to calm unrest.", r_name, s_name));
        } else if (best == score_ration && !config.edictTemplates().empty()) {
            const auto& tmpl = config.edictTemplates()[static_cast<std::size_t>(clock.year()) %
                                                      std::min<std::size_t>(2, config.edictTemplates().size())];
            std::string edict_name = replaceToken(tmpl.name_pattern, "{house}", r_soc.house);
            edict_name = replaceToken(edict_name, "{ruler}", r_name);
            edict_name = replaceToken(edict_name, "{scholar}", r_name);
            edict_name = replaceToken(edict_name, "{year}", std::to_string(clock.year()));
            if (sett.active_law != edict_name) {
                const entt::entity le = registry.create();
                registry.emplace<ecs::NameComponent>(le, edict_name);
                registry.emplace<ecs::ActiveLawComponent>(le, ecs::ActiveLawComponent{
                    edict_name,
                    r_soc.title + " " + r_name,
                    clock.year(),
                    s_name,
                    tmpl.summary,
                    "edict"
                });
                sett.active_law = edict_name;
                stock.add(ResourceType::Grain, 15.0);
                log.add(clock.year(), clock.season(), clock.month(), clock.day(),
                        LogCategory::Eponym,
                        std::format("{} enacted '{}' at {}.", r_name, edict_name, s_name));
                capLaws(registry);
            }
        } else if (best == score_embezzle) {
            stock.add(ResourceType::Silver, 18.0);
            if (world_info.empire_entity != entt::null && registry.valid(world_info.empire_entity)) {
                auto& emp = registry.get<ecs::OffMapEmpireComponent>(world_info.empire_entity);
                emp.favor = std::max(0.05, emp.favor - 0.03);
            }
            log.add(clock.year(), clock.season(), clock.month(), clock.day(),
                    LogCategory::Feudal,
                    std::format("{} quietly skimmed 18 silver from customs at {}.", r_name, s_name));
        }
    }

    // Capital AI Spring Envoy to Southern Empire of Heliodor when silver is abundant and favor needs bolstering
    if (clock.season() == Season::Spring &&
        world_info.capital_entity != entt::null && registry.valid(world_info.capital_entity) &&
        world_info.south_pass_entity != entt::null && registry.valid(world_info.south_pass_entity) &&
        world_info.empire_entity != entt::null && registry.valid(world_info.empire_entity)) {
        const auto& cap_stock = registry.get<ecs::ResourceStockpileComponent>(world_info.capital_entity);
        const auto& emp = registry.get<ecs::OffMapEmpireComponent>(world_info.empire_entity);
        bool envoy_active = false;
        for (const entt::entity ce : registry.view<ecs::CaravanComponent>()) {
            if (registry.get<ecs::CaravanComponent>(ce).kind == CaravanCargoKind::ImperialEnvoy) {
                envoy_active = true;
                break;
            }
        }
        if (!envoy_active && cap_stock.get(ResourceType::Silver) > 95.0 && emp.favor < 0.68) {
            std::array<double, kResourceCount> gift{};
            gift[static_cast<std::size_t>(ResourceType::Silver)] = 15.0;
            if (EconomySystem::spawnCaravan(registry, map, config, world_info.capital_entity,
                                            world_info.south_pass_entity, CaravanCargoKind::ImperialEnvoy, gift)) {
                log.add(clock.year(), clock.season(), clock.month(), clock.day(),
                        LogCategory::Diplomacy,
                        "Vaelgard dispatched Spring diplomatic envoy (15 Ag) toward South Pass.");
            }
        }
    }

    // Foreign Ostmark raids in Summer/Autumn & Nordmark defensive muster
    if ((clock.season() == Season::Summer || clock.season() == Season::Autumn) &&
        registry.view<ecs::ArmyComponent>().size() < 3 &&
        world_info.rival_duchy_entity != entt::null && registry.valid(world_info.rival_duchy_entity)) {
        const auto& rival = registry.get<ecs::OffMapEmpireComponent>(world_info.rival_duchy_entity);
        if (rival.relations < -5 && rival.truce_years_left == 0 && rng.nextDouble() < 0.12) {
            std::vector<entt::entity> targets;
            for (const std::string_view tid : {"fort_vael", "kaldheim", "south_pass", "westwatch"}) {
                if (const auto it = world_info.settlement_by_id.find(std::string(tid));
                    it != world_info.settlement_by_id.end()) {
                    targets.push_back(it->second);
                }
            }
            if (!targets.empty()) {
                const entt::entity target = rng.pick(std::span<const entt::entity>(targets));
                const GridPos t_pos = registry.get<ecs::GridPosComponent>(target).pos;
                const auto& t_name = registry.get<ecs::NameComponent>(target).value;

                GridPos spawn_pos = world_info.west_pass_entry;
                if (!map.inBounds(spawn_pos) || !config.biomeByIndex(map.at(spawn_pos).biome_index).walkable) {
                    spawn_pos = GridPos{std::max(4, map.width() / 6), map.height() / 2};
                }
                auto path = world::findPath(map, config, spawn_pos, t_pos, false);
                if (path.size() >= 2) {
                    const entt::entity raider = registry.create();
                    const std::string r_title = "Ostmark Warband";
                    registry.emplace<ecs::NameComponent>(raider, r_title);
                    registry.emplace<ecs::GridPosComponent>(raider, spawn_pos);
                    registry.emplace<ecs::ArmyComponent>(raider, ecs::ArmyComponent{
                        r_title,
                        entt::null,
                        entt::null,
                        target,
                        true,
                        false,
                        rng.nextInt(140, 220),
                        rng.nextInt(12, 22),
                        rng.nextInt(1, 3),
                        SkillRank::Advanced,
                        false,
                        false,
                        0.0,
                        std::move(path),
                        0
                    });

                    log.add(clock.year(), clock.season(), clock.month(), clock.day(),
                            LogCategory::War,
                            std::format("Ostmark Warband crossed the march toward {}!", t_name));

                    // Mobilize capital banner to intercept
                    if (world_info.capital_entity != entt::null && !hasActiveBanner(world_info.capital_entity)) {
                        mobilizeBanner(registry, map, config, world_info.capital_entity, target,
                                       "Vaelgard Royal Banner", 35);
                    }
                }
            }
        }
    }
}

bool GovernanceWarSystem::issuePlayerEdict(entt::registry& registry,
                                           const data::ConfigRegistry& config,
                                           entt::entity seat_settlement,
                                           const core::SimClock& clock,
                                           HistoryLog& log) {
    if (seat_settlement == entt::null || !registry.valid(seat_settlement) ||
        !registry.all_of<ecs::SettlementComponent, ecs::NameComponent>(seat_settlement)) {
        return false;
    }

    auto& sett = registry.get<ecs::SettlementComponent>(seat_settlement);
    const auto& s_name = registry.get<ecs::NameComponent>(seat_settlement).value;
    std::string r_name = "Royal Regent";
    std::string r_house = "Vaelen";
    std::string r_title = "Lord";
    if (sett.ruler_entity != entt::null && registry.valid(sett.ruler_entity)) {
        r_name = registry.get<ecs::NameComponent>(sett.ruler_entity).value;
        const auto& soc = registry.get<ecs::SocialStatusComponent>(sett.ruler_entity);
        r_house = soc.house;
        r_title = soc.title;
    }

    std::string edict_name = std::format("The {} Edict of {}", r_house, clock.year());
    std::string summary = std::format("Decreed in {} to stabilize harvests and parish order.", s_name);
    if (!config.edictTemplates().empty()) {
        const auto& tmpl = config.edictTemplates()[static_cast<std::size_t>(clock.totalDays()) % config.edictTemplates().size()];
        edict_name = replaceToken(tmpl.name_pattern, "{house}", r_house);
        edict_name = replaceToken(edict_name, "{ruler}", r_name);
        edict_name = replaceToken(edict_name, "{scholar}", r_name);
        edict_name = replaceToken(edict_name, "{year}", std::to_string(clock.year()));
        summary = tmpl.summary;
    }

    const entt::entity le = registry.create();
    registry.emplace<ecs::NameComponent>(le, edict_name);
    registry.emplace<ecs::ActiveLawComponent>(le, ecs::ActiveLawComponent{
        edict_name,
        r_title + " " + r_name,
        clock.year(),
        s_name,
        summary,
        "edict"
    });
    sett.active_law = edict_name;
    sett.prosperity = std::min(0.99, sett.prosperity + 0.03);
    if (registry.all_of<ecs::DemographicsComponent>(seat_settlement)) {
        for (auto& c : registry.get<ecs::DemographicsComponent>(seat_settlement).cohorts) {
            c.unrest = std::max(0.01, c.unrest - 0.08);
        }
    }
    capLaws(registry);

    log.add(clock.year(), clock.season(), clock.month(), clock.day(),
            LogCategory::Eponym,
            std::format("Decree issued at {}: '{}'.", s_name, edict_name));
    return true;
}

bool GovernanceWarSystem::dispatchPlayerRetinue(entt::registry& registry,
                                                const world::WorldMap& map,
                                                const data::ConfigRegistry& config,
                                                const world::GeneratedWorldInfo& world_info,
                                                entt::entity seat_settlement,
                                                const core::SimClock& clock,
                                                HistoryLog& log) {
    if (seat_settlement == entt::null || !registry.valid(seat_settlement)) {
        return false;
    }

    entt::entity origin = seat_settlement;
    if (registry.all_of<ecs::FeudalContractComponent>(origin) &&
        registry.get<ecs::FeudalContractComponent>(origin).exempt_from_levy) {
        origin = world_info.capital_entity;
    }
    if (origin == entt::null || !registry.valid(origin)) {
        return false;
    }

    // Target any active Ostmark raider's destination, otherwise patrol the selected seat
    entt::entity target = seat_settlement;
    for (const entt::entity ae : registry.view<ecs::ArmyComponent>()) {
        const auto& a = registry.get<ecs::ArmyComponent>(ae);
        if (a.is_foreign_raider && a.target_settlement != entt::null) {
            target = a.target_settlement;
            break;
        }
    }

    const auto& orig_name = registry.get<ecs::NameComponent>(origin).value;
    const auto& targ_name = registry.get<ecs::NameComponent>(target).value;
    const std::string banner = std::format("{} Levy Banner", orig_name);

    const entt::entity army_ent = mobilizeBanner(registry, map, config, origin, target, banner, 28);
    if (army_ent == entt::null) {
        return false;
    }
    const auto& army = registry.get<ecs::ArmyComponent>(army_ent);
    log.add(clock.year(), clock.season(), clock.month(), clock.day(),
            LogCategory::War,
            std::format("Raised {} ({} Spears, {} Kt, {} Mg) toward {}.",
                        banner, army.levied_spearmen, army.armored_knights, army.court_mages, targ_name));
    return true;
}

bool GovernanceWarSystem::conductDiplomacy(entt::registry& registry,
                                           const world::WorldMap& map,
                                           const data::ConfigRegistry& config,
                                           const world::GeneratedWorldInfo& world_info,
                                           entt::entity seat_settlement,
                                           const core::SimClock& clock,
                                           core::Rng& rng,
                                           HistoryLog& log) {
    (void)rng;
    if (seat_settlement == entt::null || !registry.valid(seat_settlement) ||
        !registry.all_of<ecs::SettlementComponent, ecs::NameComponent, ecs::GridPosComponent>(seat_settlement)) {
        return false;
    }

    const auto& sett = registry.get<ecs::SettlementComponent>(seat_settlement);
    const auto& s_name = registry.get<ecs::NameComponent>(seat_settlement).value;
    const GridPos orig_pos = registry.get<ecs::GridPosComponent>(seat_settlement).pos;

    int ostmark_truce = 0;
    if (world_info.rival_duchy_entity != entt::null && registry.valid(world_info.rival_duchy_entity)) {
        ostmark_truce = registry.get<ecs::OffMapEmpireComponent>(world_info.rival_duchy_entity).truce_years_left;
    }

    const bool western_fort = (sett.id == "fort_vael" || sett.id == "westwatch" || sett.id == "kaldheim");
    const bool strong_frontier = (sett.knights_ready >= 12 &&
                                  seat_settlement != world_info.capital_entity &&
                                  seat_settlement != world_info.south_pass_entity);

    if ((western_fort || strong_frontier) && ostmark_truce == 0) {
        const std::string banner = std::format("{} March Raid", s_name);
        const entt::entity army_ent = mobilizeBanner(registry, map, config, seat_settlement, entt::null, banner, 25);
        if (army_ent != entt::null) {
            auto& army = registry.get<ecs::ArmyComponent>(army_ent);
            auto path = world::findPath(map, config, orig_pos, world_info.west_pass_entry, false);
            if (path.size() >= 2) {
                army.path = std::move(path);
                army.path_index = 0;
            }
            army.is_counter_raid = true;
            log.add(clock.year(), clock.season(), clock.month(), clock.day(),
                    LogCategory::Diplomacy,
                    std::format("{} launched punitive counter-raid toward Westmarch Pass!", s_name));
            return true;
        }
    }

    if (world_info.capital_entity == entt::null || !registry.valid(world_info.capital_entity) ||
        world_info.south_pass_entity == entt::null || !registry.valid(world_info.south_pass_entity)) {
        return false;
    }

    const auto& cap_stock = registry.get<ecs::ResourceStockpileComponent>(world_info.capital_entity);
    const double gift_silver = std::min(15.0, std::max(0.0, cap_stock.get(ResourceType::Silver) - 10.0));
    std::array<double, kResourceCount> cargo{};
    if (gift_silver > 0.0) {
        cargo[static_cast<std::size_t>(ResourceType::Silver)] = gift_silver;
    }

    if (!EconomySystem::spawnCaravan(registry, map, config, world_info.capital_entity,
                                     world_info.south_pass_entity, CaravanCargoKind::ImperialEnvoy, cargo)) {
        return false;
    }

    if (world_info.empire_entity != entt::null && registry.valid(world_info.empire_entity)) {
        auto& emp = registry.get<ecs::OffMapEmpireComponent>(world_info.empire_entity);
        ++emp.envoys_exchanged;
        emp.favor = std::min(1.0, emp.favor + 0.05);
        emp.relations = std::min(100, emp.relations + 5);
    }

    log.add(clock.year(), clock.season(), clock.month(), clock.day(),
            LogCategory::Diplomacy,
            "Dispatched royal envoy from Vaelgard to Heliodor via South Pass.");
    return true;
}

} // namespace sovereignia::sim

