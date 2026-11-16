#include "sovereignia/core/clock.hpp"
#include "sovereignia/core/paths.hpp"
#include "sovereignia/ecs/components.hpp"
#include "sovereignia/sim/economy_system.hpp"
#include "sovereignia/sim/engine.hpp"
#include "sovereignia/ui/terminal_ui.hpp"
#include "sovereignia/world/pathfinding.hpp"

#include <chrono>
#include <cmath>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

#define SOV_ASSERT(cond)                                                                         \
    do {                                                                                         \
        if (!(cond)) {                                                                           \
            std::ostringstream oss;                                                              \
            oss << "Assertion failed: " << #cond << " (" << __FILE__ << ":" << __LINE__ << ")"; \
            throw std::runtime_error(oss.str());                                                 \
        }                                                                                        \
    } while (false)

void testJsonLoadingAndWorldGeneration() {
    sovereignia::sim::SimulationEngine engine;
    engine.loadData(sovereignia::core::resolveDataDir());
    SOV_ASSERT(engine.config().biomes().size() == 15);
    SOV_ASSERT(engine.config().settlementSeeds().size() == 12);
    SOV_ASSERT(engine.config().maleNames().size() >= 70);
    SOV_ASSERT(engine.config().femaleNames().size() >= 70);
    SOV_ASSERT(engine.config().clergyNames().size() >= 25);
    SOV_ASSERT(engine.config().houses().size() >= 15);
    SOV_ASSERT(engine.config().epithets().size() >= 25);
    SOV_ASSERT(!engine.config().swordStyles().empty());
    SOV_ASSERT(!engine.config().initialLaws().empty());
    SOV_ASSERT(!engine.config().initialTexts().empty());

    engine.initializeWorld(482ULL, 480);
    SOV_ASSERT(engine.map().width() == 108);
    SOV_ASSERT(engine.map().height() == 48);

    const auto settlements = engine.orderedSettlements();
    SOV_ASSERT(settlements.size() == 12);
    SOV_ASSERT(engine.map().cachedRouteCount() >= 132);

    const entt::entity capital = engine.worldInfo().capital_entity;
    const entt::entity south_pass = engine.worldInfo().south_pass_entity;
    SOV_ASSERT(capital != entt::null);
    SOV_ASSERT(south_pass != entt::null);

    const auto& reg = engine.registry();
    const auto cap_pos = reg.get<sovereignia::ecs::GridPosComponent>(capital).pos;
    const auto sp_pos = reg.get<sovereignia::ecs::GridPosComponent>(south_pass).pos;

    for (const entt::entity s_ent : settlements) {
        const auto pos = reg.get<sovereignia::ecs::GridPosComponent>(s_ent).pos;
        const auto path_cap = sovereignia::world::findPath(engine.map(), engine.config(), pos, cap_pos, false);
        const auto path_sp = sovereignia::world::findPath(engine.map(), engine.config(), pos, sp_pos, false);
        const auto path_wp = sovereignia::world::findPath(engine.map(), engine.config(), pos, engine.worldInfo().west_pass_entry, false);
        SOV_ASSERT(!path_cap.empty());
        SOV_ASSERT(!path_sp.empty());
        SOV_ASSERT(!path_wp.empty());

        const auto& sett = reg.get<sovereignia::ecs::SettlementComponent>(s_ent);
        SOV_ASSERT(sett.ruler_entity != entt::null && reg.valid(sett.ruler_entity));
        SOV_ASSERT(reg.all_of<sovereignia::ecs::HouseholdComponent>(sett.ruler_entity));
        SOV_ASSERT(!reg.get<sovereignia::ecs::HouseholdComponent>(sett.ruler_entity).member_entities.empty());
    }

    const int lake_idx = engine.config().biomeIndex("inland_lake");
    int bridges = 0;
    int lakes = 0;
    int groves = 0;
    int watchtowers = 0;
    int mana_wells = 0;
    for (int y = 0; y < engine.map().height(); ++y) {
        for (int x = 0; x < engine.map().width(); ++x) {
            const auto& cell = engine.map().at(x, y);
            SOV_ASSERT(!cell.region_name.empty());
            if (cell.has_bridge) ++bridges;
            if (cell.biome_index == lake_idx) ++lakes;
            if (cell.site_kind == sovereignia::MapSiteKind::SacredGrove) ++groves;
            if (cell.site_kind == sovereignia::MapSiteKind::Watchtower) ++watchtowers;
            if (cell.site_kind == sovereignia::MapSiteKind::ManaWell) ++mana_wells;
        }
    }
    SOV_ASSERT(bridges > 0);
    SOV_ASSERT(lakes > 0);
    SOV_ASSERT(groves >= 2);
    SOV_ASSERT(watchtowers >= 3);
    SOV_ASSERT(mana_wells >= 3);

    std::cout << "[PASS] Phase 1-2: 108x48 realistic map, 15 biomes, 12 seats, lakes, bridges & landmarks\n";
}

