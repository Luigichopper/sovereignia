#include "sovereignia/ui/terminal_ui.hpp"

#include "sovereignia/ecs/components.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>
#include <ftxui/screen/screen.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <format>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace sovereignia::ui {

namespace {

using namespace ftxui;

Color categoryColor(LogCategory cat) {
    switch (cat) {
        case LogCategory::Economy: return Color::RGB(130, 210, 145);
        case LogCategory::Feudal: return Color::RGB(235, 190, 95);
        case LogCategory::Theology: return Color::RGB(175, 155, 245);
        case LogCategory::Diplomacy: return Color::RGB(120, 195, 245);
        case LogCategory::Season: return Color::RGB(185, 220, 235);
        case LogCategory::Eponym: return Color::RGB(255, 155, 110);
        case LogCategory::War: return Color::RGB(255, 105, 105);
        case LogCategory::Dynasty: return Color::RGB(245, 165, 210);
        case LogCategory::Ecology: return Color::RGB(115, 225, 165);
    }
    return Color::White;
}

Color settlementGlyphColor(SettlementType type, bool selected) {
    if (selected) {
        return Color::RGB(255, 245, 120);
    }
    switch (type) {
        case SettlementType::Capital: return Color::RGB(255, 215, 70);
        case SettlementType::Fortress: return Color::RGB(235, 115, 105);
        case SettlementType::Bishopric: return Color::RGB(205, 165, 255);
        case SettlementType::MiningCamp: return Color::RGB(175, 200, 235);
        case SettlementType::LumberCamp: return Color::RGB(120, 225, 130);
        case SettlementType::FishingPort: return Color::RGB(105, 225, 245);
        case SettlementType::FarmingVillage: return Color::RGB(235, 235, 195);
    }
    return Color::White;
}

void siteColors(MapSiteKind kind, Color& fg, Color& bg) {
    switch (kind) {
        case MapSiteKind::AncientRuin:
            fg = Color::RGB(215, 175, 255);
            bg = Color::RGB(36, 22, 42);
            break;
        case MapSiteKind::BattleSite:
            fg = Color::RGB(240, 120, 145);
            bg = Color::RGB(48, 18, 26);
            break;
        case MapSiteKind::Hermitage:
            fg = Color::RGB(195, 210, 255);
            bg = Color::RGB(28, 30, 52);
            break;
        case MapSiteKind::Watchtower:
            fg = Color::RGB(255, 200, 95);
            bg = Color::RGB(46, 32, 16);
            break;
        case MapSiteKind::ManaWell:
            fg = Color::RGB(115, 245, 255);
            bg = Color::RGB(14, 46, 58);
            break;
        case MapSiteKind::SacredGrove:
            fg = Color::RGB(175, 255, 125);
            bg = Color::RGB(18, 48, 20);
            break;
        case MapSiteKind::None:
            break;
    }
}

std::string formatDelta(double val) {
    if (val >= 0.0) {
        return std::format("+{:.0f}", val);
    }
    return std::format("{:.0f}", val);
}

std::string logFilterLabel(int category_filter) {
    if (category_filter < 0 || category_filter > static_cast<int>(LogCategory::Ecology)) {
        return "ALL";
    }
    return std::string(toString(static_cast<LogCategory>(category_filter)));
}

void applyLensColors(MapLens lens,
                     const world::MapCell& cell,
                     const data::BiomeDef& biome,
                     double local_threat,
                     Color& fg,
                     Color& bg) {
    if (lens == MapLens::Terrain) {
        return;
    }

    if (lens == MapLens::Forestry) {
        if (biome.is_water) {
            fg = Color::RGB(60, 110, 165);
            bg = Color::RGB(10, 22, 42);
            return;
        }
        if (!biome.forest_capable) {
            fg = Color::RGB(115, 118, 128);
            bg = Color::RGB(24, 26, 32);
            return;
        }
        const double fc = cell.forest_cover;
        if (fc >= 0.85) {
            fg = Color::RGB(95, 245, 125);
            bg = Color::RGB(10, 56, 22);
        } else if (fc >= 0.58) {
            fg = Color::RGB(85, 205, 105);
            bg = Color::RGB(16, 46, 20);
        } else if (fc >= 0.30) {
            fg = Color::RGB(140, 205, 95);
            bg = Color::RGB(24, 44, 18);
        } else if (fc >= 0.14) {
            fg = Color::RGB(190, 180, 95);
            bg = Color::RGB(34, 34, 18);
        } else {
            fg = Color::RGB(175, 130, 90);
            bg = Color::RGB(38, 24, 16);
        }
        return;
    }

    if (lens == MapLens::Elevation) {
        if (biome.is_water) {
            fg = Color::RGB(70, 145, 210);
            bg = Color::RGB(10, 24, 52);
        } else if (cell.elevation >= 0.86) {
            fg = Color::RGB(245, 250, 255);
            bg = Color::RGB(82, 102, 128);
        } else if (cell.elevation >= 0.68) {
            fg = Color::RGB(220, 195, 155);
            bg = Color::RGB(66, 52, 40);
        } else if (cell.elevation >= 0.48) {
            fg = Color::RGB(205, 175, 105);
            bg = Color::RGB(48, 40, 24);
        } else if (cell.elevation >= 0.28) {
            fg = Color::RGB(145, 210, 115);
            bg = Color::RGB(24, 48, 24);
        } else {
            fg = Color::RGB(130, 225, 165);
            bg = Color::RGB(18, 42, 34);
        }
        return;
    }

    if (lens == MapLens::Mana) {
        const double m = cell.mana_density;
        if (m >= 1.25) {
            fg = Color::RGB(145, 250, 255);
            bg = Color::RGB(26, 68, 92);
        } else if (m >= 0.85) {
            fg = Color::RGB(195, 160, 255);
            bg = Color::RGB(42, 24, 68);
        } else if (m >= 0.55) {
            fg = Color::RGB(125, 175, 245);
            bg = Color::RGB(22, 34, 58);
        } else {
            fg = Color::RGB(105, 120, 140);
            bg = Color::RGB(16, 20, 28);
        }
        return;
    }

    if (lens == MapLens::Threat) {
        if (biome.is_water) {
            fg = Color::RGB(60, 110, 165);
            bg = Color::RGB(10, 22, 42);
        } else if (local_threat >= 0.24) {
            fg = Color::RGB(255, 125, 120);
            bg = Color::RGB(74, 20, 22);
        } else if (local_threat >= 0.14) {
            fg = Color::RGB(245, 195, 100);
            bg = Color::RGB(56, 38, 16);
        } else {
            fg = Color::RGB(120, 215, 145);
            bg = Color::RGB(18, 42, 24);
        }
    }
}

Element renderHeader(const sim::SimulationEngine& engine, const UIState& state) {
    const auto& reg = engine.registry();
    const auto& clock = engine.clock();

    double emp_favor = 0.6;
    if (engine.worldInfo().empire_entity != entt::null &&
        reg.valid(engine.worldInfo().empire_entity)) {
        emp_favor = reg.get<ecs::OffMapEmpireComponent>(engine.worldInfo().empire_entity).favor;
    }

    const std::string speed_badge = state.paused
        ? "[PAUSED]"
        : std::format("[PLAY {}x]", state.speed_level);

    auto status_color = state.paused ? Color::RGB(235, 175, 85) : Color::RGB(120, 235, 145);

    Elements items{
        text(" ソヴレィニア SOVEREIGNIA ") | bold | color(Color::RGB(255, 220, 120)),
        text("| 布告と典礼の王国 ") | color(Color::RGB(185, 200, 220)),
        text("| ") | dim,
        text(engine.config().kingdomName()) | bold | color(Color::RGB(215, 235, 255))
    };

    if (!state.status_toast.empty()) {
        items.push_back(text(" | ") | dim);
        items.push_back(text(state.status_toast) | color(Color::RGB(145, 240, 185)));
    }

    items.push_back(filler());
    items.push_back(text(std::format("Pop:{} Imp:{:.0f}% ",
                                     engine.totalKingdomPopulation(), emp_favor * 100.0)) | color(Color::RGB(190, 205, 215)));
    items.push_back(text(clock.formatFullDate()) | bold | color(Color::RGB(255, 235, 165)));
    items.push_back(text(" "));
    items.push_back(text(speed_badge) | bold | color(status_color));
    items.push_back(text(" "));

    return hbox(std::move(items)) | bgcolor(Color::RGB(18, 24, 36));
}

Element renderMapPanel(const sim::SimulationEngine& engine, const UIState& state, int max_map_w, int max_map_h) {
    const auto& map = engine.map();
    const auto& reg = engine.registry();
    const auto& config = engine.config();

    std::unordered_map<int, const ecs::CaravanComponent*> caravan_cells;
    const auto caravan_view = reg.view<ecs::CaravanComponent, ecs::GridPosComponent>();
    for (const entt::entity ce : caravan_view) {
        const auto& pos = caravan_view.get<ecs::GridPosComponent>(ce).pos;
        caravan_cells[pos.y * map.width() + pos.x] = &caravan_view.get<ecs::CaravanComponent>(ce);
    }

    std::unordered_map<int, const ecs::ArmyComponent*> army_cells;
    const auto army_view = reg.view<ecs::ArmyComponent, ecs::GridPosComponent>();
    for (const entt::entity ae : army_view) {
        const auto& pos = army_view.get<ecs::GridPosComponent>(ae).pos;
        army_cells[pos.y * map.width() + pos.x] = &army_view.get<ecs::ArmyComponent>(ae);
    }

    struct ThreatSource {
        GridPos pos;
        double threat = 0.0;
    };
    std::vector<ThreatSource> threat_sources;
    if (state.map_lens == MapLens::Threat) {
        for (const entt::entity se : engine.orderedSettlements()) {
            threat_sources.push_back(ThreatSource{
                reg.get<ecs::GridPosComponent>(se).pos,
                reg.get<ecs::SettlementComponent>(se).bandit_threat
            });
        }
    }

    const int view_w = std::clamp(max_map_w, 24, map.width());
    const int view_h = std::clamp(max_map_h, 10, map.height());

    const int cam_x = std::clamp(state.cursor_x - view_w / 2, 0, std::max(0, map.width() - view_w));
    const int cam_y = std::clamp(state.cursor_y - view_h / 2, 0, std::max(0, map.height() - view_h));

    Elements rows;
    rows.reserve(static_cast<std::size_t>(view_h));

    for (int sy = 0; sy < view_h; ++sy) {
        const int y = cam_y + sy;
        Elements cols;
        cols.reserve(static_cast<std::size_t>(view_w));

        for (int sx = 0; sx < view_w; ++sx) {
            const int x = cam_x + sx;
            const auto& cell = map.at(x, y);
            const auto& biome = config.biomeByIndex(cell.biome_index);

            char ch = biome.glyph;
            Color fg = Color::RGB(
                static_cast<std::uint8_t>(biome.fg.r),
                static_cast<std::uint8_t>(biome.fg.g),
                static_cast<std::uint8_t>(biome.fg.b));
            Color bg = Color::RGB(
                static_cast<std::uint8_t>(biome.bg.r),
                static_cast<std::uint8_t>(biome.bg.g),
                static_cast<std::uint8_t>(biome.bg.b));
            bool is_bold = false;

            if (cell.has_river && cell.road_level == RoadLevel::None && cell.settlement_entity == entt::null) {
                ch = '~';
                if (cell.river_width >= 2) {
                    fg = Color::RGB(130, 215, 255);
                    bg = Color::RGB(18, 48, 86);
                    is_bold = true;
                } else {
                    fg = Color::RGB(95, 185, 245);
                    bg = Color::RGB(16, 40, 68);
                }
            }

            if (cell.road_level == RoadLevel::DirtRoad) {
                ch = '.';
                fg = Color::RGB(210, 175, 115);
            } else if (cell.road_level == RoadLevel::ImperialHighway) {
                ch = '=';
                fg = Color::RGB(250, 210, 105);
                is_bold = true;
            }

            if (cell.has_bridge && cell.settlement_entity == entt::null) {
                ch = '=';
                fg = Color::RGB(235, 220, 185);
                bg = Color::RGB(30, 52, 78);
                is_bold = true;
            }

            if (state.map_lens != MapLens::Terrain) {
                double local_threat = 0.06;
                if (state.map_lens == MapLens::Threat) {
                    double best_d = 999.0;
                    for (const auto& ts : threat_sources) {
                        const double d = std::hypot(static_cast<double>(ts.pos.x - x),
                                                    static_cast<double>(ts.pos.y - y));
                        if (d < best_d) {
                            best_d = d;
                            local_threat = ts.threat;
                        }
                    }
                    if (x <= map.width() / 7 || y >= map.height() - 6) {
                        local_threat = std::max(local_threat, 0.25);
                    }
                }
                applyLensColors(state.map_lens, cell, biome, local_threat, fg, bg);
            }

            if (cell.site_kind != MapSiteKind::None && cell.settlement_entity == entt::null) {
                ch = siteGlyph(cell.site_kind);
                siteColors(cell.site_kind, fg, bg);
                is_bold = true;
            }

            const auto cit = caravan_cells.find(y * map.width() + x);
            if (cit != caravan_cells.end() && cell.settlement_entity == entt::null) {
                ch = '$';
                fg = Color::RGB(255, 240, 110);
                bg = Color::RGB(60, 42, 12);
                is_bold = true;
            }

            const auto ait = army_cells.find(y * map.width() + x);
            if (ait != army_cells.end() && cell.settlement_entity == entt::null) {
                ch = '!';
                fg = ait->second->is_foreign_raider ? Color::RGB(255, 95, 95) : Color::RGB(130, 235, 255);
                bg = Color::RGB(58, 18, 22);
                is_bold = true;
            }

            if (cell.settlement_entity != entt::null && reg.valid(cell.settlement_entity)) {
                const auto& sc = reg.get<ecs::SettlementComponent>(cell.settlement_entity);
                ch = settlementGlyph(sc.type);
                const bool is_sel = (cell.settlement_entity == state.selected_settlement);
                fg = settlementGlyphColor(sc.type, is_sel);
                bg = is_sel ? Color::RGB(75, 58, 22) : Color::RGB(28, 30, 38);
                is_bold = true;
            }

            Element glyph_el = text(std::string(1, ch)) | color(fg) | bgcolor(bg);
            if (is_bold) {
                glyph_el = glyph_el | bold;
            }
            if (x == state.cursor_x && y == state.cursor_y) {
                glyph_el = glyph_el | inverted;
            }
            cols.push_back(std::move(glyph_el));
        }
        rows.push_back(hbox(std::move(cols)));
    }

    std::string cursor_line = "No cell";
    if (map.inBounds(state.cursor_x, state.cursor_y)) {
        const auto& cell = map.at(state.cursor_x, state.cursor_y);
        const auto& biome = config.biomeByIndex(cell.biome_index);
        const int elev_m = static_cast<int>(std::round(cell.elevation * 2200.0));

        std::string feat;
        if (cell.settlement_entity != entt::null && reg.valid(cell.settlement_entity)) {
            const auto& sn = reg.get<ecs::NameComponent>(cell.settlement_entity).value;
            const auto& sc = reg.get<ecs::SettlementComponent>(cell.settlement_entity);
            feat = std::format("{} ({})", sn, toString(sc.type));
        } else if (const auto ait = army_cells.find(state.cursor_y * map.width() + state.cursor_x); ait != army_cells.end()) {
            feat = std::format("Army:{} ({}Sp)", ait->second->banner_name, ait->second->levied_spearmen);
        } else if (const auto cit = caravan_cells.find(state.cursor_y * map.width() + state.cursor_x); cit != caravan_cells.end()) {
            feat = std::format("Caravan:{}", toString(cit->second->kind));
        } else if (cell.site_kind != MapSiteKind::None) {
            feat = cell.site_name;
        } else if (cell.has_bridge) {
            feat = "Stone/Timber Bridge";
        } else if (cell.has_river) {
            feat = cell.river_width >= 2 ? "Major River" : "Stream";
        } else {
            feat = std::string(toString(cell.road_level));
        }

        cursor_line = std::format("({},{}) {} | {}m C:{:.0f}% S:{:.0f}% M:{:.2f} | {}",
                                  state.cursor_x, state.cursor_y,
                                  biome.name,
                                  elev_m,
                                  cell.forest_cover * 100.0,
                                  cell.soil_fertility * 100.0,
                                  cell.mana_density,
                                  feat);
    }

    const std::string legend =
        "@Cap #Fort +See M:Mine T:Lumb &:Port o:Vill $=Carv !=Army %=Ruin x:Bat h:Herm I:Tow *:Mana Y:Grove";

    return window(
        text(std::format(" [M]AP: {} [{}x{} | Lens: {}] ",
                         engine.config().kingdomName(),
                         map.width(), map.height(),
                         toString(state.map_lens)))
            | bold | color(Color::RGB(255, 225, 130)),
        vbox({
            vbox(std::move(rows)) | center,
            separatorLight(),
            text(cursor_line) | color(Color::RGB(215, 230, 245)),
            text(legend) | dim
        })
    );
}

Element renderHelpPanel(const UIState& state) {
    Elements lines{
        text(" NAVIGATION & CAMERA") | bold | color(Color::RGB(255, 225, 130)),
        text("  Arrows / W,A,D : Move cursor 1 tile    H,J,K,L : Fast pan 5 tiles"),
        text("  Z              : Center on seat        TAB / ] / [ : Next / Prev Seat"),
        separatorLight(),
        text(" MAP LENSES, ECOLOGY & LOGS") | bold | color(Color::RGB(135, 230, 160)),
        text(std::format("  V     : Cycle Map Lens (Current: {})", toString(state.map_lens))),
        text("  F     : Cycle Forestry Charter (Coppice -> Clear-Cut -> Reforest)"),
        text(std::format("  G     : Filter Chronicle Category (Current: {})", logFilterLabel(state.log_category_filter))),
        text("  PgUp / PgDn : Scroll Kingdom Chronicle History Log"),
        separatorLight(),
        text(" TIME & RULER ACTIONS") | bold | color(Color::RGB(150, 225, 255)),
        text("  SPACE : Pause/Play    . / , / ; : Advance +1d / +30d / +90d"),
        text("  1..4  : Speed 1x..4x  M/N/T/R/C : Map / Nobility / Family Tree / Rel / Chron"),
        text("  E     : Issue Edict   B         : Levy & Mobilize Banner"),
        text("  U     : Commission Building / Road Upgrade   O : Envoy / Counter-Raid"),
        text("  s / S : Synod (North / Imperial Canon)       ? : Close Help   Q : Quit"),
        separatorLight(),
        text(" MAP GLYPHS") | bold | color(Color::RGB(215, 185, 255)),
        text("  Water: ~(Sea) -(Shallows) ~(Lake)   Woodland: F(Old) f(Pine) t(Birch) \"(Cleared)"),
        text("  Land : .(Plain) :(Valley) '(Tundra) ;(Moor) n(Hills) ^(Crags) A(Glacier)"),
        text("  Sites: %(Ruin) x(Battle) h(Hermitage) I(Watchtower) *(ManaWell) Y(SacredGrove)")
    };

    return window(
        text(" [?] HELP, CONTROLS & MAP LEGEND ") | bold | color(Color::RGB(255, 225, 130)),
        vbox(std::move(lines))
    );
}

Element renderNobilityTab(const sim::SimulationEngine& engine, const UIState& state) {
    const auto& reg = engine.registry();
    Elements rows;
    rows.push_back(hbox({
        text(" Seat              ") | bold,
        text("Ruler & House               ") | bold,
        text("Age ") | bold,
        text("Sword Style/Rank      ") | bold,
        text("Magic/Mana") | bold
    }) | color(Color::RGB(255, 220, 130)));
    rows.push_back(separatorLight());

    for (const entt::entity s_ent : engine.orderedSettlements()) {
        const auto& s_name = reg.get<ecs::NameComponent>(s_ent).value;
        const auto& sett = reg.get<ecs::SettlementComponent>(s_ent);
        if (sett.ruler_entity == entt::null || !reg.valid(sett.ruler_entity)) {
            continue;
        }
        const auto& r_name = reg.get<ecs::NameComponent>(sett.ruler_entity).value;
        const auto& ch = reg.get<ecs::CharacterComponent>(sett.ruler_entity);
        const auto& soc = reg.get<ecs::SocialStatusComponent>(sett.ruler_entity);
        const auto& mm = reg.get<ecs::MartialMagicComponent>(sett.ruler_entity);

        const std::string seat_col = std::format("{:<18}", s_name.substr(0, 17));
        const std::string ruler_col = std::format("{:<27}", (r_name + " (" + soc.house + ")").substr(0, 26));
        const std::string age_col = std::format("{:<3} ", ch.age);
        const std::string sword_col = std::format("{:<21}",
            (mm.sword_rank == SkillRank::None ? std::string("None") : mm.sword_style + " " + std::string(toString(mm.sword_rank))).substr(0, 20));
        const std::string magic_col = mm.magic_rank == SkillRank::None
            ? std::format("None ({:.0f}MP)", mm.mana_capacity)
            : std::format("{} ({:.0f}MP{})", toString(mm.magic_rank), mm.mana_capacity, mm.incantationless ? ",Silent" : "");

        auto row = hbox({
            text(" " + seat_col),
            text(ruler_col),
            text(age_col),
            text(sword_col),
            text(magic_col)
        });
        if (s_ent == state.selected_settlement) {
            row = row | bgcolor(Color::RGB(45, 40, 22)) | bold;
        }
        rows.push_back(std::move(row));
    }

    rows.push_back(separatorLight());
    const auto army_view = reg.view<ecs::ArmyComponent>();
    if (!army_view.empty()) {
        int shown_armies = 0;
        for (const entt::entity ae : army_view) {
            if (shown_armies >= 2) break;
            const auto& arm = army_view.get<ecs::ArmyComponent>(ae);
            rows.push_back(text(std::format(" ! Army: {} ({} Sp, {} Kt, {} Mg){}",
                                            arm.banner_name, arm.levied_spearmen, arm.armored_knights,
                                            arm.court_mages, arm.returning_home ? " [Returning]" : ""))
                           | color(arm.is_foreign_raider ? Color::RGB(255, 115, 115) : Color::RGB(130, 225, 255)));
            ++shown_armies;
        }
    }
    const auto& memorials = engine.memorialArchive();
    if (!memorials.empty()) {
        rows.push_back(text(std::format(" Memorial Hall ({} departed):", memorials.size())) | bold | color(Color::RGB(210, 210, 225)));
        const std::size_t show = std::min<std::size_t>(3, memorials.size());
        for (std::size_t i = 0; i < show; ++i) {
            const auto& m = memorials[memorials.size() - 1 - i];
            rows.push_back(text(std::format("  * [{}] {} ({}, age {}): {}",
                                            m.death_year, m.name, m.title, m.age_at_death, m.epitaph)) | dim);
        }
    }

    return window(
        text(" [N]OBILITY & DYNASTIC HOUSES ") | bold | color(Color::RGB(255, 225, 130)),
        vbox(std::move(rows))
    );
}

Element renderReligionTab(const sim::SimulationEngine& engine) {
    const auto& reg = engine.registry();
    const auto& church = engine.churchState();
    Elements lines;

    lines.push_back(text(std::format(" AUTOCEPHALY: {:.0f}% | SCHISM TENSION: {:.0f}% | SYNODS: {}{}",
                                     church.autocephaly * 100.0,
                                     church.schism_tension * 100.0,
                                     church.synods_held,
                                     church.in_formal_schism ? " [SCHISM]" : ""))
                    | bold | color(Color::RGB(205, 175, 255)));
    lines.push_back(text(std::format(" SAINTS: {} | HERESIES: {} | COPIED: {} | Controversy: '{}'",
                                     church.canonized_saints,
                                     church.active_heresies_count,
                                     church.texts_circulated,
                                     church.active_controversy)) | dim);
    lines.push_back(separatorLight());

    const auto emp_view = reg.view<ecs::OffMapEmpireComponent>();
    for (const entt::entity ee : emp_view) {
        const auto& emp = emp_view.get<ecs::OffMapEmpireComponent>(ee);
        std::string tags;
        if (emp.has_trade_treaty) tags += " [Treaty]";
        if (emp.has_imperial_charter) tags += " [Charter]";
        if (emp.truce_years_left > 0) tags += std::format(" [Truce {}y]", emp.truce_years_left);
        if (emp.envoys_exchanged > 0) tags += std::format(" Env:{}", emp.envoys_exchanged);
        lines.push_back(text(std::format(" * {} — {} | Rel:{:+d} | Favor:{:.0f}% | Trib:{:.0f} Ag ({}){}",
                                         emp.title, emp.ruler_name, emp.relations,
                                         emp.favor * 100.0, emp.tribute_owed_silver, emp.stance, tags)));
    }

    lines.push_back(separatorLight());
    lines.push_back(text(" PHYSICAL TEXTS, ICONS & MONASTIC TREATISES") | bold | color(Color::RGB(185, 220, 255)));
    const auto text_view = reg.view<ecs::LiteraryWorkComponent>();
    int shown = 0;
    for (const entt::entity te : text_view) {
        const auto& tw = text_view.get<ecs::LiteraryWorkComponent>(te);
        std::string holder = tw.location;
        if (tw.holder_settlement != entt::null && reg.valid(tw.holder_settlement) &&
            reg.all_of<ecs::NameComponent>(tw.holder_settlement)) {
            holder = reg.get<ecs::NameComponent>(tw.holder_settlement).value;
        }
        lines.push_back(text(std::format(" - [{}] '{}' ({}) by {} @ {} [Trait:{}]",
                                         tw.year, tw.title, tw.kind, tw.author, holder, tw.trait_imprint)));
        if (++shown >= 7) {
            break;
        }
    }

    return window(
        text(" [R]ELIGION, THEOLOGY & EMPIRE ") | bold | color(Color::RGB(205, 175, 255)),
        vbox(std::move(lines))
    );
}

Element renderChronicleTab(const sim::SimulationEngine& engine) {
    const auto& reg = engine.registry();
    const auto& map = engine.map();
    const auto& eco = engine.ecologyState();
    Elements lines;

    lines.push_back(text(" KINGDOM WOODLAND ECOLOGY") | bold | color(Color::RGB(125, 230, 160)));
    lines.push_back(text(std::format(" Canopy: {:.0f}% | Forested: {}/{} (Old-Growth: {}, Cleared: {}, Regrown: {})",
                                     eco.average_canopy * 100.0,
                                     eco.forested_tiles,
                                     eco.initial_forested_tiles,
                                     eco.old_growth_tiles,
                                     eco.tiles_cleared_total,
                                     eco.tiles_regrown_total)) | color(Color::RGB(170, 230, 180)));
    lines.push_back(separatorLight());

    lines.push_back(text(" REGISTERED EPONYMOUS LAWS & THEOREMS") | bold | color(Color::RGB(255, 185, 120)));
    const auto law_view = reg.view<ecs::ActiveLawComponent>();
    int law_count = 0;
    for (const entt::entity le : law_view) {
        const auto& law = law_view.get<ecs::ActiveLawComponent>(le);
        lines.push_back(text(std::format(" * [{}] {} ({}) — {}", law.enacted_year, law.name, law.seat_name, law.author)));
        if (++law_count >= 4) {
            break;
        }
    }

    lines.push_back(separatorLight());
    lines.push_back(text(" RUINS, GROVES, TOWERS & HISTORIC BATTLEFIELDS") | bold | color(Color::RGB(215, 175, 255)));
    int site_count = 0;
    for (int y = 0; y < map.height() && site_count < 4; ++y) {
        for (int x = 0; x < map.width() && site_count < 4; ++x) {
            const auto& cell = map.at(x, y);
            if (cell.site_kind != MapSiteKind::None) {
                lines.push_back(text(std::format(" {} ({},{}) [{}] {}",
                                                 siteGlyph(cell.site_kind),
                                                 x, y, toString(cell.site_kind), cell.site_name)));
                ++site_count;
            }
        }
    }

    lines.push_back(separatorLight());
    lines.push_back(text(" RECENT KINGDOM CHRONICLE") | bold | color(Color::RGB(185, 225, 240)));
    for (const auto& entry : engine.historyLog().recent(4)) {
        lines.push_back(text(std::format(" [{}, {}] [{}] {}",
                                         entry.year, toString(entry.season),
                                         toString(entry.category), entry.message))
                        | color(categoryColor(entry.category)));
    }

    return window(
        text(" [C]HRONICLE & NAMED LAWS ") | bold | color(Color::RGB(255, 185, 120)),
        vbox(std::move(lines))
    );
}

Element renderFamilyTreeTab(const sim::SimulationEngine& engine, const UIState& state) {
    const auto& reg = engine.registry();
    const auto& tree = engine.familyTree();

    if (state.selected_settlement == entt::null || !reg.valid(state.selected_settlement)) {
        return window(text(" [T] FAMILY TREE & DYNASTIC LINEAGE ") | bold, text("No settlement selected."));
    }

    const entt::entity s_ent = state.selected_settlement;
    const auto& s_name = reg.get<ecs::NameComponent>(s_ent).value;
    const auto& sett = reg.get<ecs::SettlementComponent>(s_ent);

    sim::PersonId ruler_pid = sim::kInvalidPersonId;
    std::string house_name = "Vaelen";
    if (sett.ruler_entity != entt::null && reg.valid(sett.ruler_entity)) {
        if (reg.all_of<ecs::FamilyComponent>(sett.ruler_entity)) {
            ruler_pid = reg.get<ecs::FamilyComponent>(sett.ruler_entity).person_id;
        }
        if (reg.all_of<ecs::SocialStatusComponent>(sett.ruler_entity)) {
            house_name = reg.get<ecs::SocialStatusComponent>(sett.ruler_entity).house;
        }
    }

    const sim::PersonNode* rnode = tree.find(ruler_pid);
    if (rnode == nullptr) {
        return window(text(" [T] FAMILY TREE & DYNASTIC LINEAGE ") | bold, text("No recorded dynasty."));
    }

    Elements lines;
    lines.push_back(text(std::format(" House {} ({}) | Generation {} | Rulers:{} | Kin:{}",
                                     house_name, s_name, rnode->generation,
                                     tree.houseTotalRulers(house_name),
                                     tree.houseTotalMembers(house_name)))
                    | bold | color(Color::RGB(255, 225, 130)));
    lines.push_back(separatorLight());

    // Ancestral / Ruling Line
    lines.push_back(text(" ANCESTRAL & RULING LINE") | bold | color(Color::RGB(185, 225, 255)));
    const auto ancestors = tree.ancestorChain(ruler_pid, 5);
    std::string indent;
    for (std::size_t i = 0; i < ancestors.size(); ++i) {
        const sim::PersonNode* anc = tree.find(ancestors[i]);
        if (anc == nullptr) continue;
        const std::string span_str = anc->alive
            ? std::format("b.{} M{} D{}, alive", anc->birth_year, anc->birth_month, anc->birth_day)
            : std::format("{} M{} D{}-{}", anc->birth_year, anc->birth_month, anc->birth_day, anc->death_year);
        std::string sp_note;
        if (!anc->spouse_ids.empty()) {
            if (const sim::PersonNode* sp = tree.find(anc->spouse_ids.front())) {
                sp_note = " m." + sp->given_name;
            }
        }
        const std::string rank_note = (anc->sword_rank >= SkillRank::Intermediate)
            ? std::format(" [{}{}]", toString(anc->sword_rank), anc->incantationless ? ",Silent" : "")
            : (anc->incantationless ? std::string(" [Silent]") : std::string());
        lines.push_back(text(std::format(" {}+-- [Gen {}] {} ({}){} {}",
                                         indent, anc->generation,
                                         tree.formatDisplayName(anc->id, false),
                                         span_str, sp_note, rank_note)) | dim);
        if (indent.size() < 8) {
            indent += "  ";
        }
    }

    // Current Ruler & Consorts
    lines.push_back(text(std::format(" {}[@] {} ({}, Age {}, {})",
                                     indent,
                                     tree.formatDisplayName(rnode->id, false),
                                     tree.formatBirthDate(rnode->id),
                                     rnode->current_age,
                                     rnode->patronymic.empty() ? std::string("Founder") : rnode->patronymic))
                    | bold | color(Color::RGB(255, 235, 145)));

    int shown_sp = 0;
    for (const sim::PersonId spid : rnode->spouse_ids) {
        if (shown_sp >= 2) break;
        const sim::PersonNode* sp = tree.find(spid);
        if (sp == nullptr) continue;
        const std::string life = sp->alive
            ? std::format("Age {} M{} D{}", sp->current_age, sp->birth_month, sp->birth_day)
            : std::format("+d.{}", sp->death_year);
        lines.push_back(text(std::format(" {}|-- (=) Spouse: {} ({}, House {})",
                                         indent, sp->given_name, life, sp->birth_house))
                        | color(Color::RGB(235, 195, 230)));
        ++shown_sp;
    }
    if (!rnode->concubine_ids.empty()) {
        const sim::PersonNode* conc = tree.find(rnode->concubine_ids.front());
        if (conc != nullptr) {
            const std::string life = conc->alive
                ? std::format("Age {} M{} D{}", conc->current_age, conc->birth_month, conc->birth_day)
                : std::format("+d.{}", conc->death_year);
            lines.push_back(text(std::format(" {}|-- (~) Concubine: {} ({}, House {})",
                                             indent, conc->given_name, life, conc->birth_house))
                            | color(Color::RGB(220, 175, 200)));
        }
    }

    // Children & Heirs
    lines.push_back(separatorLight());
    lines.push_back(text(std::format(" CHILDREN & HEIRS ({})", rnode->children_ids.size()))
                    | bold | color(Color::RGB(165, 230, 175)));
    if (rnode->children_ids.empty()) {
        lines.push_back(text("  (No recorded issue yet)") | dim);
    } else {
        const std::size_t show_ch = std::min<std::size_t>(5, rnode->children_ids.size());
        for (std::size_t i = 0; i < show_ch; ++i) {
            const sim::PersonNode* cn = tree.find(rnode->children_ids[i]);
            if (cn == nullptr) continue;
            const std::string role_tag = cn->is_bastard ? "Bastard"
                : (cn->was_ruler ? "Ruler" : (i == 0 ? "Heir" : "Scion"));
            const std::string life = cn->alive
                ? std::format("Age {} M{} D{}", cn->current_age, cn->birth_month, cn->birth_day)
                : std::format("+d.{}", cn->death_year);
            std::string mom_name = "?";
            if (const sim::PersonNode* mn = tree.find(cn->mother_id)) {
                mom_name = mn->given_name;
            }
            lines.push_back(text(std::format("  |-- [{}] {} ({}, Mom:{}, MP:{:.0f}{})",
                                             role_tag,
                                             tree.formatDisplayName(cn->id, true),
                                             life,
                                             mom_name,
                                             cn->mana_capacity,
                                             cn->incantationless ? ",Silent" : "")));
        }
        if (rnode->children_ids.size() > show_ch) {
            lines.push_back(text(std::format("   (+{} more recorded children)", rnode->children_ids.size() - show_ch)) | dim);
        }
    }

    // Siblings & Half-Siblings
    const auto sibs = tree.siblingsOf(ruler_pid);
    if (!sibs.empty()) {
        lines.push_back(separatorLight());
        lines.push_back(text(std::format(" SIBLINGS & HALF-SIBLINGS ({})", sibs.size()))
                        | bold | color(Color::RGB(185, 215, 240)));
        const std::size_t show_sib = std::min<std::size_t>(3, sibs.size());
        for (std::size_t i = 0; i < show_sib; ++i) {
            const sim::PersonNode* sn = tree.find(sibs[i]);
            if (sn == nullptr) continue;
            const bool full_sib = (sn->father_id == rnode->father_id && sn->mother_id == rnode->mother_id);
            const std::string life = sn->alive
                ? std::format("Age {} M{} D{}", sn->current_age, sn->birth_month, sn->birth_day)
                : std::format("+d.{}", sn->death_year);
            const std::string skill = (sn->sword_rank >= SkillRank::Intermediate)
                ? std::format("{} {}", sn->sword_style, toString(sn->sword_rank))
                : sn->title;
            lines.push_back(text(std::format("  |-- [{}] {} ({}, {})",
                                             full_sib ? "Sib" : "Half-Sib",
                                             tree.formatDisplayName(sn->id, false),
                                             life,
                                             skill)));
        }
    }

    return window(
        text(" [T] FAMILY TREE & DYNASTIC LINEAGE ") | bold | color(Color::RGB(255, 225, 130)),
        vbox(std::move(lines))
    );
}

Element renderInspectPanel(const sim::SimulationEngine& engine, const UIState& state) {
    const auto& reg = engine.registry();
    if (state.selected_settlement == entt::null || !reg.valid(state.selected_settlement)) {
        return window(text(" [I]NSPECT ") | bold, text("No settlement selected."));
    }

    const entt::entity s_ent = state.selected_settlement;
    const auto& s_name = reg.get<ecs::NameComponent>(s_ent).value;
    const auto& sett = reg.get<ecs::SettlementComponent>(s_ent);
    const auto& demo = reg.get<ecs::DemographicsComponent>(s_ent);
    const GridPos s_pos = reg.get<ecs::GridPosComponent>(s_ent).pos;
    const auto& s_cell = engine.map().at(s_pos);

    Elements lines;
    lines.push_back(hbox({
        text(s_name) | bold | color(Color::RGB(255, 230, 140)),
        text(std::format(" ({})", toString(sett.type))) | color(Color::RGB(185, 200, 215))
    }));
    lines.push_back(text(std::format("Prosperity: {:.0f}% | Banditry: {:.0f}%",
                                     sett.prosperity * 100.0, sett.bandit_threat * 100.0)) | dim);
    lines.push_back(text(std::format("Region: {} | Woods:{:.0f}% [{}]",
                                     s_cell.region_name,
                                     sett.local_woodland_cover * 100.0,
                                     toShortString(sett.forestry_policy)))
                    | color(Color::RGB(155, 225, 165)));
    lines.push_back(separatorLight());

    if (sett.ruler_entity != entt::null && reg.valid(sett.ruler_entity)) {
        const auto& r_name = reg.get<ecs::NameComponent>(sett.ruler_entity).value;
        const auto& ch = reg.get<ecs::CharacterComponent>(sett.ruler_entity);
        const auto& soc = reg.get<ecs::SocialStatusComponent>(sett.ruler_entity);
        const auto& mm = reg.get<ecs::MartialMagicComponent>(sett.ruler_entity);
        const auto& faith = reg.get<ecs::FaithComponent>(sett.ruler_entity);
        const auto& fam = reg.get<ecs::FamilyComponent>(sett.ruler_entity);

        const std::string ep_str = fam.epithet.empty() ? std::string() : (" '" + fam.epithet + "'");
        const std::string reg_badge = (sett.regent_entity != entt::null && reg.valid(sett.regent_entity))
            ? " [REGENCY]"
            : std::string();
        lines.push_back(text(std::format("{}: {}{}{}", soc.title, r_name, ep_str, reg_badge))
                        | bold | color(Color::RGB(220, 240, 255)));
        if (sett.regent_entity != entt::null && reg.valid(sett.regent_entity)) {
            lines.push_back(text(std::format("Regent: {}", reg.get<ecs::NameComponent>(sett.regent_entity).value))
                            | color(Color::RGB(255, 190, 110)));
        }
        lines.push_back(paragraph(std::format("Age: {} (M{} D{}, {}) | House: {} | {}",
                                              ch.age, ch.birth_month, ch.birth_day,
                                              monthName(ch.birth_month), soc.house, ch.trait)));
        lines.push_back(text(std::format("Faith: {} ({:.0f}% Zeal)", toShortString(faith.rite), faith.zeal * 100.0)));
        lines.push_back(text(std::format("Sword: {} ({})",
                                         mm.sword_rank == SkillRank::None ? std::string("None") : mm.sword_style,
                                         toString(mm.sword_rank))));
        lines.push_back(text(std::format("Magic: {} ({}) MP:{:.0f}{}",
                                         mm.magic_rank == SkillRank::None ? std::string("None") : mm.magic_school,
                                         toString(mm.magic_rank),
                                         mm.mana_capacity,
                                         mm.incantationless ? " [Silent]" : "")));

        auto officerGiven = [&](entt::entity oe) -> std::string {
            if (oe == entt::null || !reg.valid(oe) || !reg.all_of<ecs::NameComponent>(oe)) {
                return "-";
            }
            return sim::FamilyTree::extractGivenName(reg.get<ecs::NameComponent>(oe).value);
        };
        lines.push_back(text(std::format("Court: Sw:{} Mg:{} Ch:{}",
                                         officerGiven(sett.sword_instructor_entity),
                                         officerGiven(sett.court_mage_entity),
                                         officerGiven(sett.court_chaplain_entity)))
                        | color(Color::RGB(190, 225, 235)));
        if (!sett.patron_saint.empty() || !sett.active_heresy.empty()) {
            std::string rel_note;
            if (!sett.patron_saint.empty()) {
                rel_note += "Patron:" + sett.patron_saint;
            }
            if (!sett.active_heresy.empty()) {
                if (!rel_note.empty()) rel_note += " | ";
                rel_note += "Heresy:" + sett.active_heresy;
            }
            lines.push_back(text(rel_note)
                            | color(!sett.active_heresy.empty() ? Color::RGB(255, 130, 125) : Color::RGB(205, 185, 255)));
        }

        if (const sim::PersonNode* rnode = engine.familyTree().find(fam.person_id)) {
            const sim::PersonNode* fnode = engine.familyTree().find(rnode->father_id);
            const sim::PersonNode* mnode = engine.familyTree().find(rnode->mother_id);
            const std::string f_str = fnode ? fnode->given_name : std::string("-");
            const std::string m_str = mnode ? mnode->given_name : std::string("-");
            lines.push_back(text(std::format("Lineage: Gen {} | Parents: {}, {}",
                                             rnode->generation, f_str, m_str))
                            | color(Color::RGB(215, 200, 245)));
        }

        lines.push_back(separatorLight());
        lines.push_back(text("Household (ECS):") | bold | color(Color::RGB(255, 215, 145)));
        if (reg.all_of<ecs::HouseholdComponent>(sett.ruler_entity)) {
            const auto& hh = reg.get<ecs::HouseholdComponent>(sett.ruler_entity);
            int shown = 0;
            int total_living = 0;
            for (const entt::entity me : hh.member_entities) {
                if (!reg.valid(me) || !reg.all_of<ecs::CharacterComponent, ecs::FamilyComponent>(me)) {
                    continue;
                }
                const auto& m_ch = reg.get<ecs::CharacterComponent>(me);
                if (!m_ch.alive) continue;
                ++total_living;
                if (shown >= 3) continue;

                const auto& m_name = reg.get<ecs::NameComponent>(me).value;
                const auto& m_soc = reg.get<ecs::SocialStatusComponent>(me);
                const auto& m_fam = reg.get<ecs::FamilyComponent>(me);
                const auto& m_mm = reg.get<ecs::MartialMagicComponent>(me);

                std::string detail;
                if (m_fam.pregnancy_months > 0) {
                    detail = std::format("Preg {}m, Due D{}", m_fam.pregnancy_months, m_fam.pregnancy_due_day);
                } else if (m_mm.sword_rank >= SkillRank::Intermediate) {
                    detail = std::format("{} {}", m_mm.sword_style, toString(m_mm.sword_rank));
                } else if (m_mm.mana_capacity >= 65.0) {
                    detail = std::format("MP:{:.0f}", m_mm.mana_capacity);
                } else {
                    detail = m_fam.note.empty() ? m_ch.trait : m_fam.note;
                }
                lines.push_back(paragraph(std::format(" - {}: {} ({}, M{} D{}, {})",
                                                      m_soc.title, m_name, m_ch.age,
                                                      m_ch.birth_month, m_ch.birth_day, detail)));
                ++shown;
            }
            if (total_living > shown) {
                lines.push_back(text(std::format("   (+{} more kin/retainers)", total_living - shown)) | dim);
            }
        }
    }

    int library_count = 0;
    for (const entt::entity te : reg.view<ecs::LiteraryWorkComponent>()) {
        if (reg.get<ecs::LiteraryWorkComponent>(te).holder_settlement == s_ent) {
            ++library_count;
        }
    }

    lines.push_back(separatorLight());
    lines.push_back(text(std::format("Pop:{} Fed:{:.0f}% Warm:{:.0f}% Orth:{:.0f}% Lib:{}",
                                     demo.totalPopulation(),
                                     demo.weightedFedRatio() * 100.0,
                                     demo.cohorts.empty() ? 100.0 : demo.cohorts.front().warmth_ratio * 100.0,
                                     demo.weightedOrthodoxy() * 100.0,
                                     library_count)) | bold | color(Color::RGB(165, 230, 175)));

    std::string cohort_summary;
    for (std::size_t i = 0; i < demo.cohorts.size(); ++i) {
        if (i > 0) cohort_summary += "  ";
        cohort_summary += std::format("{}:{}", toString(demo.cohorts[i].type).substr(0, 4), demo.cohorts[i].population);
    }
    lines.push_back(text(cohort_summary));

    return window(
        text(" [I]NSPECT ") | bold | color(Color::RGB(150, 225, 255)),
        vbox(std::move(lines))
    );
}

Element renderHistoryPanel(const sim::SimulationEngine& engine, const UIState& state) {
    Elements lines;
    const auto entries = engine.historyLog().slice(
        5,
        static_cast<std::size_t>(std::max(0, state.log_scroll)),
        state.log_category_filter);

    if (entries.empty()) {
        lines.push_back(text(" > No chronicles match current filter.") | dim);
    } else {
        for (const auto& e : entries) {
            lines.push_back(paragraph(std::format(" > [{}, {} M{} D{}] [{}]: {}",
                                                  e.year, toString(e.season),
                                                  e.month, e.day,
                                                  toString(e.category), e.message))
                            | color(categoryColor(e.category)));
        }
    }

    const std::string title = state.log_scroll > 0
        ? std::format(" [H]ISTORY & WORLD LOG [Filter: {} | +{}] ",
                      logFilterLabel(state.log_category_filter), state.log_scroll)
        : std::format(" [H]ISTORY & WORLD LOG [Filter: {}] ",
                      logFilterLabel(state.log_category_filter));

    return window(
        text(title) | bold | color(Color::RGB(215, 230, 175)),
        vbox(std::move(lines))
    );
}

Element renderLedgerPanel(const sim::SimulationEngine& engine, const UIState& state) {
    const auto& reg = engine.registry();
    if (state.selected_settlement == entt::null || !reg.valid(state.selected_settlement)) {
        return window(text(" [L]OCAL LEDGER / LAWS ") | bold, text("No settlement."));
    }

    const entt::entity s_ent = state.selected_settlement;
    const auto& sett = reg.get<ecs::SettlementComponent>(s_ent);
    const auto& stock = reg.get<ecs::ResourceStockpileComponent>(s_ent);
    const auto& contract = reg.get<ecs::FeudalContractComponent>(s_ent);
    const std::size_t caravans = reg.view<ecs::CaravanComponent>().size();

    Elements lines;
    lines.push_back(text(std::format("Law: {}", sett.active_law)) | color(Color::RGB(255, 195, 130)));
    lines.push_back(text(std::format("Text: '{}'", sett.dominant_text)) | color(Color::RGB(195, 185, 245)));
    lines.push_back(text(std::format("Grain: {:.0f}t ({}/mo)  Fish: {:.0f}t ({}/mo)",
                                     stock.get(ResourceType::Grain), formatDelta(stock.delta(ResourceType::Grain)),
                                     stock.get(ResourceType::Fish), formatDelta(stock.delta(ResourceType::Fish)))));
    lines.push_back(text(std::format("Wood: {:.0f}t ({}/mo)   Timber: {:.0f}t ({}/mo)",
                                     stock.get(ResourceType::Firewood), formatDelta(stock.delta(ResourceType::Firewood)),
                                     stock.get(ResourceType::Timber), formatDelta(stock.delta(ResourceType::Timber)))));
    lines.push_back(text(std::format("Iron:{:.0f}t  Ag:{:.0f}  Crys:{:.0f}  Caravans:{}",
                                     stock.get(ResourceType::Iron),
                                     stock.get(ResourceType::Silver),
                                     stock.get(ResourceType::MagicCrystals),
                                     caravans)));
    lines.push_back(text(std::format("Corv:{:.0f}% Tithe:{:.0f}% | Levy:{} Sp, {} Kt, {} Mg",
                                     contract.corvee_rate * 100.0,
                                     contract.tithe_rate * 100.0,
                                     sett.levy_spearmen_ready,
                                     sett.knights_ready,
                                     sett.mages_ready)) | bold | color(Color::RGB(230, 225, 165)));
    const std::string bld_status = (sett.active_construction != BuildingType::None)
        ? std::format("{}({}m)", toShortString(sett.active_construction), sett.construction_months_left)
        : std::string("Idle");
    lines.push_back(text(std::format("Bld: G{} S{} A{} W{} Arms:{} | {}",
                                     sett.granary_level,
                                     sett.scriptorium_level,
                                     sett.armory_level,
                                     sett.ward_tower_level,
                                     sett.forged_arms_stock,
                                     bld_status)) | color(Color::RGB(165, 225, 210)));

    return window(
        text(" [L]OCAL LEDGER / LAWS ") | bold | color(Color::RGB(255, 210, 140)),
        vbox(std::move(lines))
    );
}

Element renderFooter() {
    return hbox({
        text(" [SPC]Play [.,;]Step [1-4]Spd [Arw/HJKL]Pan [Z]Cnt [TAB]Seat [M/N/T/R/C] [V]Lens [F]For [G]Log [E/B/U/O/S] [?]Help [Q]Quit")
            | color(Color::RGB(205, 215, 230))
    }) | bgcolor(Color::RGB(18, 24, 36));
}

Element buildRootElement(const sim::SimulationEngine& engine,
                         const UIState& state,
                         int term_w,
                         int term_h) {
    const int right_col_w = std::clamp(term_w * 40 / 100, 42, 56);
    const int left_col_w = std::max(30, term_w - right_col_w);
    const int top_row_h = std::max(14, term_h - 11);

    Element top_left;
    if (state.show_help) {
        top_left = renderHelpPanel(state);
    } else {
        switch (state.active_tab) {
            case ActiveTab::Map:
                top_left = renderMapPanel(engine, state, left_col_w - 4, top_row_h - 5);
                break;
            case ActiveTab::Nobility:
                top_left = renderNobilityTab(engine, state);
                break;
            case ActiveTab::Religion:
                top_left = renderReligionTab(engine);
                break;
            case ActiveTab::Chronicle:
                top_left = renderChronicleTab(engine);
                break;
            case ActiveTab::FamilyTree:
                top_left = renderFamilyTreeTab(engine, state);
                break;
        }
    }

    auto top_row = hbox({
        top_left | flex,
        renderInspectPanel(engine, state) | size(WIDTH, EQUAL, right_col_w)
    }) | flex;

    auto bottom_row = hbox({
        renderHistoryPanel(engine, state) | flex,
        renderLedgerPanel(engine, state) | size(WIDTH, EQUAL, right_col_w)
    }) | size(HEIGHT, EQUAL, 9);

    return vbox({
        renderHeader(engine, state),
        top_row,
        bottom_row,
        renderFooter()
    });
}

int stepDelayMs(int speed_level) noexcept {
    switch (speed_level) {
        case 1: return 450;
        case 2: return 160;
        case 3: return 45;
        case 4: return 35;
    }
    return 450;
}

} // namespace

