#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace sovereignia {

enum class Season : std::uint8_t {
    Spring = 0,
    Summer = 1,
    Autumn = 2,
    Winter = 3
};

enum class ResourceType : std::uint8_t {
    Grain = 0,
    Fish = 1,
    Firewood = 2,
    Timber = 3,
    Iron = 4,
    Silver = 5,
    MagicCrystals = 6,
    Count = 7
};

inline constexpr std::size_t kResourceCount = static_cast<std::size_t>(ResourceType::Count);

enum class CohortType : std::uint8_t {
    Farmers = 0,
    Fishermen = 1,
    Miners = 2,
    Lumberjacks = 3,
    Artisans = 4,
    Count = 5
};

inline constexpr std::size_t kCohortCount = static_cast<std::size_t>(CohortType::Count);

enum class SettlementType : std::uint8_t {
    Capital = 0,
    Fortress = 1,
    Bishopric = 2,
    MiningCamp = 3,
    LumberCamp = 4,
    FishingPort = 5,
    FarmingVillage = 6
};

enum class RoadLevel : std::uint8_t {
    None = 0,
    DirtRoad = 1,
    ImperialHighway = 2
};

enum class SkillRank : std::uint8_t {
    None = 0,
    Beginner = 1,
    Intermediate = 2,
    Advanced = 3,
    Saint = 4,
    King = 5,
    Emperor = 6
};

enum class RiteType : std::uint8_t {
    NorthernAutocephalous = 0,
    ImperialOrthodox = 1,
    HesychastMystic = 2
};

enum class LogCategory : std::uint8_t {
    Economy = 0,
    Feudal = 1,
    Theology = 2,
    Diplomacy = 3,
    Season = 4,
    Eponym = 5,
    War = 6,
    Dynasty = 7,
    Ecology = 8
};

enum class CaravanCargoKind : std::uint8_t {
    Corvee = 0,
    Tithe = 1,
    GrainRelief = 2,
    TimberWagon = 3,
    ImperialTribute = 4,
    MarketTrade = 5,
    ImperialEnvoy = 6
};

enum class BuildingType : std::uint8_t {
    None = 0,
    StoneGranary = 1,
    Scriptorium = 2,
    KnightlyArmory = 3,
    AetherWardTower = 4,
    HighwayPaving = 5
};

enum class DynastyRole : std::uint8_t {
    HeadOfHouse = 0,
    LegitimateSpouse = 1,
    Concubine = 2,
    LegitimateChild = 3,
    BastardChild = 4,
    RetainerTutor = 5,
    ClergyMember = 6
};

enum class MapSiteKind : std::uint8_t {
    None = 0,
    AncientRuin = 1,
    BattleSite = 2,
    Hermitage = 3,
    Watchtower = 4,
    ManaWell = 5,
    SacredGrove = 6
};

enum class ForestryPolicy : std::uint8_t {
    BalancedCoppice = 0,
    IntensiveFelling = 1,
    WardenReforestation = 2
};

struct GridPos {
    int x = 0;
    int y = 0;

    bool operator==(const GridPos& other) const noexcept {
        return x == other.x && y == other.y;
    }
    bool operator!=(const GridPos& other) const noexcept {
        return !(*this == other);
    }
};

constexpr std::string_view toString(Season s) noexcept {
    switch (s) {
        case Season::Spring: return "Spring";
        case Season::Summer: return "Summer";
        case Season::Autumn: return "Autumn";
        case Season::Winter: return "Winter";
    }
    return "Spring";
}

constexpr std::string_view monthName(int month) noexcept {
    switch (month) {
        case 1: return "Frostmoon";
        case 2: return "Rimewane";
        case 3: return "Thawmoot";
        case 4: return "Seedwake";
        case 5: return "Bloomtide";
        case 6: return "Suncrest";
        case 7: return "Haymath";
        case 8: return "Goldsheaf";
        case 9: return "Harvestmere";
        case 10: return "Fellwind";
        case 11: return "Darkfen";
        case 12: return "Yulefast";
        default: return "Frostmoon";
    }
}

constexpr std::string_view toString(ResourceType r) noexcept {
    switch (r) {
        case ResourceType::Grain: return "Grain";
        case ResourceType::Fish: return "Fish";
        case ResourceType::Firewood: return "Firewood";
        case ResourceType::Timber: return "Timber";
        case ResourceType::Iron: return "Iron";
        case ResourceType::Silver: return "Silver";
        case ResourceType::MagicCrystals: return "Crystals";
        case ResourceType::Count: return "Unknown";
    }
    return "Unknown";
}