void testClockSeasonalEconomyAndMarketTrade() {
    sovereignia::sim::SimulationEngine engine;
    engine.initializeWorld(482ULL, 480);

    int daily_ticks = 0;
    int monthly_ticks = 0;
    int seasonal_ticks = 0;
    int yearly_ticks = 0;

    for (int i = 0; i < sovereignia::core::SimClock::kDaysPerYear; ++i) {
        const auto flags = engine.tickDay();
        if (flags.daily) ++daily_ticks;
        if (flags.monthly) ++monthly_ticks;
        if (flags.seasonal) ++seasonal_ticks;
        if (flags.yearly) ++yearly_ticks;
    }

    SOV_ASSERT(daily_ticks == 360);
    SOV_ASSERT(monthly_ticks == 12);
    SOV_ASSERT(seasonal_ticks == 4);
    SOV_ASSERT(yearly_ticks == 1);
    SOV_ASSERT(engine.clock().year() == 481);

    // Advance 1 more year so MarketTrade keeps remote camps fed
    engine.advanceDays(360);
    const auto& reg = engine.registry();
    for (const entt::entity s_ent : engine.orderedSettlements()) {
        const auto& stock = reg.get<sovereignia::ecs::ResourceStockpileComponent>(s_ent);
        const auto& demo = reg.get<sovereignia::ecs::DemographicsComponent>(s_ent);
        for (std::size_t r = 0; r < sovereignia::kResourceCount; ++r) {
            const double val = stock.get(static_cast<sovereignia::ResourceType>(r));
            SOV_ASSERT(std::isfinite(val));
            SOV_ASSERT(val >= 0.0);
        }
        SOV_ASSERT(demo.weightedFedRatio() > 0.50);
    }

    sovereignia::sim::EconomySystem econ;
    sovereignia::sim::HistoryLog dummy_log;
    auto measureSeasonDay = [&](int target_month) {
        sovereignia::sim::SimulationEngine tmp;
        tmp.initializeWorld(777ULL, 480);
        tmp.advanceDays((target_month - 1) * 30);
        return econ.tickDaily(tmp.registry(), tmp.map(), tmp.config(), tmp.clock(), dummy_log);
    };

    const auto summer_totals = measureSeasonDay(4);
    const auto autumn_totals = measureSeasonDay(7);
    const auto winter_totals = measureSeasonDay(10);

    SOV_ASSERT(autumn_totals.grain_produced > winter_totals.grain_produced * 5.0);
    SOV_ASSERT(winter_totals.firewood_consumed > summer_totals.firewood_consumed * 3.0);

    std::cout << "[PASS] Phase 2: 4-frequency clock, seasonal yields & market trade balance\n";
}

void testForestEcologyDeforestationAndReforestation() {
    // 1. Slow deforestation under IntensiveFelling + heavy logging/charcoal extraction
    sovereignia::sim::SimulationEngine defor_eng;
    defor_eng.initializeWorld(482ULL, 480);

    const entt::entity frostholm = defor_eng.worldInfo().settlement_by_id.at("frostholm");
    const auto frost_pos = defor_eng.registry().get<sovereignia::ecs::GridPosComponent>(frostholm).pos;
    auto& f_sett = defor_eng.registry().get<sovereignia::ecs::SettlementComponent>(frostholm);
    f_sett.forestry_policy = sovereignia::ForestryPolicy::IntensiveFelling;
    if (f_sett.ruler_entity != entt::null && defor_eng.registry().valid(f_sett.ruler_entity)) {
        defor_eng.registry().get<sovereignia::ecs::CharacterComponent>(f_sett.ruler_entity).trait = "Scheming";
    }
    for (auto& cohort : defor_eng.registry().get<sovereignia::ecs::DemographicsComponent>(frostholm).cohorts) {
        if (cohort.type == sovereignia::CohortType::Lumberjacks) {
            cohort.population = 360;
        } else if (cohort.type == sovereignia::CohortType::Miners) {
            cohort.population = 140;
        }
    }

    sovereignia::GridPos grove_pos{-1, -1};
    for (int y = 0; y < defor_eng.map().height() && grove_pos.x < 0; ++y) {
        for (int x = 0; x < defor_eng.map().width(); ++x) {
            if (defor_eng.map().at(x, y).site_kind == sovereignia::MapSiteKind::SacredGrove) {
                grove_pos = {x, y};
                break;
            }
        }
    }
    SOV_ASSERT(grove_pos.x >= 0);

    const double cov0 = defor_eng.map().localForestCover(frost_pos, 5);
    defor_eng.advanceDays(360);
    const double cov_1yr = defor_eng.map().localForestCover(frost_pos, 5);
    SOV_ASSERT(cov_1yr > 0.0);
    SOV_ASSERT((cov0 - cov_1yr) < 0.05);

    defor_eng.advanceDays(19 * 360);
    const double cov_20yr = defor_eng.map().localForestCover(frost_pos, 5);
    SOV_ASSERT(cov_20yr <= cov0 - 0.04);
    SOV_ASSERT(defor_eng.map().at(grove_pos).forest_cover >= 0.90);

    // 2. Slow reforestation under WardenReforestation on the logged Frostholm hinterland with logging halted
    auto& f_sett_refor = defor_eng.registry().get<sovereignia::ecs::SettlementComponent>(frostholm);
    f_sett_refor.forestry_policy = sovereignia::ForestryPolicy::WardenReforestation;
    if (f_sett_refor.ruler_entity != entt::null && defor_eng.registry().valid(f_sett_refor.ruler_entity)) {
        defor_eng.registry().get<sovereignia::ecs::CharacterComponent>(f_sett_refor.ruler_entity).trait = "Diligent";
    }
    for (auto& cohort : defor_eng.registry().get<sovereignia::ecs::DemographicsComponent>(frostholm).cohorts) {
        if (cohort.type == sovereignia::CohortType::Lumberjacks || cohort.type == sovereignia::CohortType::Miners) {
            cohort.population = 0;
        }
    }

    defor_eng.advanceDays(30 * 360);
    const double cov_recovered = defor_eng.map().localForestCover(frost_pos, 5);
    SOV_ASSERT(cov_recovered > cov_20yr + 0.02);

    std::cout << "[PASS] Ecology: Slow 20yr deforestation (" << cov0 << "->" << cov_20yr
              << ") & 30yr reforestation (" << cov_20yr << "->" << cov_recovered << ")\n";
}

