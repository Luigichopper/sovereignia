#include "sovereignia/data/config_registry.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <stdexcept>

namespace sovereignia::data {

namespace {

nlohmann::json readJsonFile(const std::filesystem::path& path) {
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        throw std::runtime_error("Failed to open JSON file: " + path.string());
    }
    nlohmann::json doc;
    ifs >> doc;
    return doc;
}

RgbColor parseRgb(const nlohmann::json& arr) {
    if (!arr.is_array() || arr.size() < 3) {
        return RgbColor{};
    }
    return RgbColor{
        arr[0].get<int>(),
        arr[1].get<int>(),
        arr[2].get<int>()
    };
}

} // namespace

void ConfigRegistry::loadFromDirectory(const std::filesystem::path& data_dir) {
    biomes_.clear();
    biome_index_map_.clear();
    settlements_.clear();
    houses_.clear();
    noble_titles_.clear();
    male_names_.clear();
    female_names_.clear();
    clergy_names_.clear();
    epithets_.clear();
    bastard_cognomens_.clear();
    cadet_suffixes_.clear();
    traits_.clear();
    sword_styles_.clear();
    magic_schools_.clear();
    rites_.clear();
    initial_texts_.clear();
    treatise_templates_.clear();
    initial_laws_.clear();
    edict_templates_.clear();

    // 1. Biomes
    {
        const auto doc = readJsonFile(data_dir / "biomes.json");
        for (const auto& jb : doc.at("biomes")) {
            BiomeDef def;
            def.id = jb.at("id").get<std::string>();
            def.name = jb.at("name").get<std::string>();
            const auto glyph_str = jb.at("glyph").get<std::string>();
            def.glyph = glyph_str.empty() ? '.' : glyph_str[0];
            def.fg = parseRgb(jb.at("fg_rgb"));
            def.bg = parseRgb(jb.at("bg_rgb"));
            def.fertility = jb.at("fertility").get<double>();
            def.timber_yield = jb.at("timber_yield").get<double>();
            def.ore_yield = jb.at("ore_yield").get<double>();
            def.fish_yield = jb.at("fish_yield").get<double>();
            def.base_mana = jb.at("base_mana").get<double>();
            def.move_cost = jb.at("move_cost").get<double>();
            def.walkable = jb.at("walkable").get<bool>();
            def.forest_capable = jb.value("forest_capable", false);
            def.is_water = jb.value("is_water", false);
            biome_index_map_[def.id] = static_cast<int>(biomes_.size());
            biomes_.push_back(std::move(def));
        }
    }

    // 2. Settlements
    {
        const auto doc = readJsonFile(data_dir / "settlements.json");
        kingdom_name_ = doc.value("kingdom_name", "Kingdom of Nordmark");
        for (const auto& js : doc.at("settlements")) {
            SettlementSeed seed;
            seed.id = js.at("id").get<std::string>();
            seed.name = js.at("name").get<std::string>();
            seed.type = parseSettlementType(js.at("type").get<std::string>());
            seed.region_hint = js.at("region_hint").get<std::string>();

            const auto& jc = js.at("cohorts");
            seed.cohorts[static_cast<std::size_t>(CohortType::Farmers)] = jc.value("Farmers", 0);
            seed.cohorts[static_cast<std::size_t>(CohortType::Fishermen)] = jc.value("Fishermen", 0);
            seed.cohorts[static_cast<std::size_t>(CohortType::Miners)] = jc.value("Miners", 0);
            seed.cohorts[static_cast<std::size_t>(CohortType::Lumberjacks)] = jc.value("Lumberjacks", 0);
            seed.cohorts[static_cast<std::size_t>(CohortType::Artisans)] = jc.value("Artisans", 0);

            const auto& jst = js.at("stockpiles");
            seed.stockpiles[static_cast<std::size_t>(ResourceType::Grain)] = jst.value("Grain", 0.0);
            seed.stockpiles[static_cast<std::size_t>(ResourceType::Fish)] = jst.value("Fish", 0.0);
            seed.stockpiles[static_cast<std::size_t>(ResourceType::Firewood)] = jst.value("Firewood", 0.0);
            seed.stockpiles[static_cast<std::size_t>(ResourceType::Timber)] = jst.value("Timber", 0.0);
            seed.stockpiles[static_cast<std::size_t>(ResourceType::Iron)] = jst.value("Iron", 0.0);
            seed.stockpiles[static_cast<std::size_t>(ResourceType::Silver)] = jst.value("Silver", 0.0);
            seed.stockpiles[static_cast<std::size_t>(ResourceType::MagicCrystals)] = jst.value("MagicCrystals", 0.0);

            seed.corvee_rate = js.value("corvee_rate", 0.15);
            seed.tithe_rate = js.value("tithe_rate", 0.10);
            seed.dominant_text = js.value("dominant_text", "The Frost Canon");
            seed.active_law = js.value("active_law", "The Vaelenian Frost Tithe of 479");

            const auto& jr = js.at("ruler");
            seed.ruler.name = jr.at("name").get<std::string>();
            seed.ruler.house = jr.at("house").get<std::string>();
            seed.ruler.title = jr.at("title").get<std::string>();
            seed.ruler.age = jr.value("age", 35);
            seed.ruler.female = jr.value("female", false);
            seed.ruler.trait = jr.value("trait", "Pragmatic");
            seed.ruler.sword_style = jr.value("sword_style", "None");
            seed.ruler.sword_rank = parseSkillRank(jr.value("sword_rank", "None"));
            seed.ruler.magic_school = jr.value("magic_school", "None");
            seed.ruler.magic_rank = parseSkillRank(jr.value("magic_rank", "None"));
            seed.ruler.mana_capacity = jr.value("mana_capacity", 20.0);
            seed.ruler.incantationless = jr.value("incantationless", false);
            seed.ruler.rite = parseRiteType(jr.value("rite", "NorthernAutocephalous"));
            seed.ruler.zeal = jr.value("zeal", 0.7);

            if (jr.contains("household")) {
                for (const auto& jh : jr.at("household")) {
                    HouseholdMemberSeed hm;
                    hm.name = jh.at("name").get<std::string>();
                    hm.role = jh.at("role").get<std::string>();
                    hm.age = jh.value("age", 25);
                    hm.note = jh.value("note", "");
                    seed.ruler.household.push_back(std::move(hm));
                }
            }

            settlements_.push_back(std::move(seed));
        }
    }

    // 3. Titles & Names
    {
        const auto doc = readJsonFile(data_dir / "titles.json");
        houses_ = doc.at("houses").get<std::vector<std::string>>();
        noble_titles_ = doc.at("noble_titles").get<std::vector<std::string>>();
        male_names_ = doc.at("male_names").get<std::vector<std::string>>();
        female_names_ = doc.at("female_names").get<std::vector<std::string>>();
        clergy_names_ = doc.value("clergy_names", std::vector<std::string>{});
        epithets_ = doc.value("epithets", std::vector<std::string>{});
        bastard_cognomens_ = doc.value("bastard_cognomens", std::vector<std::string>{});
        cadet_suffixes_ = doc.value("cadet_suffixes", std::vector<std::string>{});
        traits_ = doc.at("traits").get<std::vector<std::string>>();
    }

    // 4. Skills
    {
        const auto doc = readJsonFile(data_dir / "skills.json");
        for (const auto& jss : doc.at("sword_styles")) {
            SwordStyleDef def;
            def.name = jss.at("name").get<std::string>();
            def.philosophy = jss.at("philosophy").get<std::string>();
            def.description = jss.at("description").get<std::string>();
            sword_styles_.push_back(std::move(def));
        }
        for (const auto& jms : doc.at("magic_schools")) {
            MagicSchoolDef def;
            def.name = jms.at("name").get<std::string>();
            def.base_incantation_lines = jms.value("base_incantation_lines", 4);
            def.description = jms.at("description").get<std::string>();
            magic_schools_.push_back(std::move(def));
        }
    }

    // 5. Doctrines & Texts
    {
        const auto doc = readJsonFile(data_dir / "doctrines.json");
        for (const auto& jr : doc.at("rites")) {
            RiteDef def;
            def.id = jr.at("id").get<std::string>();
            def.type = parseRiteType(def.id);
            def.name = jr.at("name").get<std::string>();
            def.description = jr.at("description").get<std::string>();
            rites_.push_back(std::move(def));
        }
        for (const auto& jt : doc.at("initial_texts")) {
            InitialTextDef def;
            def.title = jt.at("title").get<std::string>();
            def.author = jt.at("author").get<std::string>();
            def.kind = jt.at("kind").get<std::string>();
            def.year = jt.value("year", 400);
            def.location = jt.at("location").get<std::string>();
            def.doctrine = jt.at("doctrine").get<std::string>();
            initial_texts_.push_back(std::move(def));
        }
        treatise_templates_ = doc.at("treatise_templates").get<std::vector<std::string>>();
    }

    // 6. Laws
    {
        const auto doc = readJsonFile(data_dir / "laws.json");
        for (const auto& jl : doc.at("initial_laws")) {
            InitialLawDef def;
            def.name = jl.at("name").get<std::string>();
            def.author = jl.at("author").get<std::string>();
            def.year = jl.value("year", 470);
            def.seat = jl.at("seat").get<std::string>();
            def.summary = jl.at("summary").get<std::string>();
            initial_laws_.push_back(std::move(def));
        }
        for (const auto& jet : doc.at("edict_templates")) {
            EdictTemplateDef def;
            def.kind = jet.at("kind").get<std::string>();
            def.name_pattern = jet.at("name_pattern").get<std::string>();
            def.summary = jet.at("summary").get<std::string>();
            edict_templates_.push_back(std::move(def));
        }
    }
}

int ConfigRegistry::biomeIndex(std::string_view id) const {
    const auto it = biome_index_map_.find(std::string(id));
    if (it == biome_index_map_.end()) {
        throw std::runtime_error("Unknown biome id: " + std::string(id));
    }
    return it->second;
}

const BiomeDef& ConfigRegistry::biomeByIndex(int index) const {
    return biomes_.at(static_cast<std::size_t>(index));
}

} // namespace sovereignia::data