constexpr std::string_view toString(CohortType c) noexcept {
    switch (c) {
        case CohortType::Farmers: return "Farmers";
        case CohortType::Fishermen: return "Fishermen";
        case CohortType::Miners: return "Miners";
        case CohortType::Lumberjacks: return "Lumberjacks";
        case CohortType::Artisans: return "Artisans";
        case CohortType::Count: return "Unknown";
    }
    return "Unknown";
}

constexpr std::string_view toString(SettlementType st) noexcept {
    switch (st) {
        case SettlementType::Capital: return "Capital";
        case SettlementType::Fortress: return "Fortress";
        case SettlementType::Bishopric: return "See/Bishopric";
        case SettlementType::MiningCamp: return "Mining Camp";
        case SettlementType::LumberCamp: return "Lumber Camp";
        case SettlementType::FishingPort: return "Fishing Port";
        case SettlementType::FarmingVillage: return "Farming Village";
    }
    return "Settlement";
}

constexpr char settlementGlyph(SettlementType st) noexcept {
    switch (st) {
        case SettlementType::Capital: return '@';
        case SettlementType::Fortress: return '#';
        case SettlementType::Bishopric: return '+';
        case SettlementType::MiningCamp: return 'M';
        case SettlementType::LumberCamp: return 'T';
        case SettlementType::FishingPort: return '&';
        case SettlementType::FarmingVillage: return 'o';
    }
    return 'o';
}

constexpr std::string_view toString(RoadLevel rl) noexcept {
    switch (rl) {
        case RoadLevel::None: return "Wilderness";
        case RoadLevel::DirtRoad: return "Dirt Road";
        case RoadLevel::ImperialHighway: return "Imperial Highway";
    }
    return "Wilderness";
}

constexpr std::string_view toString(SkillRank sr) noexcept {
    switch (sr) {
        case SkillRank::None: return "None";
        case SkillRank::Beginner: return "Beginner";
        case SkillRank::Intermediate: return "Intermediate";
        case SkillRank::Advanced: return "Advanced";
        case SkillRank::Saint: return "Saint";
        case SkillRank::King: return "King";
        case SkillRank::Emperor: return "Emperor";
    }
    return "None";
}

constexpr std::string_view toString(RiteType rt) noexcept {
    switch (rt) {
        case RiteType::NorthernAutocephalous: return "Northern Autocephalous Rite";
        case RiteType::ImperialOrthodox: return "Imperial Orthodox Rite";
        case RiteType::HesychastMystic: return "Hesychast Mystic Tradition";
    }
    return "Northern Autocephalous Rite";
}

constexpr std::string_view toShortString(RiteType rt) noexcept {
    switch (rt) {
        case RiteType::NorthernAutocephalous: return "Northern Rite";
        case RiteType::ImperialOrthodox: return "Imperial Rite";
        case RiteType::HesychastMystic: return "Hesychast";
    }
    return "Northern Rite";
}

constexpr std::string_view toString(LogCategory cat) noexcept {
    switch (cat) {
        case LogCategory::Economy: return "ECON";
        case LogCategory::Feudal: return "FEUDAL";
        case LogCategory::Theology: return "THEOL";
        case LogCategory::Diplomacy: return "DIPLO";
        case LogCategory::Season: return "SEASON";
        case LogCategory::Eponym: return "EPONYM";
        case LogCategory::War: return "WAR";
        case LogCategory::Dynasty: return "DYNASTY";
        case LogCategory::Ecology: return "ECOL";
    }
    return "LOG";
}

constexpr std::string_view toString(CaravanCargoKind kind) noexcept {
    switch (kind) {
        case CaravanCargoKind::Corvee: return "Corvee Train";
        case CaravanCargoKind::Tithe: return "Church Tithe";
        case CaravanCargoKind::GrainRelief: return "Grain Relief";
        case CaravanCargoKind::TimberWagon: return "Firewood Sled";
        case CaravanCargoKind::ImperialTribute: return "Imperial Tribute";
        case CaravanCargoKind::MarketTrade: return "Market Trade";
        case CaravanCargoKind::ImperialEnvoy: return "Diplomatic Envoy";
    }
    return "Caravan";
}