void testDynastySuccessionAndChildhoodMana() {
    sovereignia::sim::SimulationEngine engine;
    engine.initializeWorld(482ULL, 480);

    const entt::entity capital = engine.worldInfo().capital_entity;
    const entt::entity cap_ruler = engine.registry().get<sovereignia::ecs::SettlementComponent>(capital).ruler_entity;
    const auto& hh_init = engine.registry().get<sovereignia::ecs::HouseholdComponent>(cap_ruler).member_entities;
    entt::entity lukas = entt::null;
    for (const entt::entity me : hh_init) {
        if (engine.registry().get<sovereignia::ecs::NameComponent>(me).value.find("Lukas") != std::string::npos) {
            lukas = me;
            break;
        }
    }
    SOV_ASSERT(lukas != entt::null);
    const double lukas_mana_before = engine.registry().get<sovereignia::ecs::MartialMagicComponent>(lukas).mana_capacity;

    engine.advanceDays(360);
    SOV_ASSERT(engine.registry().valid(lukas));
    const double lukas_mana_after = engine.registry().get<sovereignia::ecs::MartialMagicComponent>(lukas).mana_capacity;
    SOV_ASSERT(lukas_mana_after > lukas_mana_before);

    engine.registry().get<sovereignia::ecs::CharacterComponent>(cap_ruler).age = 95;
    engine.advanceDays(360);

    const entt::entity new_ruler = engine.registry().get<sovereignia::ecs::SettlementComponent>(capital).ruler_entity;
    SOV_ASSERT(new_ruler != cap_ruler);
    SOV_ASSERT(engine.registry().valid(new_ruler));
    SOV_ASSERT(engine.registry().get<sovereignia::ecs::CharacterComponent>(new_ruler).alive);
    SOV_ASSERT(!engine.memorialArchive().empty());

    std::cout << "[PASS] Phase 3: Childhood mana window, mortality, memorial & succession\n";
}

