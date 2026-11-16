#include "sovereignia/core/paths.hpp"
#include "sovereignia/ecs/components.hpp"
#include "sovereignia/sim/engine.hpp"
#include "sovereignia/ui/terminal_ui.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <iostream>
#include <string>
#include <string_view>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

struct CliOptions {
    std::uint64_t seed = 482ULL;
    int pregen_years = 200;
    int days = 0;
    int map_width = sovereignia::world::WorldMap::kDefaultWidth;
    int map_height = sovereignia::world::WorldMap::kDefaultHeight;
    sovereignia::ui::MapLens lens = sovereignia::ui::MapLens::Terrain;
    bool headless = false;
    bool render_snapshot = false;
    bool show_tree = false;
    bool show_help = false;
};

void printUsage() {
    std::cout
        << "Sovereignia (ソヴレィニア: 布告と典礼の王国)\n"
        << "Usage: sovereignia [options]\n"
        << "  --seed <N>          Deterministic world seed (default: 482)\n"
        << "  --pregen <years>    Fast-forward pre-history years on start (default: 200)\n"
        << "  --map-size <WxH>    World map grid size (default: 108x48)\n"
        << "  --lens <mode>       Map lens: terrain|forest|elevation|mana|threat\n"
        << "  --headless          Run without interactive TUI loop\n"
        << "  --days <N>          Advance N simulation days in headless mode (default: 0)\n"
        << "  --tree              Print the Capital dynasty's multi-generational family tree\n"
        << "  --render-snapshot   Print an FTXUI terminal snapshot in headless mode\n"
        << "  --help              Show this help message\n";
}

sovereignia::ui::MapLens parseLens(std::string_view v) noexcept {
    if (v == "forest" || v == "forestry") return sovereignia::ui::MapLens::Forestry;
    if (v == "elevation" || v == "elev") return sovereignia::ui::MapLens::Elevation;
    if (v == "mana") return sovereignia::ui::MapLens::Mana;
    if (v == "threat") return sovereignia::ui::MapLens::Threat;
    return sovereignia::ui::MapLens::Terrain;
}

CliOptions parseArgs(int argc, char** argv) {
    CliOptions opts;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--help" || arg == "-h") {
            opts.show_help = true;
        } else if (arg == "--headless") {
            opts.headless = true;
        } else if (arg == "--render-snapshot") {
            opts.render_snapshot = true;
        } else if (arg == "--tree") {
            opts.show_tree = true;
        } else if (arg == "--seed" && i + 1 < argc) {
            opts.seed = std::strtoull(argv[++i], nullptr, 10);
        } else if (arg == "--pregen" && i + 1 < argc) {
            opts.pregen_years = std::atoi(argv[++i]);
        } else if (arg == "--days" && i + 1 < argc) {
            opts.days = std::atoi(argv[++i]);
        } else if (arg == "--lens" && i + 1 < argc) {
            opts.lens = parseLens(argv[++i]);
        } else if (arg == "--map-size" && i + 1 < argc) {
            const std::string spec(argv[++i]);
            const auto sep = spec.find_first_of("xX");
            if (sep != std::string::npos) {
                const int w = std::atoi(spec.substr(0, sep).c_str());
                const int h = std::atoi(spec.substr(sep + 1).c_str());
                opts.map_width = std::clamp(w, 64, 240);
                opts.map_height = std::clamp(h, 28, 120);
            }
        }
    }
    return opts;
}

void printHeadlessSummary(const sovereignia::sim::SimulationEngine& engine) {
    const auto& reg = engine.registry();
    const auto& eco = engine.ecologyState();
    std::cout << std::format("=== Sovereignia Headless Report | {} | Map:{}x{} | Total Pop: {} ===\n",
                             engine.clock().formatFullDate(),
                             engine.map().width(),
                             engine.map().height(),
                             engine.totalKingdomPopulation());
    std::cout << std::format("  Autocephaly: {:.0f}% | Schism Tension: {:.0f}% | Synods: {} | Memorials: {} | Active Armies: {}\n",
                             engine.churchState().autocephaly * 100.0,
                             engine.churchState().schism_tension * 100.0,
                             engine.churchState().synods_held,
                             engine.memorialArchive().size(),
                             reg.view<sovereignia::ecs::ArmyComponent>().size());
    std::cout << std::format("  Ecology: Avg Canopy {:.0f}% | Forested:{}/{} | Old-Growth:{} | Cleared:{} | Regrown:{}\n",
                             eco.average_canopy * 100.0,
                             eco.forested_tiles,
                             eco.initial_forested_tiles,
                             eco.old_growth_tiles,
                             eco.tiles_cleared_total,
                             eco.tiles_regrown_total);

    for (const entt::entity s_ent : engine.orderedSettlements()) {
        const auto& name = reg.get<sovereignia::ecs::NameComponent>(s_ent).value;
        const auto& sett = reg.get<sovereignia::ecs::SettlementComponent>(s_ent);
        const auto& stock = reg.get<sovereignia::ecs::ResourceStockpileComponent>(s_ent);
        const auto& demo = reg.get<sovereignia::ecs::DemographicsComponent>(s_ent);
        std::cout << std::format("  {:<22} Pop:{:<4} Grain:{:>6.1f} ({:>+5.1f}/mo) Wood:{:>5.1f} ({:>+5.1f}/mo) Fed:{:>3.0f}% Woods:{:>2.0f}% [{}]\n",
                                 name,
                                 demo.totalPopulation(),
                                 stock.get(sovereignia::ResourceType::Grain),
                                 stock.delta(sovereignia::ResourceType::Grain),
                                 stock.get(sovereignia::ResourceType::Firewood),
                                 stock.delta(sovereignia::ResourceType::Firewood),
                                 demo.weightedFedRatio() * 100.0,
                                 sett.local_woodland_cover * 100.0,
                                 sovereignia::toShortString(sett.forestry_policy));
    }

    std::cout << "Recent Chronicle:\n";
    for (const auto& entry : engine.historyLog().recent(6)) {
        std::cout << std::format("  [{}, {}] [{}] {}\n",
                                 entry.year,
                                 sovereignia::toString(entry.season),
                                 sovereignia::toString(entry.category),
                                 entry.message);
    }
}

