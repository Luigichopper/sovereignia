#include "sovereignia/sim/theology_system.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <span>
#include <string>
#include <vector>

namespace sovereignia::sim {

namespace {

constexpr std::array<std::string_view, 12> kNorthernSaints{
    "St. Seraphim the Steadfast",
    "St. Nikon of the Tarn",
    "St. Anna the Hospitaller",
    "St. Toma the Chronicler",
    "St. Hakon of the Frost",
    "St. Varvara of Saltmere",
    "St. Ignatiy the Builder",
    "St. Efrem the Silent",
    "St. Pavel of Westwatch",
    "St. Martha of Birchhollow",
    "St. Mikhail of Vaelgard",
    "St. Theodora of Mirren"
};

void capLiteraryWorks(entt::registry& registry) {
    std::vector<entt::entity> generated_works;
    auto view = registry.view<ecs::LiteraryWorkComponent>();
    for (const entt::entity te : view) {
        const auto& title = view.get<ecs::LiteraryWorkComponent>(te).title;
        if (title != "The Frost Canon" &&
            title != "Homilies of St. Cyril" &&
            title != "Treatise on the Uncreated Mana" &&
            title != "Chronicle of the First Margraves") {
            generated_works.push_back(te);
        }
    }
    if (generated_works.size() <= 28) {
        return;
    }
    std::sort(generated_works.begin(), generated_works.end(), [&](entt::entity a, entt::entity b) {
        return view.get<ecs::LiteraryWorkComponent>(a).year <
               view.get<ecs::LiteraryWorkComponent>(b).year;
    });
    const std::size_t remove_count = generated_works.size() - 28;
    for (std::size_t i = 0; i < remove_count; ++i) {
        registry.destroy(generated_works[i]);
    }
}

void canonizeSettlementSaint(entt::registry& registry,
                             entt::entity target_seat,
                             const std::string& saint_name,
                             ecs::ChurchState& church_state,
                             const core::SimClock& clock,
                             HistoryLog& log,
                             int synod_num = 0) {
    if (target_seat == entt::null || !registry.valid(target_seat) ||
        !registry.all_of<ecs::SettlementComponent, ecs::NameComponent>(target_seat)) {
        return;
    }
    auto& sett = registry.get<ecs::SettlementComponent>(target_seat);
    if (!sett.patron_saint.empty()) {
        return;
    }
    sett.patron_saint = saint_name;
    sett.is_pilgrimage_site = true;
    ++church_state.canonized_saints;
    const auto& seat_name = registry.get<ecs::NameComponent>(target_seat).value;
    if (synod_num > 0) {
        log.add(clock.year(), clock.season(), clock.month(), clock.day(),
                LogCategory::Theology,
                std::format("Synod #{} canonized {} as Patron Saint of {}!",
                            synod_num, saint_name, seat_name));
    } else {
        log.add(clock.year(), clock.season(), clock.month(), clock.day(),
                LogCategory::Theology,
                std::format("The Northern Church canonized {} as Patron Saint of {}!",
                            saint_name, seat_name));
    }
}

} // namespace

void TheologySystem::conveneSynod(entt::registry& registry,
                                  const world::GeneratedWorldInfo& world_info,
                                  const core::SimClock& clock,
                                  ecs::ChurchState& church_state,
                                  bool endorse_northern_rite,
                                  HistoryLog& log) {
    ++church_state.synods_held;

    if (endorse_northern_rite) {
        church_state.autocephaly = std::min(1.0, church_state.autocephaly + 0.05);
        church_state.schism_tension = std::min(1.0, church_state.schism_tension + 0.06);
        if (world_info.empire_entity != entt::null && registry.valid(world_info.empire_entity)) {
            auto& emp = registry.get<ecs::OffMapEmpireComponent>(world_info.empire_entity);
            emp.favor = std::max(0.05, emp.favor - 0.04);
            emp.relations = std::max(-100, emp.relations - 4);
        }
        for (const entt::entity se : registry.view<ecs::DemographicsComponent>()) {
            for (auto& c : registry.get<ecs::DemographicsComponent>(se).cohorts) {
                c.unrest = std::max(0.01, c.unrest - 0.04);
                c.orthodoxy = std::min(1.0, c.orthodoxy + 0.02);
            }
        }
        log.add(clock.year(), clock.season(), clock.month(), clock.day(),
                LogCategory::Theology,
                std::format("Synod #{} at St. Cyril's endorsed '{}' under Northern Rite.",
                            church_state.synods_held, church_state.active_controversy));

        if (church_state.canonized_saints < 12) {
            entt::entity saint_seat = entt::null;
            if (world_info.monastery_entity != entt::null && registry.valid(world_info.monastery_entity) &&
                registry.get<ecs::SettlementComponent>(world_info.monastery_entity).patron_saint.empty()) {
                saint_seat = world_info.monastery_entity;
            } else {
                for (const entt::entity se : registry.view<ecs::SettlementComponent>()) {
                    if (registry.get<ecs::SettlementComponent>(se).patron_saint.empty()) {
                        saint_seat = se;
                        break;
                    }
                }
            }
            if (saint_seat != entt::null) {
                const std::string s_name(kNorthernSaints[static_cast<std::size_t>(church_state.canonized_saints) % kNorthernSaints.size()]);
                canonizeSettlementSaint(registry, saint_seat, s_name, church_state, clock, log, church_state.synods_held);
            }
        }
    } else {
        church_state.autocephaly = std::max(0.15, church_state.autocephaly - 0.03);
        church_state.schism_tension = std::max(0.02, church_state.schism_tension - 0.06);
        if (world_info.empire_entity != entt::null && registry.valid(world_info.empire_entity)) {
            auto& emp = registry.get<ecs::OffMapEmpireComponent>(world_info.empire_entity);
            emp.favor = std::min(1.0, emp.favor + 0.05);
            emp.relations = std::min(100, emp.relations + 4);
        }
        for (const entt::entity se : registry.view<ecs::DemographicsComponent>()) {
            for (auto& c : registry.get<ecs::DemographicsComponent>(se).cohorts) {
                c.unrest = std::min(1.0, c.unrest + 0.03);
            }
        }
        log.add(clock.year(), clock.season(), clock.month(), clock.day(),
                LogCategory::Theology,
                std::format("Synod #{} deferred to Heliodor's Imperial Orthodox Canon on '{}'.",
                            church_state.synods_held, church_state.active_controversy));
    }

    if (church_state.schism_tension >= 0.85 && !church_state.in_formal_schism) {
        church_state.in_formal_schism = true;
        if (world_info.monastery_entity != entt::null && registry.valid(world_info.monastery_entity)) {
            const entt::entity metro = registry.get<ecs::SettlementComponent>(world_info.monastery_entity).ruler_entity;
            if (metro != entt::null && registry.valid(metro)) {
                registry.get<ecs::SocialStatusComponent>(metro).title = "Patriarch of the North";
            }
        }
        log.add(clock.year(), clock.season(), clock.month(), clock.day(),
                LogCategory::Eponym,
                std::format("The Great Northern Schism of {}! St. Cyril's proclaimed full Patriarchal Autocephaly.",
                            clock.year()));
    }
}

void TheologySystem::tickYearly(entt::registry& registry,
                                const data::ConfigRegistry& config,
                                const world::GeneratedWorldInfo& world_info,
                                const core::SimClock& clock,
                                core::Rng& rng,
                                ecs::ChurchState& church_state,
                                HistoryLog& log) {
    // 1. Literature, Treatises, Icons, and Epic Poems produced every ~2 years
    if ((clock.year() % 2 == 0 || rng.nextDouble() < 0.35) && !config.treatiseTemplates().empty()) {
        std::vector<entt::entity> seats;
        for (const entt::entity se : registry.view<ecs::SettlementComponent>()) {
            seats.push_back(se);
        }
        const entt::entity holder = seats.empty()
            ? world_info.monastery_entity
            : (rng.nextDouble() < 0.55 && world_info.monastery_entity != entt::null
                ? world_info.monastery_entity
                : rng.pick(std::span<const entt::entity>(seats)));

        const std::string holder_name = (holder != entt::null && registry.valid(holder))
            ? registry.get<ecs::NameComponent>(holder).value
            : std::string("St. Cyril's Monastery");

        static constexpr std::array<std::string_view, 4> kKinds{"TheologicalTreatise", "Icon", "LiturgicalCanon", "EpicPoem"};
        static constexpr std::array<std::string_view, 5> kTraits{"Ascetic", "Charitable", "Zealous", "Pragmatic", "Hardy"};

        const std::string& base_title = rng.pick(std::span<const std::string>(config.treatiseTemplates()));
        const std::string kind = std::string(rng.pick(std::span<const std::string_view>(kKinds)));
        const std::string clergy_first = config.clergyNames().empty()
            ? std::string("Toma")
            : rng.pick(std::span<const std::string>(config.clergyNames()));
        const std::string female_first = config.femaleNames().empty()
            ? std::string("Martha")
            : rng.pick(std::span<const std::string>(config.femaleNames()));
        const std::string male_first = config.maleNames().empty()
            ? std::string("Halvar")
            : rng.pick(std::span<const std::string>(config.maleNames()));
        const std::string house_name = config.houses().empty()
            ? std::string("Vaelen")
            : rng.pick(std::span<const std::string>(config.houses()));

        std::string author;
        if (kind == "EpicPoem") {
            author = "Skald " + male_first + " " + house_name;
        } else if (kind == "Icon") {
            author = (rng.nextDouble() < 0.5) ? ("Sister " + female_first) : ("Brother " + clergy_first);
        } else if (rng.nextDouble() < 0.35) {
            author = "Scholar " + female_first + " " + house_name;
        } else {
            author = "Abbot " + clergy_first + " " + house_name;
        }

        const std::string trait = std::string(rng.pick(std::span<const std::string_view>(kTraits)));
        const bool controversial = (rng.nextDouble() < 0.35);

        std::string full_title = (kind == "Icon")
            ? std::format("Icon of the Northern Watch ({})", clock.year())
            : (kind == "EpicPoem"
                ? std::format("Lay of {} ({})", house_name, clock.year())
                : base_title);

        if (kind != "Icon" && kind != "EpicPoem") {
            bool duplicate_title = false;
            for (const entt::entity existing : registry.view<ecs::LiteraryWorkComponent>()) {
                if (registry.get<ecs::LiteraryWorkComponent>(existing).title == full_title) {
                    duplicate_title = true;
                    break;
                }
            }
            if (duplicate_title) {
                full_title = std::format("{} ({})", base_title, clock.year());
            }
        }

        const entt::entity te = registry.create();
        registry.emplace<ecs::NameComponent>(te, full_title);
        registry.emplace<ecs::LiteraryWorkComponent>(te, ecs::LiteraryWorkComponent{
            full_title,
            author,
            kind,
            clock.year(),
            holder_name,
            controversial
                ? "Sparks debate between Hesychast mystics and Imperial canonists."
                : "Preserved in local library for noble tutoring and liturgy.",
            holder,
            trait,
            controversial,
            true
        });

        if (holder != entt::null && registry.valid(holder)) {
            registry.get<ecs::SettlementComponent>(holder).dominant_text = full_title;
        }
        if (controversial) {
            church_state.active_controversy = full_title;
        }

        log.add(clock.year(), clock.season(), clock.month(), clock.day(),
                LogCategory::Theology,
                std::format("{} created {} '{}' at {}.", author, kind, full_title, holder_name));

        capLiteraryWorks(registry);
    }

    // 2. Periodic Northern Synod every 4 years
    if (clock.year() % 4 == 0) {
        double zeal = 0.75;
        if (world_info.monastery_entity != entt::null && registry.valid(world_info.monastery_entity)) {
            const entt::entity metro = registry.get<ecs::SettlementComponent>(world_info.monastery_entity).ruler_entity;
            if (metro != entt::null && registry.valid(metro) && registry.all_of<ecs::FaithComponent>(metro)) {
                zeal = registry.get<ecs::FaithComponent>(metro).zeal;
            }
        }
        double emp_favor = 0.55;
        if (world_info.empire_entity != entt::null && registry.valid(world_info.empire_entity)) {
            emp_favor = registry.get<ecs::OffMapEmpireComponent>(world_info.empire_entity).favor;
        }
        const double north_score = zeal * 0.65 + church_state.autocephaly * 0.45 + rng.nextRange(-0.18, 0.28);
        const double imp_score = emp_favor * 0.65 + 0.28;
        const bool endorse_north = north_score >= imp_score;
        conveneSynod(registry, world_info, clock, church_state, endorse_north, log);
    }

    // 3. Manuscript Copying & Physical Library Circulation
    std::vector<entt::entity> all_seats;
    for (const entt::entity se : registry.view<ecs::SettlementComponent>()) {
        all_seats.push_back(se);
    }
    if (all_seats.size() >= 2) {
        std::vector<entt::entity> scriptoria;
        for (const entt::entity se : all_seats) {
            const auto& sett = registry.get<ecs::SettlementComponent>(se);
            if (sett.scriptorium_level >= 1 || sett.type == SettlementType::Bishopric) {
                scriptoria.push_back(se);
            }
        }
        if (!scriptoria.empty()) {
            const entt::entity src_ent = rng.pick(std::span<const entt::entity>(scriptoria));
            const auto& src_name = registry.get<ecs::NameComponent>(src_ent).value;
            ecs::LiteraryWorkComponent chosen_copy{};
            bool found_work = false;
            for (const entt::entity te : registry.view<ecs::LiteraryWorkComponent>()) {
                const auto& tw = registry.get<ecs::LiteraryWorkComponent>(te);
                if (tw.holder_settlement == src_ent) {
                    chosen_copy = tw;
                    found_work = true;
                    break;
                }
            }
            if (found_work) {
                entt::entity dest_ent = entt::null;
                for (const entt::entity cand : all_seats) {
                    if (cand == src_ent) continue;
                    bool already_has = false;
                    for (const entt::entity te : registry.view<ecs::LiteraryWorkComponent>()) {
                        const auto& tw = registry.get<ecs::LiteraryWorkComponent>(te);
                        if (tw.holder_settlement == cand && tw.title == chosen_copy.title) {
                            already_has = true;
                            break;
                        }
                    }
                    if (!already_has) {
                        dest_ent = cand;
                        break;
                    }
                }
                if (dest_ent != entt::null) {
                    const auto& dest_name = registry.get<ecs::NameComponent>(dest_ent).value;
                    const entt::entity copy_ent = registry.create();
                    registry.emplace<ecs::NameComponent>(copy_ent, chosen_copy.title);
                    chosen_copy.holder_settlement = dest_ent;
                    chosen_copy.location = dest_name;
                    registry.emplace<ecs::LiteraryWorkComponent>(copy_ent, chosen_copy);
                    registry.get<ecs::SettlementComponent>(dest_ent).dominant_text = chosen_copy.title;
                    if (registry.all_of<ecs::DemographicsComponent>(dest_ent)) {
                        for (auto& c : registry.get<ecs::DemographicsComponent>(dest_ent).cohorts) {
                            c.orthodoxy = std::min(1.0, c.orthodoxy + 0.02);
                        }
                    }
                    ++church_state.texts_circulated;
                    log.add(clock.year(), clock.season(), clock.month(), clock.day(),
                            LogCategory::Theology,
                            std::format("Scriptorium at {} copied '{}' for {}'s library.",
                                        src_name, chosen_copy.title, dest_name));
                    capLiteraryWorks(registry);
                }
            }
        }
    }

    // 4. Grassroots Heresy Outbreak, Contagion & Pastoral Reconciliation
    static constexpr std::array<std::string_view, 4> kHeresyNames{
        "Frost-Iconoclast Sect",
        "Uncreated Light Brethren",
        "Aurelian Canonist Dissidents",
        "Ash-Penance Zealots"
    };
    struct SpreadEvent {
        entt::entity target = entt::null;
        std::string heresy;
    };
    std::vector<SpreadEvent> pending_spreads;

    for (const entt::entity se : all_seats) {
        auto& sett = registry.get<ecs::SettlementComponent>(se);
        if (!registry.all_of<ecs::DemographicsComponent, ecs::NameComponent>(se)) {
            continue;
        }
        const auto& s_name = registry.get<ecs::NameComponent>(se).value;
        auto& demo = registry.get<ecs::DemographicsComponent>(se);
        const double orth = demo.weightedOrthodoxy();
        const double unrest = demo.weightedUnrest();

        if (!sett.active_heresy.empty()) {
            const bool has_pastoral = (sett.type == SettlementType::Bishopric ||
                                       sett.scriptorium_level >= 1 ||
                                       sett.court_chaplain_entity != entt::null);
            if (has_pastoral || orth >= 0.84) {
                for (auto& c : demo.cohorts) {
                    c.orthodoxy = std::min(1.0, c.orthodoxy + 0.09);
                }
                if (demo.weightedOrthodoxy() >= 0.82 || rng.nextDouble() < 0.45) {
                    const std::string resolved = sett.active_heresy;
                    sett.active_heresy.clear();
                    log.add(clock.year(), clock.season(), clock.month(), clock.day(),
                            LogCategory::Theology,
                            std::format("Pastoral mission reconciled the {} in {}.", resolved, s_name));
                }
            } else {
                for (auto& c : demo.cohorts) {
                    c.orthodoxy = std::max(0.15, c.orthodoxy - 0.03);
                }
                if (rng.nextDouble() < 0.20 && registry.all_of<ecs::GridPosComponent>(se)) {
                    const GridPos p_src = registry.get<ecs::GridPosComponent>(se).pos;
                    entt::entity nearest = entt::null;
                    double best_dist = 1e9;
                    for (const entt::entity other : all_seats) {
                        if (other == se || !registry.all_of<ecs::GridPosComponent, ecs::DemographicsComponent>(other)) {
                            continue;
                        }
                        const auto& o_sett = registry.get<ecs::SettlementComponent>(other);
                        if (!o_sett.active_heresy.empty() ||
                            registry.get<ecs::DemographicsComponent>(other).weightedOrthodoxy() >= 0.85) {
                            continue;
                        }
                        const GridPos p_o = registry.get<ecs::GridPosComponent>(other).pos;
                        const double d = std::hypot(static_cast<double>(p_o.x - p_src.x),
                                                    static_cast<double>(p_o.y - p_src.y));
                        if (d < best_dist) {
                            best_dist = d;
                            nearest = other;
                        }
                    }
                    if (nearest != entt::null) {
                        pending_spreads.push_back(SpreadEvent{nearest, sett.active_heresy});
                    }
                }
            }
        } else {
            if (orth < 0.78 || (church_state.schism_tension > 0.45 && unrest > 0.18 && rng.nextDouble() < 0.12)) {
                sett.active_heresy = std::string(rng.pick(std::span<const std::string_view>(kHeresyNames)));
                for (auto& c : demo.cohorts) {
                    c.orthodoxy = std::max(0.15, c.orthodoxy - 0.06);
                }
                log.add(clock.year(), clock.season(), clock.month(), clock.day(),
                        LogCategory::Theology,
                        std::format("Grassroots heresy '{}' erupted in {}!", sett.active_heresy, s_name));
            }
        }
    }

    for (const auto& sp : pending_spreads) {
        if (!registry.valid(sp.target)) continue;
        auto& o_sett = registry.get<ecs::SettlementComponent>(sp.target);
        if (!o_sett.active_heresy.empty()) continue;
        o_sett.active_heresy = sp.heresy;
        for (auto& c : registry.get<ecs::DemographicsComponent>(sp.target).cohorts) {
            c.orthodoxy = std::max(0.15, c.orthodoxy - 0.04);
        }
        const auto& o_name = registry.get<ecs::NameComponent>(sp.target).value;
        log.add(clock.year(), clock.season(), clock.month(), clock.day(),
                LogCategory::Theology,
                std::format("Heresy '{}' spread along the roads to {}!", sp.heresy, o_name));
    }

    int heresy_total = 0;
    for (const entt::entity se : all_seats) {
        if (!registry.get<ecs::SettlementComponent>(se).active_heresy.empty()) {
            ++heresy_total;
        }
    }
    church_state.active_heresies_count = heresy_total;

    // 5. Periodic Northern Saint Canonization every 6 years
    if ((clock.year() % 6 == 0) && church_state.canonized_saints < 12) {
        std::vector<entt::entity> uncrowned;
        for (const entt::entity se : all_seats) {
            if (registry.get<ecs::SettlementComponent>(se).patron_saint.empty()) {
                uncrowned.push_back(se);
            }
        }
        if (!uncrowned.empty()) {
            const entt::entity chosen = rng.pick(std::span<const entt::entity>(uncrowned));
            const std::string base_name = config.clergyNames().empty()
                ? std::string("Nikon")
                : rng.pick(std::span<const std::string>(config.clergyNames()));
            static constexpr std::array<std::string_view, 6> kSaintEpithets{
                "the Vigilant", "of the Snows", "the Confessor", "the Merciful", "the Anchorite", "of the Ley"
            };
            const std::string saint_label = std::format("St. {} {}",
                                                        base_name,
                                                        rng.pick(std::span<const std::string_view>(kSaintEpithets)));
            canonizeSettlementSaint(registry, chosen, saint_label, church_state, clock, log, 0);
        }
    }
}

} // namespace sovereignia::sim