void testExpandedNamesAndFamilyTreeLineage() {
    sovereignia::sim::SimulationEngine engine;
    engine.initializeWorld(482ULL, 480);

    const entt::entity capital = engine.worldInfo().capital_entity;
    const entt::entity aleks_ent = engine.registry().get<sovereignia::ecs::SettlementComponent>(capital).ruler_entity;
    const auto aleks_pid = engine.registry().get<sovereignia::ecs::FamilyComponent>(aleks_ent).person_id;
    SOV_ASSERT(aleks_pid != sovereignia::sim::kInvalidPersonId);

    sovereignia::sim::PersonId lukas_pid = sovereignia::sim::kInvalidPersonId;
    for (const entt::entity me : engine.registry().get<sovereignia::ecs::HouseholdComponent>(aleks_ent).member_entities) {
        if (engine.registry().get<sovereignia::ecs::NameComponent>(me).value.find("Lukas") != std::string::npos) {
            lukas_pid = engine.registry().get<sovereignia::ecs::FamilyComponent>(me).person_id;
            break;
        }
    }
    SOV_ASSERT(lukas_pid != sovereignia::sim::kInvalidPersonId);
    const auto* lukas_node = engine.familyTree().find(lukas_pid);
    SOV_ASSERT(lukas_node != nullptr);
    SOV_ASSERT(lukas_node->father_id == aleks_pid);
    SOV_ASSERT(lukas_node->mother_id != sovereignia::sim::kInvalidPersonId);
    SOV_ASSERT(lukas_node->patronymic == "Aleksandrovich");

    // Advance 120 years so multi-generational dynasties succeed, marry, birth, and die
    engine.advanceDays(120 * 360);
    SOV_ASSERT(engine.familyTree().totalPersons() > 60);

    const entt::entity cur_ruler = engine.registry().get<sovereignia::ecs::SettlementComponent>(capital).ruler_entity;
    const auto cur_pid = engine.registry().get<sovereignia::ecs::FamilyComponent>(cur_ruler).person_id;
    SOV_ASSERT(cur_pid != sovereignia::sim::kInvalidPersonId);
    const auto chain = engine.familyTree().ancestorChain(cur_pid, 10);
    SOV_ASSERT(chain.size() >= 2);
    for (const auto aid : chain) {
        SOV_ASSERT(engine.familyTree().find(aid) != nullptr);
    }

    // Ensure no two living siblings in any settlement household share an identical given name
    const auto& reg = engine.registry();
    for (const entt::entity s_ent : engine.orderedSettlements()) {
        const entt::entity re = reg.get<sovereignia::ecs::SettlementComponent>(s_ent).ruler_entity;
        if (re == entt::null || !reg.all_of<sovereignia::ecs::HouseholdComponent>(re)) continue;
        std::unordered_set<std::string> child_names;
        for (const entt::entity me : reg.get<sovereignia::ecs::HouseholdComponent>(re).member_entities) {
            if (!reg.valid(me) || !reg.all_of<sovereignia::ecs::CharacterComponent, sovereignia::ecs::FamilyComponent>(me)) continue;
            if (!reg.get<sovereignia::ecs::CharacterComponent>(me).alive) continue;
            const auto& mf = reg.get<sovereignia::ecs::FamilyComponent>(me);
            if (mf.role == sovereignia::DynastyRole::LegitimateChild || mf.role == sovereignia::DynastyRole::BastardChild) {
                SOV_ASSERT(!mf.given_name.empty());
                SOV_ASSERT(child_names.insert(mf.given_name).second);
            }
        }
    }

    // Verify birthdays span varied months & days across recorded kin
    std::unordered_set<int> months_seen;
    std::unordered_set<int> days_seen;
    for (const auto& node : engine.familyTree().nodes()) {
        SOV_ASSERT(node.birth_month >= 1 && node.birth_month <= 12);
        SOV_ASSERT(node.birth_day >= 1 && node.birth_day <= 30);
        months_seen.insert(node.birth_month);
        days_seen.insert(node.birth_day);
    }
    SOV_ASSERT(months_seen.size() >= 8);
    SOV_ASSERT(days_seen.size() >= 12);

    // Verify a character ages strictly on their own (birth_month, birth_day), not on M1 D1 New Year's
    sovereignia::sim::SimulationEngine eng_bday;
    eng_bday.initializeWorld(482ULL, 480);
    entt::entity target = entt::null;
    int bm = 0;
    int bd = 0;
    int age0 = 0;
    for (const entt::entity ce : eng_bday.registry().view<sovereignia::ecs::CharacterComponent>()) {
        const auto& ch = eng_bday.registry().get<sovereignia::ecs::CharacterComponent>(ce);
        if (ch.alive && ch.age < 40 && ch.birth_month >= 2) {
            target = ce;
            bm = ch.birth_month;
            bd = ch.birth_day;
            age0 = ch.age;
            break;
        }
    }
    SOV_ASSERT(target != entt::null);

    eng_bday.advanceDays(359); // Year 480 M12 D30 (one birthday passed in 480)
    SOV_ASSERT(eng_bday.registry().get<sovereignia::ecs::CharacterComponent>(target).age == age0 + 1);

    eng_bday.tickDay(); // Year 481 M1 D1 (New Year's Day yearly tick)
    SOV_ASSERT(eng_bday.clock().month() == 1 && eng_bday.clock().day() == 1);
    SOV_ASSERT(eng_bday.registry().get<sovereignia::ecs::CharacterComponent>(target).age == age0 + 1);

    bool aged_on_birthday = false;
    for (int d = 0; d < 360; ++d) {
        eng_bday.tickDay();
        const int cur_age = eng_bday.registry().get<sovereignia::ecs::CharacterComponent>(target).age;
        if (eng_bday.clock().month() == bm && eng_bday.clock().day() == bd) {
            SOV_ASSERT(cur_age == age0 + 2);
            aged_on_birthday = true;
            break;
        }
        SOV_ASSERT(cur_age == age0 + 1);
    }
    SOV_ASSERT(aged_on_birthday);

    std::cout << "[PASS] Onomastics & Family Tree: " << engine.familyTree().totalPersons()
              << " recorded kin (" << months_seen.size() << " birth months, " << days_seen.size()
              << " birth days), Vaelgard lineage depth=" << (chain.size() + 1) << ", distinct sibling names verified\n";
}

void testLevyMobilizationCombatAndDemobilization() {
    sovereignia::sim::SimulationEngine engine;
    engine.initializeWorld(482ULL, 480);

    const entt::entity oakhaven = engine.worldInfo().settlement_by_id.at("oakhaven");
    const entt::entity vaelgard = engine.worldInfo().capital_entity;

    const int farmers_before = engine.registry().get<sovereignia::ecs::DemographicsComponent>(oakhaven)
                                   .countOf(sovereignia::CohortType::Farmers);
    const entt::entity army_ent = engine.governanceWar().mobilizeBanner(
        engine.registry(), engine.map(), engine.config(), oakhaven, vaelgard, "Oakhaven Levy", 30);
    SOV_ASSERT(army_ent != entt::null);
    const int levied = engine.registry().get<sovereignia::ecs::ArmyComponent>(army_ent).levied_spearmen;
    SOV_ASSERT(levied > 0);

    const int farmers_during = engine.registry().get<sovereignia::ecs::DemographicsComponent>(oakhaven)
                                   .countOf(sovereignia::CohortType::Farmers);
    SOV_ASSERT(farmers_during == farmers_before - levied);

    engine.advanceDays(65);
    SOV_ASSERT(!engine.registry().valid(army_ent));

    const int farmers_after = engine.registry().get<sovereignia::ecs::DemographicsComponent>(oakhaven)
                                  .countOf(sovereignia::CohortType::Farmers);
    SOV_ASSERT(farmers_after >= farmers_before);

    SOV_ASSERT(engine.issueEdictAt(vaelgard));
    SOV_ASSERT(engine.dispatchRetinueAt(vaelgard));
    engine.convenePlayerSynod(true);
    SOV_ASSERT(engine.churchState().synods_held >= 2);

    std::cout << "[PASS] Phase 4: Farmer levy pull, marching armies, combat & demobilization\n";
}

