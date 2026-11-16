#pragma once

#include "sovereignia/sim/engine.hpp"

#include <entt/entt.hpp>

#include <string>
#include <string_view>

namespace sovereignia::ui {

enum class ActiveTab {
    Map = 0,
    Nobility = 1,
    Religion = 2,
    Chronicle = 3,
    FamilyTree = 4
};

enum class MapLens {
    Terrain = 0,
    Forestry = 1,
    Elevation = 2,
    Mana = 3,
    Threat = 4
};

constexpr std::string_view toString(MapLens lens) noexcept {
    switch (lens) {
        case MapLens::Terrain: return "Terrain";
        case MapLens::Forestry: return "Forestry";
        case MapLens::Elevation: return "Elevation";
        case MapLens::Mana: return "Mana Ley";
        case MapLens::Threat: return "Threat";
    }
    return "Terrain";
}

struct UIState {
    ActiveTab active_tab = ActiveTab::Map;
    MapLens map_lens = MapLens::Terrain;
    int cursor_x = 0;
    int cursor_y = 0;
    entt::entity selected_settlement = entt::null;
    bool paused = true;
    int speed_level = 1; // 1 = 1x, 2 = 2x, 3 = 3x, 4 = 4x
    bool show_help = false;
    int log_scroll = 0;
    int log_category_filter = -1; // -1 = ALL
    std::string status_toast;
};

/// Initializes UI cursor and selection on the Capital settlement.
UIState makeInitialUIState(const sim::SimulationEngine& engine);

/// Moves UI selection to the next settlement in deterministic order.
void selectNextSettlement(const sim::SimulationEngine& engine, UIState& state);

/// Moves UI selection to the previous settlement in deterministic order.
void selectPrevSettlement(const sim::SimulationEngine& engine, UIState& state);

/// Recenters map cursor onto the currently selected settlement.
void centerOnSelectedSettlement(const sim::SimulationEngine& engine, UIState& state);

/// Cycles the active map lens (Terrain -> Forestry -> Elevation -> Mana -> Threat).
void cycleMapLens(UIState& state);

/// Cycles the chronicle history log category filter (-1 = ALL through Ecology).
void cycleLogFilter(UIState& state);

/// Moves map cursor safely within bounds and selects any settlement under the cursor.
void moveCursor(const sim::SimulationEngine& engine, UIState& state, int dx, int dy);

/// Renders a fixed-size UTF-8 terminal frame of the 4-panel FTXUI layout for headless snapshots & tests.
std::string renderSnapshotToString(const sim::SimulationEngine& engine,
                                   const UIState& state,
                                   int width = 122,
                                   int height = 38);

/// Launches the interactive full-screen FTXUI terminal session.
void runInteractiveApp(sim::SimulationEngine& engine);

} // namespace sovereignia::ui