constexpr std::string_view toString(BuildingType b) noexcept {
    switch (b) {
        case BuildingType::None: return "None";
        case BuildingType::StoneGranary: return "Stone Granary";
        case BuildingType::Scriptorium: return "Scriptorium";
        case BuildingType::KnightlyArmory: return "Knightly Armory";
        case BuildingType::AetherWardTower: return "Aether Ward Tower";
        case BuildingType::HighwayPaving: return "Highway Paving";
    }
    return "None";
}

constexpr std::string_view toShortString(BuildingType b) noexcept {
    switch (b) {
        case BuildingType::None: return "Idle";
        case BuildingType::StoneGranary: return "Granary";
        case BuildingType::Scriptorium: return "Script.";
        case BuildingType::KnightlyArmory: return "Armory";
        case BuildingType::AetherWardTower: return "Ward";
        case BuildingType::HighwayPaving: return "Road";
    }
    return "Idle";
}

constexpr std::string_view toString(DynastyRole role) noexcept {
    switch (role) {
        case DynastyRole::HeadOfHouse: return "Head of House";
        case DynastyRole::LegitimateSpouse: return "Legitimate Spouse";
        case DynastyRole::Concubine: return "Concubine";
        case DynastyRole::LegitimateChild: return "Legitimate Child";
        case DynastyRole::BastardChild: return "Bastard Child";
        case DynastyRole::RetainerTutor: return "Household Tutor";
        case DynastyRole::ClergyMember: return "Household Clergy";
    }
    return "Household";
}

constexpr std::string_view toString(MapSiteKind kind) noexcept {
    switch (kind) {
        case MapSiteKind::None: return "None";
        case MapSiteKind::AncientRuin: return "Ancient Ruin";
        case MapSiteKind::BattleSite: return "Historic Battle";
        case MapSiteKind::Hermitage: return "Monastic Hermitage";
        case MapSiteKind::Watchtower: return "Border Watchtower";
        case MapSiteKind::ManaWell: return "Ley-Line Mana Well";
        case MapSiteKind::SacredGrove: return "Sacred Old-Growth Grove";
    }
    return "Site";
}

constexpr char siteGlyph(MapSiteKind kind) noexcept {
    switch (kind) {
        case MapSiteKind::None: return '.';
        case MapSiteKind::AncientRuin: return '%';
        case MapSiteKind::BattleSite: return 'x';
        case MapSiteKind::Hermitage: return 'h';
        case MapSiteKind::Watchtower: return 'I';
        case MapSiteKind::ManaWell: return '*';
        case MapSiteKind::SacredGrove: return 'Y';
    }
    return '.';
}

constexpr std::string_view toString(ForestryPolicy policy) noexcept {
    switch (policy) {
        case ForestryPolicy::BalancedCoppice: return "Balanced Coppice";
        case ForestryPolicy::IntensiveFelling: return "Intensive Felling";
        case ForestryPolicy::WardenReforestation: return "Warden Reforestation";
    }
    return "Balanced Coppice";
}

constexpr std::string_view toShortString(ForestryPolicy policy) noexcept {
    switch (policy) {
        case ForestryPolicy::BalancedCoppice: return "Coppice";
        case ForestryPolicy::IntensiveFelling: return "Clear-Cut";
        case ForestryPolicy::WardenReforestation: return "Reforest";
    }
    return "Coppice";
}

inline SettlementType parseSettlementType(std::string_view s) noexcept {
    if (s == "Capital") return SettlementType::Capital;
    if (s == "Fortress") return SettlementType::Fortress;
    if (s == "Bishopric" || s == "See") return SettlementType::Bishopric;
    if (s == "MiningCamp") return SettlementType::MiningCamp;
    if (s == "LumberCamp") return SettlementType::LumberCamp;
    if (s == "FishingPort") return SettlementType::FishingPort;
    return SettlementType::FarmingVillage;
}

inline SkillRank parseSkillRank(std::string_view s) noexcept {
    if (s == "Beginner") return SkillRank::Beginner;
    if (s == "Intermediate") return SkillRank::Intermediate;
    if (s == "Advanced") return SkillRank::Advanced;
    if (s == "Saint") return SkillRank::Saint;
    if (s == "King") return SkillRank::King;
    if (s == "Emperor") return SkillRank::Emperor;
    return SkillRank::None;
}

inline RiteType parseRiteType(std::string_view s) noexcept {
    if (s == "ImperialOrthodox") return RiteType::ImperialOrthodox;
    if (s == "HesychastMystic") return RiteType::HesychastMystic;
    return RiteType::NorthernAutocephalous;
}

} // namespace sovereignia