void testPreHistory200YearsAndDeterminism() {
    const auto t0 = std::chrono::steady_clock::now();
    sovereignia::sim::SimulationEngine engine;
    engine.initializeWorld(482ULL, 282);
    engine.runPreHistory(200);
    const auto t1 = std::chrono::steady_clock::now();
    const double secs = std::chrono::duration<double>(t1 - t0).count();

    SOV_ASSERT(engine.clock().year() == 482);
    const int pop = engine.totalKingdomPopulation();
    SOV_ASSERT(pop >= 2500 && pop <= 15000);
    SOV_ASSERT(!engine.memorialArchive().empty());
    SOV_ASSERT(engine.churchState().synods_held > 10);
    SOV_ASSERT(engine.ecologyState().tiles_cleared_total + engine.ecologyState().tiles_regrown_total > 0);
    SOV_ASSERT(engine.ecologyState().average_canopy >= 0.15 && engine.ecologyState().average_canopy <= 0.90);

    const auto& reg = engine.registry();
    for (const entt::entity s_ent : engine.orderedSettlements()) {
        const auto& sett = reg.get<sovereignia::ecs::SettlementComponent>(s_ent);
        SOV_ASSERT(sett.ruler_entity != entt::null && reg.valid(sett.ruler_entity));
        SOV_ASSERT(reg.get<sovereignia::ecs::CharacterComponent>(sett.ruler_entity).alive);

        const auto& stock = reg.get<sovereignia::ecs::ResourceStockpileComponent>(s_ent);
        for (std::size_t r = 0; r < sovereignia::kResourceCount; ++r) {
            const double val = stock.get(static_cast<sovereignia::ResourceType>(r));
            SOV_ASSERT(std::isfinite(val));
            SOV_ASSERT(val >= 0.0);
        }
    }

    sovereignia::sim::SimulationEngine eng_a;
    sovereignia::sim::SimulationEngine eng_b;
    eng_a.initializeWorld(999ULL, 430);
    eng_b.initializeWorld(999ULL, 430);
    eng_a.runPreHistory(50);
    eng_b.runPreHistory(50);

    SOV_ASSERT(eng_a.clock().year() == 480);
    SOV_ASSERT(eng_a.totalKingdomPopulation() == eng_b.totalKingdomPopulation());
    SOV_ASSERT(eng_a.memorialArchive().size() == eng_b.memorialArchive().size());
    SOV_ASSERT(eng_a.churchState().synods_held == eng_b.churchState().synods_held);
    SOV_ASSERT(eng_a.familyTree().totalPersons() == eng_b.familyTree().totalPersons());
    SOV_ASSERT(eng_a.ecologyState().forested_tiles == eng_b.ecologyState().forested_tiles);
    SOV_ASSERT(std::abs(eng_a.ecologyState().average_canopy - eng_b.ecologyState().average_canopy) < 1e-9);

    const auto& stock_a = eng_a.registry().get<sovereignia::ecs::ResourceStockpileComponent>(eng_a.worldInfo().capital_entity);
    const auto& stock_b = eng_b.registry().get<sovereignia::ecs::ResourceStockpileComponent>(eng_b.worldInfo().capital_entity);
    SOV_ASSERT(std::abs(stock_a.get(sovereignia::ResourceType::Grain) -
                        stock_b.get(sovereignia::ResourceType::Grain)) < 1e-9);

    std::cout << "[PASS] Phase 5: 200-year pre-history (" << secs << "s, Pop=" << pop
              << ", Canopy=" << static_cast<int>(engine.ecologyState().average_canopy * 100.0) << "%"
              << ", Memorials=" << engine.memorialArchive().size()
              << ", Synods=" << engine.churchState().synods_held << ") & determinism\n";
}

