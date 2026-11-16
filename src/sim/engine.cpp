#include "sovereignia/sim/engine.hpp"

#include "sovereignia/core/paths.hpp"
#include "sovereignia/ecs/components.hpp"

#include <format>

namespace sovereignia::sim {

SimulationEngine::SimulationEngine() = default;

void SimulationEngine::loadData(const std::filesystem::path &data_dir) {
  config_.loadFromDirectory(data_dir);
  data_loaded_ = true;
}

void SimulationEngine::initializeWorld(std::uint64_t seed,
                                       int start_year,
                                       int map_width,
                                       int map_height) {
  if (!data_loaded_) {
    loadData(core::resolveDataDir());
  }

  registry_.clear();
  history_log_.clear();
  memorial_archive_.clear();
  ordered_settlements_.clear();
  family_tree_.clear();
  church_state_ = ecs::ChurchState{};
  ecology_state_ = ecs::EcologyState{};
  rng_.reseed(seed);
  clock_.reset(start_year);
  map_.resize(map_width, map_height);

  world_info_ = world::generateNorthernKingdom(map_, registry_, config_, rng_,
                                             family_tree_, start_year);
  ecology_.initializeMetrics(map_, config_, ecology_state_);
  dynasty_.refreshCourtOffices(registry_, clock_, history_log_);

  for (const auto &s_seed : config_.settlementSeeds()) {
    const auto it = world_info_.settlement_by_id.find(s_seed.id);
    if (it != world_info_.settlement_by_id.end()) {
      ordered_settlements_.push_back(it->second);
    }
  }

  history_log_.add(clock_.year(), clock_.season(), clock_.month(), clock_.day(),
                   LogCategory::Diplomacy,
                   std::format("Kingdom of Nordmark chartered under Margrave "
                               "Aleksandr Vaelen ({} settlements).",
                               ordered_settlements_.size()));
  history_log_.add(clock_.year(), clock_.season(), clock_.month(), clock_.day(),
                   LogCategory::Theology,
                   "Metropolitan Seraphim preserves Northern Autocephaly at "
                   "St. Cyril's Monastery.");
}

void SimulationEngine::runPreHistory(int years) {
  if (years <= 0) {
    return;
  }
  const int total_days = years * core::SimClock::kDaysPerYear;
  advanceDays(total_days);
}

core::TickFlags SimulationEngine::tickDay() {
  const core::TickFlags flags = clock_.advanceDay();

  economy_.tickDaily(registry_, map_, config_, clock_, history_log_);
  dynasty_.tickDaily(registry_, config_, clock_, rng_, family_tree_,
                     history_log_);
  governance_war_.tickDaily(registry_, map_, config_, world_info_, clock_, rng_,
                            history_log_);

  if (flags.monthly) {
    economy_.tickMonthly(registry_, map_, config_, world_info_, clock_,
                         history_log_);
    dynasty_.tickMonthly(registry_, config_, clock_, rng_, family_tree_,
                         history_log_);
  }
  if (flags.seasonal) {
    economy_.tickSeasonal(registry_, clock_, rng_, history_log_);
    ecology_.tickSeasonal(registry_, map_, config_, clock_, rng_, ecology_state_,
                          history_log_);
    governance_war_.tickSeasonal(registry_, map_, config_, world_info_, clock_,
                                 rng_, history_log_);
  }
  if (flags.yearly) {
    economy_.tickYearly(registry_, world_info_, clock_, history_log_);
    dynasty_.tickYearly(registry_, config_, world_info_, clock_, rng_,
                        family_tree_, memorial_archive_, history_log_);
    theology_.tickYearly(registry_, config_, world_info_, clock_, rng_,
                         church_state_, history_log_);
  }

  return flags;
}

bool SimulationEngine::issueEdictAt(entt::entity seat_settlement) {
  return governance_war_.issuePlayerEdict(registry_, config_, seat_settlement,
                                          clock_, history_log_);
}

bool SimulationEngine::dispatchRetinueAt(entt::entity seat_settlement) {
  return governance_war_.dispatchPlayerRetinue(registry_, map_, config_,
                                               world_info_, seat_settlement,
                                               clock_, history_log_);
}

bool SimulationEngine::commissionBuildingAt(entt::entity seat_settlement) {
  return economy_.startConstruction(registry_, map_, seat_settlement,
                                    BuildingType::None, clock_, history_log_, true);
}

bool SimulationEngine::conductDiplomacyAt(entt::entity seat_settlement) {
  return governance_war_.conductDiplomacy(registry_, map_, config_, world_info_,
                                          seat_settlement, clock_, rng_, history_log_);
}

void SimulationEngine::convenePlayerSynod(bool endorse_northern_rite) {
  theology_.conveneSynod(registry_, world_info_, clock_, church_state_,
                         endorse_northern_rite, history_log_);
}

ForestryPolicy SimulationEngine::cycleForestryPolicyAt(entt::entity seat_settlement) {
  if (seat_settlement == entt::null || !registry_.valid(seat_settlement) ||
      !registry_.all_of<ecs::SettlementComponent, ecs::NameComponent>(seat_settlement)) {
    return ForestryPolicy::BalancedCoppice;
  }
  auto &sett = registry_.get<ecs::SettlementComponent>(seat_settlement);
  const auto &s_name = registry_.get<ecs::NameComponent>(seat_settlement).value;
  ForestryPolicy next = ForestryPolicy::BalancedCoppice;
  switch (sett.forestry_policy) {
    case ForestryPolicy::BalancedCoppice:
      next = ForestryPolicy::IntensiveFelling;
      break;
    case ForestryPolicy::IntensiveFelling:
      next = ForestryPolicy::WardenReforestation;
      break;
    case ForestryPolicy::WardenReforestation:
      next = ForestryPolicy::BalancedCoppice;
      break;
  }
  sett.forestry_policy = next;
  history_log_.add(clock_.year(), clock_.season(), clock_.month(), clock_.day(),
                   LogCategory::Ecology,
                   std::format("{} adopted '{}' forestry charter.", s_name, toString(next)));
  return next;
}

void SimulationEngine::advanceDays(int days) {
  for (int i = 0; i < days; ++i) {
    tickDay();
  }
}

std::vector<entt::entity> SimulationEngine::orderedSettlements() const {
  return ordered_settlements_;
}

int SimulationEngine::totalKingdomPopulation() const {
  int total = 0;
  const auto view = registry_.view<ecs::DemographicsComponent>();
  for (const entt::entity e : view) {
    total += view.get<ecs::DemographicsComponent>(e).totalPopulation();
  }
  return total;
}

} // namespace sovereignia::sim