UIState makeInitialUIState(const sim::SimulationEngine& engine) {
    UIState state;
    const auto settlements = engine.orderedSettlements();
    if (!settlements.empty()) {
        const entt::entity cap = engine.worldInfo().capital_entity != entt::null
            ? engine.worldInfo().capital_entity
            : settlements.front();
        state.selected_settlement = cap;
        const auto& pos = engine.registry().get<ecs::GridPosComponent>(cap).pos;
        state.cursor_x = pos.x;
        state.cursor_y = pos.y;
    }
    return state;
}

void selectNextSettlement(const sim::SimulationEngine& engine, UIState& state) {
    const auto list = engine.orderedSettlements();
    if (list.empty()) {
        return;
    }
    std::size_t idx = 0;
    for (std::size_t i = 0; i < list.size(); ++i) {
        if (list[i] == state.selected_settlement) {
            idx = (i + 1) % list.size();
            break;
        }
    }
    state.selected_settlement = list[idx];
    const auto& pos = engine.registry().get<ecs::GridPosComponent>(state.selected_settlement).pos;
    state.cursor_x = pos.x;
    state.cursor_y = pos.y;
}

void selectPrevSettlement(const sim::SimulationEngine& engine, UIState& state) {
    const auto list = engine.orderedSettlements();
    if (list.empty()) {
        return;
    }
    std::size_t idx = list.size() - 1;
    for (std::size_t i = 0; i < list.size(); ++i) {
        if (list[i] == state.selected_settlement) {
            idx = (i + list.size() - 1) % list.size();
            break;
        }
    }
    state.selected_settlement = list[idx];
    const auto& pos = engine.registry().get<ecs::GridPosComponent>(state.selected_settlement).pos;
    state.cursor_x = pos.x;
    state.cursor_y = pos.y;
}

