# ソヴレィニア — Sovereignia: 布告と典礼の王国

A data-oriented C++20 & FTXUI terminal grand-strategy kingdom simulation built around a Northern feudal-theological kingdom (*Kingdom of Nordmark*), dual-scale demographics, seasonal survival economics, slow woodland ecology, dynastic intrigue, martial/magical disciplines, and multi-century emergent history.

## Implemented Features

- **Core Architecture**:
  - **EnTT** ECS (`SettlementComponent`, `ResourceStockpileComponent`, `DemographicsComponent`, `FeudalContractComponent`, `CharacterComponent`, `SocialStatusComponent`, `MartialMagicComponent`, `FaithComponent`, `FamilyComponent`, `HouseholdComponent`, `CaravanComponent`, `ArmyComponent`, `OffMapEmpireComponent`, `ActiveLawComponent`, `LiteraryWorkComponent`).
  - **FTXUI** multi-panel terminal interface (`[M]AP`, `[I]NSPECT`, `[H]ISTORY & WORLD LOG`, `[L]OCAL LEDGER / LAWS`, plus `[N]obility`, `[T] Family Tree`, `[R]eligion`, `[C]hronicle`, and `[?] Help & Legend` views).
  - **JSON Data-Driven Configs** in [`data/`](data/): 15 northern biomes, 12 settlements, 16 noble houses, 150+ first/clergy names, 32 cognomen epithets, *Mushoku Tensei*-inspired sword/magic tiers, Northern Autocephalous vs. Imperial Orthodox doctrines, and eponymous laws/woodland charters.
  - **4-Frequency Simulation Clock** (`Daily`, `Monthly`, `Seasonal`, `Yearly`) plus fast **200–500 Year Pre-Gen History**.
- **Expanded Geomorphological Map & Landmarks (`108x48` Default)**:
  - Elevation, rain-shadow moisture, soil fertility, temperature gradients, indented Eastern Fjord Shallows (`-`), 2 Glacial Lakes (*Lake Mirren* & *Cyril's Tarn*), multi-tributary widening rivers with stone/timber **bridges**, navigable mountain passes (*South Pass Gorge* & *Westmarch Pass*), named regions, and cached weighted **A*** routes across 12 settlements (`Vaelgard`, `Kaldheim`, `Fort Vael`, `St. Cyril's Monastery`, `Oakhaven`, `Saltmere`, `South Pass`, `Frostholm`, `Rotherfield`, `Mirrenstead`, `Westwatch Keep`, `Birchhollow`).
  - Rich map sites: **Ancient Ruins** (`%`), **Historic Battlefields** (`x`), **Monastic Hermitages** (`h`), **Border Watchtowers** (`I`), **Ley-Line Mana Wells** (`*`), and **Sacred Old-Growth Groves** (`Y`).
- **Slow Woodland Deforestation & Reforestation Ecology (`EcologySystem`)**:
  - Tracks per-cell continuous `forest_cover` (`0..100%`) and `soil_fertility`, with seasonal lumber/charcoal clearing around timber and mining camps (`Balanced Coppice`, `Intensive Felling`, `Warden Reforestation`), natural seed-dispersed succession over decades, hysteresis biome transitions (`Cleared Stump-Meadow "` -> `Regrowing Birch Coppice t` -> `Boreal Pinewood f` -> `Primeval Canopy Taiga F`), protected Sacred Groves, and timber yield feedback.
- **Onomastics, Dynasties, Warfare & Northern Theology**:
  - Persistent multi-generational `FamilyTree` tracking parent/spouse/concubine/sibling/successor links across centuries even after deceased ECS entities are cleaned up, with Slavic/Nordic patronymics (`-ovich`/`-ovna`, `-sson`/`-sdottir`), Roman regnal numerals, ancestral child naming, distinct household sibling naming, cognomen epithets (`the Silent`, `the Lawgiver`, `the Frost-Hand`), bastard cognomens, and cadet branch house formation.
  - Childhood mana growth (`ages 3–10`), Sword/Magic ranks (`Beginner -> Emperor`), rare **Incantationless Magic**, farmer-levy armies resolving field interceptions and sieges, manuscript looting, 4-year Synods, and Great Northern Schism dynamics.

## Building (Windows / Visual Studio 2022)

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

## Running

```powershell
# Interactive FTXUI terminal UI (with 200-year pre-history on the 108x48 map)
.\build\Release\sovereignia.exe --seed 482 --pregen 200

# Headless 200-year pre-history + Family Tree output + rendered [T] Family Tree snapshot
.\build\Release\sovereignia.exe --seed 482 --pregen 200 --headless --tree --render-snapshot

# Custom grid dimensions (--map-size WxH)
.\build\Release\sovereignia.exe --seed 482 --pregen 200 --map-size 120x52
```

### Interactive Controls

- `SPACE`: Pause / Play
- `.` / `,` / `;`: Advance `+1 Day` / `+30 Days` / `+90 Days` (1 Season)
- `1` / `2` / `3` / `4`: Set simulation speed (`1x` / `2x` / `3x` / `4x`)
- `Arrow Keys` / `W,A,D` (`1` tile) & `H,J,K,L` (`5`-tile fast pan): Pan cursor across the map
- `Z`: Center map cursor on selected settlement
- `TAB` / `]` and `[`: Cycle Next / Previous settlement
- `V`: Cycle Map Lens (`Terrain` -> `Forestry` -> `Elevation` -> `Mana Ley` -> `Threat`)
- `F`: Cycle Forestry Policy on selected seat (`Balanced Coppice` -> `Intensive Felling` -> `Warden Reforestation`)
- `G` and `PgUp` / `PgDn`: Filter and scroll the Kingdom Chronicle log
- `M` / `N` / `T` / `R` / `C`: Switch left pane (`Map`, `Nobility & Armies`, `Family Tree & Lineage`, `Religion & Texts`, `Chronicle & Ecology`)
- `E` / `B` / `s` / `S`: Issue Royal Edict / Mobilize Levy Banner / Convene Synod (`North` / `Canon`)
- `?`: Toggle full Help, Hotkeys & Glyph Legend overlay
- `Q` / `ESC`: Quit