void testFtxuiSnapshotsAndTabs() {
    sovereignia::sim::SimulationEngine engine;
    engine.initializeWorld(482ULL, 432);
    engine.runPreHistory(50);

    auto state = sovereignia::ui::makeInitialUIState(engine);
    const std::string snap_map = sovereignia::ui::renderSnapshotToString(engine, state, 122, 38);
    SOV_ASSERT(snap_map.find("[M]AP") != std::string::npos);
    SOV_ASSERT(snap_map.find("108x48") != std::string::npos);
    SOV_ASSERT(snap_map.find("Lens: Terrain") != std::string::npos);
    SOV_ASSERT(snap_map.find("[I]NSPECT") != std::string::npos);
    SOV_ASSERT(snap_map.find("[H]ISTORY") != std::string::npos);
    SOV_ASSERT(snap_map.find("[L]OCAL LEDGER") != std::string::npos);
    SOV_ASSERT(snap_map.find("Vaelgard") != std::string::npos);

    sovereignia::ui::cycleMapLens(state);
    SOV_ASSERT(state.map_lens == sovereignia::ui::MapLens::Forestry);
    const std::string snap_for = sovereignia::ui::renderSnapshotToString(engine, state, 122, 38);
    SOV_ASSERT(snap_for.find("Lens: Forestry") != std::string::npos);

    state.map_lens = sovereignia::ui::MapLens::Terrain;
    state.active_tab = sovereignia::ui::ActiveTab::Nobility;
    const std::string snap_nob = sovereignia::ui::renderSnapshotToString(engine, state, 122, 38);
    SOV_ASSERT(snap_nob.find("[N]OBILITY") != std::string::npos);

    state.active_tab = sovereignia::ui::ActiveTab::FamilyTree;
    const std::string snap_tree = sovereignia::ui::renderSnapshotToString(engine, state, 122, 38);
    SOV_ASSERT(snap_tree.find("[T] FAMILY TREE") != std::string::npos);
    SOV_ASSERT(snap_tree.find("Generation") != std::string::npos);
    SOV_ASSERT(snap_tree.find("CHILDREN") != std::string::npos);

    state.active_tab = sovereignia::ui::ActiveTab::Religion;
    const std::string snap_rel = sovereignia::ui::renderSnapshotToString(engine, state, 122, 38);
    SOV_ASSERT(snap_rel.find("[R]ELIGION") != std::string::npos);
    SOV_ASSERT(snap_rel.find("AUTOCEPHALY") != std::string::npos);

    state.active_tab = sovereignia::ui::ActiveTab::Chronicle;
    const std::string snap_chr = sovereignia::ui::renderSnapshotToString(engine, state, 122, 38);
    SOV_ASSERT(snap_chr.find("[C]HRONICLE") != std::string::npos);
    SOV_ASSERT(snap_chr.find("WOODLAND ECOLOGY") != std::string::npos);
    SOV_ASSERT(snap_chr.find("RUINS") != std::string::npos);

    state.show_help = true;
    const std::string snap_help = sovereignia::ui::renderSnapshotToString(engine, state, 122, 38);
    SOV_ASSERT(snap_help.find("[?] HELP") != std::string::npos);
    SOV_ASSERT(snap_help.find("NAVIGATION") != std::string::npos);
    state.show_help = false;

    sovereignia::ui::cycleLogFilter(state);
    SOV_ASSERT(state.log_category_filter == 0);
    const std::string snap_filt = sovereignia::ui::renderSnapshotToString(engine, state, 122, 38);
    SOV_ASSERT(snap_filt.find("Filter: ECON") != std::string::npos);

    const entt::entity init_sel = state.selected_settlement;
    sovereignia::ui::selectNextSettlement(engine, state);
    SOV_ASSERT(state.selected_settlement != init_sel);
    sovereignia::ui::selectPrevSettlement(engine, state);
    SOV_ASSERT(state.selected_settlement == init_sel);

    SOV_ASSERT(engine.cycleForestryPolicyAt(init_sel) == sovereignia::ForestryPolicy::IntensiveFelling);
    SOV_ASSERT(engine.cycleForestryPolicyAt(init_sel) == sovereignia::ForestryPolicy::WardenReforestation);

    std::cout << "[PASS] FTXUI 4-panel snapshots, lenses, help overlay, log filters & QoL controls verified\n";
}