void centerOnSelectedSettlement(const sim::SimulationEngine& engine, UIState& state) {
    const auto& reg = engine.registry();
    if (state.selected_settlement != entt::null && reg.valid(state.selected_settlement) &&
        reg.all_of<ecs::GridPosComponent>(state.selected_settlement)) {
        const auto& pos = reg.get<ecs::GridPosComponent>(state.selected_settlement).pos;
        state.cursor_x = pos.x;
        state.cursor_y = pos.y;
    }
}

void cycleMapLens(UIState& state) {
    const int next = (static_cast<int>(state.map_lens) + 1) % 5;
    state.map_lens = static_cast<MapLens>(next);
}

void cycleLogFilter(UIState& state) {
    if (state.log_category_filter < 0) {
        state.log_category_filter = 0;
    } else if (state.log_category_filter >= static_cast<int>(LogCategory::Ecology)) {
        state.log_category_filter = -1;
    } else {
        ++state.log_category_filter;
    }
    state.log_scroll = 0;
}

void moveCursor(const sim::SimulationEngine& engine, UIState& state, int dx, int dy) {
    const auto& map = engine.map();
    state.cursor_x = std::clamp(state.cursor_x + dx, 0, map.width() - 1);
    state.cursor_y = std::clamp(state.cursor_y + dy, 0, map.height() - 1);
    const auto& cell = map.at(state.cursor_x, state.cursor_y);
    if (cell.settlement_entity != entt::null && engine.registry().valid(cell.settlement_entity)) {
        state.selected_settlement = cell.settlement_entity;
    }
}