void printFamilyTree(const sovereignia::sim::SimulationEngine& engine) {
    const auto& reg = engine.registry();
    const auto& tree = engine.familyTree();
    const entt::entity cap = engine.worldInfo().capital_entity;
    if (cap == entt::null || !reg.valid(cap)) {
        return;
    }
    const entt::entity ruler = reg.get<sovereignia::ecs::SettlementComponent>(cap).ruler_entity;
    if (ruler == entt::null || !reg.valid(ruler) || !reg.all_of<sovereignia::ecs::FamilyComponent>(ruler)) {
        return;
    }
    const auto r_pid = reg.get<sovereignia::ecs::FamilyComponent>(ruler).person_id;
    const auto* rnode = tree.find(r_pid);
    if (rnode == nullptr) {
        return;
    }

    std::cout << std::format("\n=== Dynasty Family Tree: House {} ({} | Total Recorded Persons: {}) ===\n",
                             rnode->house, rnode->seat_name, tree.totalPersons());
    const auto ancestors = tree.ancestorChain(r_pid, 10);
    std::string indent;
    for (const auto aid : ancestors) {
        const auto* anc = tree.find(aid);
        if (anc == nullptr) continue;
        const std::string span_str = anc->alive
            ? std::format("b.{} M{} D{}, alive", anc->birth_year, anc->birth_month, anc->birth_day)
            : std::format("{} M{} D{} - {}", anc->birth_year, anc->birth_month, anc->birth_day, anc->death_year);
        std::cout << std::format("  {}[Gen {}] {} ({})\n",
                                 indent, anc->generation,
                                 tree.formatDisplayName(aid, false),
                                 span_str);
        indent += "  ";
    }
    std::cout << std::format("  {}[@] {} ({}, Age {}, {})\n",
                             indent,
                             tree.formatDisplayName(r_pid, false),
                             tree.formatBirthDate(r_pid),
                             rnode->current_age,
                             rnode->patronymic.empty() ? std::string("Founder") : rnode->patronymic);
    for (const auto spid : rnode->spouse_ids) {
        if (const auto* sp = tree.find(spid)) {
            std::cout << std::format("  {}|-- Spouse: {} (b.{} M{} D{}, House {}, {})\n",
                                     indent, sp->given_name, sp->birth_year, sp->birth_month, sp->birth_day,
                                     sp->birth_house, sp->patronymic);
        }
    }
    for (const auto cid : rnode->children_ids) {
        if (const auto* cn = tree.find(cid)) {
            std::cout << std::format("  {}|-- Child: {} ({}, MP:{:.0f})\n",
                                     indent,
                                     tree.formatDisplayName(cid, true),
                                     cn->alive ? std::format("Age {}, M{} D{}", cn->current_age, cn->birth_month, cn->birth_day)
                                               : std::format("d.{}", cn->death_year),
                                     cn->mana_capacity);
        }
    }
}

} // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    const CliOptions opts = parseArgs(argc, argv);
    if (opts.show_help) {
        printUsage();
        return 0;
    }

    try {
        sovereignia::sim::SimulationEngine engine;
        engine.loadData(sovereignia::core::resolveDataDir());
        const int start_year = (opts.pregen_years > 10) ? std::max(50, 482 - opts.pregen_years) : 480;
        engine.initializeWorld(opts.seed, start_year, opts.map_width, opts.map_height);
        engine.runPreHistory(opts.pregen_years);

        if (opts.headless) {
            if (opts.days > 0) {
                engine.advanceDays(opts.days);
            }
            printHeadlessSummary(engine);
            if (opts.show_tree) {
                printFamilyTree(engine);
            }
            if (opts.render_snapshot) {
                auto state = sovereignia::ui::makeInitialUIState(engine);
                state.map_lens = opts.lens;
                if (opts.show_tree) {
                    state.active_tab = sovereignia::ui::ActiveTab::FamilyTree;
                }
                std::cout << "\n" << sovereignia::ui::renderSnapshotToString(engine, state, 122, 38) << "\n";
            }
            return 0;
        }

        sovereignia::ui::runInteractiveApp(engine);
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Fatal error: " << ex.what() << "\n";
        return 1;
    }
}