void testConstructionDiplomacyCourtAndTheology() {
    // 1. Construction, Highway Paving & Armsmithing Sinks
    sovereignia::sim::SimulationEngine eng_bld;
    eng_bld.initializeWorld(482ULL, 480);
    const entt::entity vaelgard = eng_bld.worldInfo().capital_entity;
    const entt::entity oakhaven = eng_bld.worldInfo().settlement_by_id.at("oakhaven");

    auto& oak_sett = eng_bld.registry().get<sovereignia::ecs::SettlementComponent>(oakhaven);
    auto& oak_stock = eng_bld.registry().get<sovereignia::ecs::ResourceStockpileComponent>(oakhaven);
    const int vael_lvl0 = eng_bld.registry().get<sovereignia::ecs::SettlementComponent>(vaelgard).granary_level +
                          eng_bld.registry().get<sovereignia::ecs::SettlementComponent>(vaelgard).scriptorium_level +
                          eng_bld.registry().get<sovereignia::ecs::SettlementComponent>(vaelgard).armory_level +
                          eng_bld.registry().get<sovereignia::ecs::SettlementComponent>(vaelgard).ward_tower_level;
    SOV_ASSERT(eng_bld.commissionBuildingAt(vaelgard));
    SOV_ASSERT(eng_bld.registry().get<sovereignia::ecs::SettlementComponent>(vaelgard).active_construction !=
               sovereignia::BuildingType::None);

    // Prepare oakhaven to pave HighwayPaving next
    oak_sett.granary_level = 2;
    oak_stock.add(sovereignia::ResourceType::Timber, 80.0);
    oak_stock.add(sovereignia::ResourceType::Iron, 60.0);
    oak_stock.add(sovereignia::ResourceType::Silver, 60.0);
    int hw_before = 0;
    for (int y = 0; y < eng_bld.map().height(); ++y) {
        for (int x = 0; x < eng_bld.map().width(); ++x) {
            if (eng_bld.map().at(x, y).road_level == sovereignia::RoadLevel::ImperialHighway) {
                ++hw_before;
            }
        }
    }
    SOV_ASSERT(eng_bld.commissionBuildingAt(oakhaven));
    SOV_ASSERT(oak_sett.active_construction == sovereignia::BuildingType::HighwayPaving);

    // Set an armory & iron surplus on fort_vael to verify armsmithing sink
    const entt::entity fort_vael = eng_bld.worldInfo().settlement_by_id.at("fort_vael");
    eng_bld.registry().get<sovereignia::ecs::SettlementComponent>(fort_vael).armory_level = 1;
    auto& fv_stock = eng_bld.registry().get<sovereignia::ecs::ResourceStockpileComponent>(fort_vael);
    fv_stock.add(sovereignia::ResourceType::Iron, 120.0);
    fv_stock.add(sovereignia::ResourceType::Silver, 80.0);

    eng_bld.advanceDays(95); // 3+ months: Oakhaven HighwayPaving completes
    SOV_ASSERT(oak_sett.active_construction == sovereignia::BuildingType::None);

    int hw_after = 0;
    for (int y = 0; y < eng_bld.map().height(); ++y) {
        for (int x = 0; x < eng_bld.map().width(); ++x) {
            if (eng_bld.map().at(x, y).road_level == sovereignia::RoadLevel::ImperialHighway) {
                ++hw_after;
            }
        }
    }
    SOV_ASSERT(hw_after > hw_before);

    eng_bld.advanceDays(30); // 4+ months total: Vaelgard 4-month building & Fort Vael armsmithing
    const auto& vael_sett_after = eng_bld.registry().get<sovereignia::ecs::SettlementComponent>(vaelgard);
    const int vael_lvl1 = vael_sett_after.granary_level + vael_sett_after.scriptorium_level +
                          vael_sett_after.armory_level + vael_sett_after.ward_tower_level;
    SOV_ASSERT(vael_lvl1 > vael_lvl0);
    SOV_ASSERT(eng_bld.registry().get<sovereignia::ecs::SettlementComponent>(fort_vael).forged_arms_stock >= 3);

    // 2. Court Offices, Underage Regency & Cross-House Marriages
    sovereignia::sim::SimulationEngine eng_court;
    eng_court.initializeWorld(482ULL, 480);
    const entt::entity cap_c = eng_court.worldInfo().capital_entity;
    const entt::entity st_cyril = eng_court.worldInfo().monastery_entity;
    SOV_ASSERT(eng_court.registry().get<sovereignia::ecs::SettlementComponent>(cap_c).sword_instructor_entity != entt::null);
    SOV_ASSERT(eng_court.registry().get<sovereignia::ecs::SettlementComponent>(st_cyril).court_mage_entity != entt::null);

    // Force succession while Lukas is still underage (<16)
    const entt::entity old_cap_ruler = eng_court.registry().get<sovereignia::ecs::SettlementComponent>(cap_c).ruler_entity;
    eng_court.registry().get<sovereignia::ecs::CharacterComponent>(old_cap_ruler).age = 95;
    eng_court.advanceDays(360);
    const auto& cap_sett_reg = eng_court.registry().get<sovereignia::ecs::SettlementComponent>(cap_c);
    SOV_ASSERT(cap_sett_reg.ruler_entity != old_cap_ruler);
    SOV_ASSERT(eng_court.registry().get<sovereignia::ecs::CharacterComponent>(cap_sett_reg.ruler_entity).age < 16);
    SOV_ASSERT(cap_sett_reg.regent_entity != entt::null);
    SOV_ASSERT(eng_court.registry().valid(cap_sett_reg.regent_entity));

    // Verify cross-house marriage between existing settlement households over multi-year run
    sovereignia::sim::SimulationEngine eng_mar;
    eng_mar.initializeWorld(482ULL, 480);
    const entt::entity oak_m = eng_mar.worldInfo().settlement_by_id.at("oakhaven");
    const entt::entity roth_m = eng_mar.worldInfo().settlement_by_id.at("rotherfield");
    const entt::entity oak_ruler = eng_mar.registry().get<sovereignia::ecs::SettlementComponent>(oak_m).ruler_entity;
    const entt::entity roth_ruler = eng_mar.registry().get<sovereignia::ecs::SettlementComponent>(roth_m).ruler_entity;
    // Make oakhaven ruler an unmarried young male and rotherfield have an adult unmarried daughter
    eng_mar.registry().get<sovereignia::ecs::CharacterComponent>(oak_ruler).is_female = false;
    eng_mar.registry().get<sovereignia::ecs::CharacterComponent>(oak_ruler).age = 24;
    for (const entt::entity me : eng_mar.registry().get<sovereignia::ecs::HouseholdComponent>(oak_ruler).member_entities) {
        if (eng_mar.registry().valid(me) &&
            eng_mar.registry().get<sovereignia::ecs::FamilyComponent>(me).role == sovereignia::DynastyRole::LegitimateSpouse) {
            eng_mar.registry().get<sovereignia::ecs::CharacterComponent>(me).alive = false;
        }
    }
    for (const entt::entity me : eng_mar.registry().get<sovereignia::ecs::HouseholdComponent>(roth_ruler).member_entities) {
        if (eng_mar.registry().valid(me) &&
            eng_mar.registry().get<sovereignia::ecs::FamilyComponent>(me).role == sovereignia::DynastyRole::LegitimateChild) {
            auto& ch = eng_mar.registry().get<sovereignia::ecs::CharacterComponent>(me);
            ch.is_female = true;
            ch.age = 20;
        }
    }
    eng_mar.advanceDays(6 * 360);
    bool saw_cross_marriage = false;
    for (const auto& entry : eng_mar.historyLog().entries()) {
        if (entry.message.find("Cross-house marriage") != std::string::npos) {
            saw_cross_marriage = true;
            break;
        }
    }
    SOV_ASSERT(saw_cross_marriage);

    // 3. Off-Map Diplomacy (Imperial Envoy & Ostmark Counter-Raid)
    sovereignia::sim::SimulationEngine eng_dip;
    eng_dip.initializeWorld(482ULL, 480);
    const entt::entity emp_ent = eng_dip.worldInfo().empire_entity;
    const entt::entity rival_ent = eng_dip.worldInfo().rival_duchy_entity;
    const double favor_before = eng_dip.registry().get<sovereignia::ecs::OffMapEmpireComponent>(emp_ent).favor;
    SOV_ASSERT(eng_dip.conductDiplomacyAt(eng_dip.worldInfo().capital_entity));
    SOV_ASSERT(eng_dip.registry().get<sovereignia::ecs::OffMapEmpireComponent>(emp_ent).envoys_exchanged >= 1);

    const entt::entity fv_dip = eng_dip.worldInfo().settlement_by_id.at("fort_vael");
    SOV_ASSERT(eng_dip.conductDiplomacyAt(fv_dip));
    bool has_counter_raid = false;
    for (const entt::entity ae : eng_dip.registry().view<sovereignia::ecs::ArmyComponent>()) {
        if (eng_dip.registry().get<sovereignia::ecs::ArmyComponent>(ae).is_counter_raid) {
            has_counter_raid = true;
            break;
        }
    }
    SOV_ASSERT(has_counter_raid);

    eng_dip.advanceDays(70);
    SOV_ASSERT(eng_dip.registry().get<sovereignia::ecs::OffMapEmpireComponent>(emp_ent).favor > favor_before);
    SOV_ASSERT(eng_dip.registry().get<sovereignia::ecs::OffMapEmpireComponent>(rival_ent).truce_years_left > 0);

    // 4. Grassroots Heresy, Saints & Text Circulation
    sovereignia::sim::SimulationEngine eng_theol;
    eng_theol.initializeWorld(482ULL, 480);
    const entt::entity sp_ent = eng_theol.worldInfo().settlement_by_id.at("south_pass");
    for (auto& c : eng_theol.registry().get<sovereignia::ecs::DemographicsComponent>(sp_ent).cohorts) {
        c.orthodoxy = 0.65;
    }
    eng_theol.advanceDays(360);
    SOV_ASSERT(!eng_theol.registry().get<sovereignia::ecs::SettlementComponent>(sp_ent).active_heresy.empty());
    SOV_ASSERT(eng_theol.churchState().active_heresies_count >= 1);

    // Scriptorium + Pastoral reconciliation & Synod canonization
    eng_theol.registry().get<sovereignia::ecs::SettlementComponent>(sp_ent).scriptorium_level = 1;
    eng_theol.convenePlayerSynod(true);
    SOV_ASSERT(eng_theol.churchState().canonized_saints >= 1);

    eng_theol.advanceDays(4 * 360);
    SOV_ASSERT(eng_theol.registry().get<sovereignia::ecs::SettlementComponent>(sp_ent).active_heresy.empty());
    SOV_ASSERT(eng_theol.churchState().texts_circulated >= 1);

    // Verify UI renders new Religion & Ledger sections cleanly
    auto ui_state = sovereignia::ui::makeInitialUIState(eng_theol);
    ui_state.active_tab = sovereignia::ui::ActiveTab::Religion;
    const std::string rel_snap = sovereignia::ui::renderSnapshotToString(eng_theol, ui_state, 122, 38);
    SOV_ASSERT(rel_snap.find("SAINTS:") != std::string::npos);
    SOV_ASSERT(rel_snap.find("Bld:") != std::string::npos);

    std::cout << "[PASS] Construction, Court Politics/Regency, Off-Map Diplomacy, Heresy & Saints verified\n";
}

} // namespace

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    try {
        testJsonLoadingAndWorldGeneration();
        testClockSeasonalEconomyAndMarketTrade();
        testForestEcologyDeforestationAndReforestation();
        testDynastySuccessionAndChildhoodMana();
        testExpandedNamesAndFamilyTreeLineage();
        testLevyMobilizationCombatAndDemobilization();
        testPreHistory200YearsAndDeterminism();
        testFtxuiSnapshotsAndTabs();
        testConstructionDiplomacyCourtAndTheology();
        std::cout << "ALL SOVEREIGNIA TESTS PASSED\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "TEST FAILURE: " << ex.what() << "\n";
        return 1;
    }
}
