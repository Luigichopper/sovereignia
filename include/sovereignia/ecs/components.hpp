#pragma once

#include "sovereignia/core/types.hpp"

#include <entt/entt.hpp>

#include <array>
#include <numeric>
#include <string>
#include <vector>

namespace sovereignia::ecs {

struct NameComponent {
    std::string value;
};

struct GridPosComponent {
    GridPos pos;
};

struct SettlementComponent {
    std::string id;
    SettlementType type = SettlementType::FarmingVillage;
    entt::entity ruler_entity = entt::null;
    entt::entity liege_settlement = entt::null;
    entt::entity ecclesiastic_see = entt::null;
    double prosperity = 0.72;
    double bandit_threat = 0.08;
    int levy_spearmen_ready = 0;
    int knights_ready = 0;
    int mages_ready = 0;
    ForestryPolicy forestry_policy = ForestryPolicy::BalancedCoppice;
    double local_woodland_cover = 0.50;
    std::string dominant_text;
    std::string active_law;
    int granary_level = 0;       // 0..2 (raises spoilage cap & lowers spoilage rate)
    int scriptorium_level = 0;   // 0..2 (boosts orthodoxy & manuscript copying)
    int armory_level = 0;        // 0..2 (forges arms from Iron, boosts knights & defense)
    int ward_tower_level = 0;    // 0..2 (uses MagicCrystals, boosts mages & lowers threat)
    BuildingType active_construction = BuildingType::None;
    int construction_months_left = 0;
    int forged_arms_stock = 0;
    entt::entity regent_entity = entt::null;
    entt::entity sword_instructor_entity = entt::null;
    entt::entity court_mage_entity = entt::null;
    entt::entity court_chaplain_entity = entt::null;
    std::string active_heresy;
    std::string patron_saint;
    bool is_pilgrimage_site = false;
};

struct ResourceStockpileComponent {
    std::array<double, kResourceCount> current{};
    std::array<double, kResourceCount> month_start_snapshot{};
    std::array<double, kResourceCount> net_monthly_delta{};

    double get(ResourceType r) const noexcept {
        return current[static_cast<std::size_t>(r)];
    }

    void add(ResourceType r, double amount) noexcept {
        auto& slot = current[static_cast<std::size_t>(r)];
        slot += amount;
        if (slot < 0.0) {
            slot = 0.0;
        }
    }

    double delta(ResourceType r) const noexcept {
        return net_monthly_delta[static_cast<std::size_t>(r)];
    }

    void snapshotMonthStart() noexcept {
        month_start_snapshot = current;
    }

    void updateMonthlyDelta() noexcept {
        for (std::size_t i = 0; i < kResourceCount; ++i) {
            net_monthly_delta[i] = current[i] - month_start_snapshot[i];
        }
        month_start_snapshot = current;
    }
};

struct PopCohort {
    CohortType type = CohortType::Farmers;
    int population = 0;
    double fed_ratio = 1.0;       // [0.0, 1.0]
    double warmth_ratio = 1.0;    // [0.0, 1.0]
    double orthodoxy = 0.88;      // [0.0, 1.0]
    double unrest = 0.05;         // [0.0, 1.0]
};

struct DemographicsComponent {
    std::vector<PopCohort> cohorts;

    int totalPopulation() const noexcept {
        int sum = 0;
        for (const auto& c : cohorts) {
            sum += c.population;
        }
        return sum;
    }

    int countOf(CohortType t) const noexcept {
        for (const auto& c : cohorts) {
            if (c.type == t) {
                return c.population;
            }
        }
        return 0;
    }

    double weightedFedRatio() const noexcept {
        int total = totalPopulation();
        if (total <= 0) return 1.0;
        double acc = 0.0;
        for (const auto& c : cohorts) {
            acc += c.fed_ratio * static_cast<double>(c.population);
        }
        return acc / static_cast<double>(total);
    }

    double weightedOrthodoxy() const noexcept {
        int total = totalPopulation();
        if (total <= 0) return 1.0;
        double acc = 0.0;
        for (const auto& c : cohorts) {
            acc += c.orthodoxy * static_cast<double>(c.population);
        }
        return acc / static_cast<double>(total);
    }

