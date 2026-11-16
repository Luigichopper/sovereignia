#pragma once

#include "sovereignia/core/types.hpp"

#include <array>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace sovereignia::data {

struct RgbColor {
    int r = 255;
    int g = 255;
    int b = 255;
};

struct BiomeDef {
    std::string id;
    std::string name;
    char glyph = '.';
    RgbColor fg;
    RgbColor bg;
    double fertility = 1.0;
    double timber_yield = 0.2;
    double ore_yield = 0.1;
    double fish_yield = 0.1;
    double base_mana = 0.4;
    double move_cost = 1.0;
    bool walkable = true;
    bool forest_capable = false;
    bool is_water = false;
};

struct HouseholdMemberSeed {
    std::string name;
    std::string role;
    int age = 25;
    std::string note;
};

struct RulerSeed {
    std::string name;
    std::string house;
    std::string title;
    int age = 35;
    bool female = false;
    std::string trait;
    std::string sword_style = "Water-Guard";
    SkillRank sword_rank = SkillRank::Beginner;
    std::string magic_school = "None";
    SkillRank magic_rank = SkillRank::None;
    double mana_capacity = 20.0;
    bool incantationless = false;
    RiteType rite = RiteType::NorthernAutocephalous;
    double zeal = 0.7;
    std::vector<HouseholdMemberSeed> household;
};

struct SettlementSeed {
    std::string id;
    std::string name;
    SettlementType type = SettlementType::FarmingVillage;
    std::string region_hint;
    std::array<int, kCohortCount> cohorts{};
    std::array<double, kResourceCount> stockpiles{};
    double corvee_rate = 0.15;
    double tithe_rate = 0.10;
    std::string dominant_text;
    std::string active_law;
    RulerSeed ruler;
};

struct SwordStyleDef {
    std::string name;
    std::string philosophy;
    std::string description;
};

struct MagicSchoolDef {
    std::string name;
    int base_incantation_lines = 4;
    std::string description;
};

struct RiteDef {
    RiteType type = RiteType::NorthernAutocephalous;
    std::string id;
    std::string name;
    std::string description;
};

struct InitialTextDef {
    std::string title;
    std::string author;
    std::string kind;
    int year = 400;
    std::string location;
    std::string doctrine;
};

struct InitialLawDef {
    std::string name;
    std::string author;
    int year = 470;
    std::string seat;
    std::string summary;
};

struct EdictTemplateDef {
    std::string kind;
    std::string name_pattern;
    std::string summary;
};

/// Holds all lore, biome, skill, doctrine, law, and settlement definitions loaded from JSON.
class ConfigRegistry {
public:
    void loadFromDirectory(const std::filesystem::path& data_dir);

    const std::vector<BiomeDef>& biomes() const noexcept { return biomes_; }
    int biomeIndex(std::string_view id) const;
    const BiomeDef& biomeByIndex(int index) const;

    const std::string& kingdomName() const noexcept { return kingdom_name_; }
    const std::vector<SettlementSeed>& settlementSeeds() const noexcept { return settlements_; }

    const std::vector<std::string>& houses() const noexcept { return houses_; }
    const std::vector<std::string>& nobleTitles() const noexcept { return noble_titles_; }
    const std::vector<std::string>& maleNames() const noexcept { return male_names_; }
    const std::vector<std::string>& femaleNames() const noexcept { return female_names_; }
    const std::vector<std::string>& clergyNames() const noexcept { return clergy_names_; }
    const std::vector<std::string>& epithets() const noexcept { return epithets_; }
    const std::vector<std::string>& bastardCognomens() const noexcept { return bastard_cognomens_; }
    const std::vector<std::string>& cadetSuffixes() const noexcept { return cadet_suffixes_; }
    const std::vector<std::string>& traits() const noexcept { return traits_; }

    const std::vector<SwordStyleDef>& swordStyles() const noexcept { return sword_styles_; }
    const std::vector<MagicSchoolDef>& magicSchools() const noexcept { return magic_schools_; }

    const std::vector<RiteDef>& rites() const noexcept { return rites_; }
    const std::vector<InitialTextDef>& initialTexts() const noexcept { return initial_texts_; }
    const std::vector<std::string>& treatiseTemplates() const noexcept { return treatise_templates_; }

    const std::vector<InitialLawDef>& initialLaws() const noexcept { return initial_laws_; }
    const std::vector<EdictTemplateDef>& edictTemplates() const noexcept { return edict_templates_; }

private:
    std::vector<BiomeDef> biomes_;
    std::unordered_map<std::string, int> biome_index_map_;

    std::string kingdom_name_ = "Kingdom of Nordmark";
    std::vector<SettlementSeed> settlements_;

    std::vector<std::string> houses_;
    std::vector<std::string> noble_titles_;
    std::vector<std::string> male_names_;
    std::vector<std::string> female_names_;
    std::vector<std::string> clergy_names_;
    std::vector<std::string> epithets_;
    std::vector<std::string> bastard_cognomens_;
    std::vector<std::string> cadet_suffixes_;
    std::vector<std::string> traits_;

    std::vector<SwordStyleDef> sword_styles_;
    std::vector<MagicSchoolDef> magic_schools_;

    std::vector<RiteDef> rites_;
    std::vector<InitialTextDef> initial_texts_;
    std::vector<std::string> treatise_templates_;

    std::vector<InitialLawDef> initial_laws_;
    std::vector<EdictTemplateDef> edict_templates_;
};

} // namespace sovereignia::data