std::string renderSnapshotToString(const sim::SimulationEngine& engine,
                                   const UIState& state,
                                   int width,
                                   int height) {
    auto screen = Screen::Create(Dimension::Fixed(width), Dimension::Fixed(height));
    Render(screen, buildRootElement(engine, state, width, height));
    return screen.ToString();
}

void runInteractiveApp(sim::SimulationEngine& engine) {
    UIState state = makeInitialUIState(engine);
    auto screen = ScreenInteractive::Fullscreen();

    std::atomic<bool> running{true};
    auto last_tick = std::chrono::steady_clock::now();

    auto renderer = Renderer([&] {
        return buildRootElement(engine, state, screen.dimx(), screen.dimy());
    });

    auto root = CatchEvent(renderer, [&](Event event) {
        if (event == Event::Custom) {
            if (!state.paused) {
                const auto now = std::chrono::steady_clock::now();
                const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_tick).count();
                if (elapsed >= stepDelayMs(state.speed_level)) {
                    const int steps = (state.speed_level >= 4) ? 4 : 1;
                    for (int s = 0; s < steps; ++s) {
                        engine.tickDay();
                    }
                    last_tick = now;
                }
            }
            return true;
        }

        if (event == Event::Character('q') || event == Event::Character('Q') || event == Event::Escape) {
            running.store(false);
            screen.Exit();
            return true;
        }
        if (event == Event::Character('?')) {
            state.show_help = !state.show_help;
            return true;
        }
        if (event == Event::Character(' ')) {
            state.paused = !state.paused;
            last_tick = std::chrono::steady_clock::now();
            return true;
        }
        if (event == Event::Character('.') || event == Event::Character('>')) {
            engine.tickDay();
            state.status_toast = "+1 day";
            return true;
        }
        if (event == Event::Character(',') || event == Event::Character('<')) {
            engine.advanceDays(30);
            state.status_toast = "+30 days";
            return true;
        }
        if (event == Event::Character(';') || event == Event::Character(':')) {
            engine.advanceDays(90);
            state.status_toast = "+90d Season";
            return true;
        }
        if (event == Event::Character('1')) {
            state.speed_level = 1;
            state.paused = false;
            return true;
        }
        if (event == Event::Character('2')) {
            state.speed_level = 2;
            state.paused = false;
            return true;
        }
        if (event == Event::Character('3')) {
            state.speed_level = 3;
            state.paused = false;
            return true;
        }
        if (event == Event::Character('4')) {
            state.speed_level = 4;
            state.paused = false;
            return true;
        }
        if (event == Event::Character('m') || event == Event::Character('M')) {
            state.active_tab = ActiveTab::Map;
            state.show_help = false;
            return true;
        }
        if (event == Event::Character('n') || event == Event::Character('N')) {
            state.active_tab = ActiveTab::Nobility;
            state.show_help = false;
            return true;
        }
        if (event == Event::Character('t') || event == Event::Character('T')) {
            state.active_tab = ActiveTab::FamilyTree;
            state.show_help = false;
            return true;
        }
        if (event == Event::Character('r') || event == Event::Character('R')) {
            state.active_tab = ActiveTab::Religion;
            state.show_help = false;
            return true;
        }
        if (event == Event::Character('c') || event == Event::Character('C')) {
            state.active_tab = ActiveTab::Chronicle;
            state.show_help = false;
            return true;
        }
        if (event == Event::Character('v') || event == Event::Character('V')) {
            cycleMapLens(state);
            state.show_help = false;
            state.status_toast = std::format("Lens: {}", toString(state.map_lens));
            return true;
        }
        if (event == Event::Character('f') || event == Event::Character('F')) {
            const auto policy = engine.cycleForestryPolicyAt(state.selected_settlement);
            state.status_toast = std::format("Forestry: {}", toShortString(policy));
            return true;
        }
        if (event == Event::Character('g') || event == Event::Character('G')) {
            cycleLogFilter(state);
            state.status_toast = std::format("Log: {}", logFilterLabel(state.log_category_filter));
            return true;
        }
        if (event == Event::PageUp) {
            state.log_scroll = std::min(static_cast<int>(engine.historyLog().entries().size()), state.log_scroll + 5);
            return true;
        }
        if (event == Event::PageDown) {
            state.log_scroll = std::max(0, state.log_scroll - 5);
            return true;
        }
        if (event == Event::Character('z') || event == Event::Character('Z')) {
            centerOnSelectedSettlement(engine, state);
            state.status_toast = "Centered on seat";
            return true;
        }
        if (event == Event::Character('e') || event == Event::Character('E')) {
            engine.issueEdictAt(state.selected_settlement);
            state.status_toast = "Edict Issued";
            return true;
        }
        if (event == Event::Character('b') || event == Event::Character('B')) {
            engine.dispatchRetinueAt(state.selected_settlement);
            state.status_toast = "Banner Raised";
            return true;
        }
        if (event == Event::Character('u') || event == Event::Character('U')) {
            if (engine.commissionBuildingAt(state.selected_settlement)) {
                state.status_toast = "Construction Commissioned";
            } else {
                state.status_toast = "Build Queue Busy";
            }
            return true;
        }
        if (event == Event::Character('o') || event == Event::Character('O')) {
            if (engine.conductDiplomacyAt(state.selected_settlement)) {
                state.status_toast = "Diplomacy / Raid Dispatched";
            } else {
                state.status_toast = "Diplomacy Unavailable";
            }
            return true;
        }
        if (event == Event::Character('s')) {
            engine.convenePlayerSynod(true);
            state.status_toast = "Synod: North";
            return true;
        }
        if (event == Event::Character('S')) {
            engine.convenePlayerSynod(false);
            state.status_toast = "Synod: Canon";
            return true;
        }
        if (event == Event::Tab || event == Event::Character(']')) {
            selectNextSettlement(engine, state);
            return true;
        }
        if (event == Event::Character('[')) {
            selectPrevSettlement(engine, state);
            return true;
        }
        if (event == Event::ArrowLeft || event == Event::Character('a') || event == Event::Character('A')) {
            moveCursor(engine, state, -1, 0);
            return true;
        }
        if (event == Event::ArrowRight || event == Event::Character('d') || event == Event::Character('D')) {
            moveCursor(engine, state, 1, 0);
            return true;
        }
        if (event == Event::ArrowUp || event == Event::Character('w') || event == Event::Character('W')) {
            moveCursor(engine, state, 0, -1);
            return true;
        }
        if (event == Event::ArrowDown) {
            moveCursor(engine, state, 0, 1);
            return true;
        }
        if (event == Event::Character('h') || event == Event::Character('H')) {
            moveCursor(engine, state, -5, 0);
            return true;
        }
        if (event == Event::Character('j') || event == Event::Character('J')) {
            moveCursor(engine, state, 0, 5);
            return true;
        }
        if (event == Event::Character('k') || event == Event::Character('K')) {
            moveCursor(engine, state, 0, -5);
            return true;
        }
        if (event == Event::Character('l') || event == Event::Character('L')) {
            moveCursor(engine, state, 5, 0);
            return true;
        }
        return false;
    });

    std::thread ticker([&] {
        while (running.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            if (running.load() && !state.paused) {
                screen.Post(Event::Custom);
            }
        }
    });

    screen.Loop(root);
    running.store(false);
    if (ticker.joinable()) {
        ticker.join();
    }
}

} // namespace sovereignia::ui