    double weightedUnrest() const noexcept {
        int total = totalPopulation();
        if (total <= 0) return 0.0;
        double acc = 0.0;
        for (const auto& c : cohorts) {
            acc += c.unrest * static_cast<double>(c.population);
        }
        return acc / static_cast<double>(total);
    }
};

struct FeudalContractComponent {
    double corvee_rate = 0.15; // Secular tax/labor tribute owed to capital
    double tithe_rate = 0.10;  // Ecclesiastical tithe owed to Bishopric/Monastery
    double tax_arrears = 0.0;
    bool exempt_from_levy = false;
};

struct CharacterComponent {
    int age = 35;
    bool is_female = false;
    bool alive = true;
    std::string trait = "Pragmatic";
    entt::entity seat_settlement = entt::null;
    int birth_month = 1; // 1..12
    int birth_day = 1;   // 1..30
};

struct SocialStatusComponent {
    std::string title = "Lord";
    std::string house = "Vaelen";
    bool is_clergy = false;
    int prestige = 50;
};

struct MartialMagicComponent {
    std::string sword_style = "Water-Guard";
    SkillRank sword_rank = SkillRank::Beginner;
    std::string magic_school = "None";
    SkillRank magic_rank = SkillRank::None;
    double mana_capacity = 20.0;
    bool incantationless = false;
};

struct FaithComponent {
    RiteType rite = RiteType::NorthernAutocephalous;
    double zeal = 0.75;
    double imperial_loyalty = 0.35;
};

struct FamilyComponent {
    entt::entity head_entity = entt::null;
    entt::entity mother_entity = entt::null;
    entt::entity father_entity = entt::null;
    DynastyRole role = DynastyRole::HeadOfHouse;
    bool is_bastard = false;
    double ambition = 0.35;
    int faction_backing = 25; // 0..100
    int pregnancy_months = 0; // >0 when expecting
    int pregnancy_due_day = 15; // 1..30
    std::string note;
    std::uint32_t person_id = 0;
    int regnal_number = 0;
    std::string given_name;
    std::string patronymic;
    std::string epithet;
};

struct HouseholdComponent {
    std::vector<entt::entity> member_entities;
};

struct ArmyComponent {
    std::string banner_name;
    entt::entity commander = entt::null;
    entt::entity origin_settlement = entt::null;
    entt::entity target_settlement = entt::null;
    bool is_foreign_raider = false;
    bool returning_home = false;
    int levied_spearmen = 0;
    int armored_knights = 0;
    int court_mages = 0;
    SkillRank captain_sword_rank = SkillRank::Beginner;
    bool has_healer_mage = false;
    bool has_incantationless_mage = false;
    double loot_silver = 0.0;
    std::vector<GridPos> path;
    std::size_t path_index = 0;
    bool is_counter_raid = false;
};

struct CaravanComponent {
    entt::entity origin = entt::null;
    entt::entity destination = entt::null;
    CaravanCargoKind kind = CaravanCargoKind::Corvee;
    std::array<double, kResourceCount> cargo{};
    std::vector<GridPos> path;
    std::size_t path_index = 0;
};

struct OffMapEmpireComponent {
    std::string title;
    std::string ruler_name;
    double favor = 0.55;          // [0.0, 1.0]
    double tribute_owed_silver = 140.0;
    double tribute_paid_this_year = 0.0;
    int relations = 25;           // [-100, 100]
    std::string stance = "Watchful Suzerain";
    bool has_trade_treaty = false;
    bool has_imperial_charter = false;
    int truce_years_left = 0;
    int envoys_exchanged = 0;
};

struct ActiveLawComponent {
    std::string name;
    std::string author;
    int enacted_year = 479;
    std::string seat_name;
    std::string summary;
    std::string kind = "edict";
};

struct LiteraryWorkComponent {
    std::string title;
    std::string author;
    std::string kind = "Treatise";
    int year = 450;
    std::string location;
    std::string doctrine;
    entt::entity holder_settlement = entt::null;
    std::string trait_imprint = "Pragmatic";
    bool is_controversial = false;
    bool endorsed = true;
};

struct ChurchState {
    double autocephaly = 0.68;      // [0.0, 1.0]
    double schism_tension = 0.28;   // [0.0, 1.0]
    int synods_held = 1;
    bool in_formal_schism = false;
    std::string active_controversy = "On the Veneration of Northern Icons";
    int canonized_saints = 0;
    int active_heresies_count = 0;
    int texts_circulated = 0;
};

struct MemorialEntry {
    std::string name;
    std::string title;
    std::string house;
    int death_year = 480;
    int age_at_death = 65;
    std::string epitaph;
};

struct EcologyState {
    int initial_forested_tiles = 0;
    int forested_tiles = 0;
    int old_growth_tiles = 0;
    int tiles_cleared_total = 0;
    int tiles_regrown_total = 0;
    double average_canopy = 0.0;
};

} // namespace sovereignia::ecs

