#include "sovereignia/sim/dynasty_system.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <string>
#include <unordered_set>
#include <vector>

namespace sovereignia::sim {

namespace {

std::string pickFirstName(const data::ConfigRegistry& config, core::Rng& rng, bool female) {
    const auto& pool = female ? config.femaleNames() : config.maleNames();
    if (pool.empty()) {
        return female ? "Elena" : "Lukas";
    }
    return rng.pick(std::span<const std::string>(pool));
}

std::string pickClergyName(const data::ConfigRegistry& config, core::Rng& rng) {
    const auto& pool = config.clergyNames().empty() ? config.maleNames() : config.clergyNames();
    if (pool.empty()) {
        return "Seraphim";
    }
    return rng.pick(std::span<const std::string>(pool));
}

std::string pickDistinctFirstName(const data::ConfigRegistry& config,
                                  core::Rng& rng,
                                  bool female,
                                  const std::unordered_set<std::string>& used_names,
                                  const FamilyTree* family_tree = nullptr,
                                  PersonId parent_id = kInvalidPersonId,
                                  bool allow_ancestral = false) {
    if (allow_ancestral && family_tree != nullptr && parent_id != kInvalidPersonId && rng.nextDouble() < 0.22) {
        if (const PersonNode* parent = family_tree->find(parent_id)) {
            const std::array<PersonId, 2> gp_ids{parent->father_id, parent->mother_id};
            for (const PersonId gpid : gp_ids) {
                if (const PersonNode* gp = family_tree->find(gpid)) {
                    if (gp->is_female == female && !gp->given_name.empty() && !used_names.contains(gp->given_name)) {
                        return gp->given_name;
                    }
                }
            }
        }
    }

    const auto& pool = female ? config.femaleNames() : config.maleNames();
    if (pool.empty()) {
        return female ? "Elena" : "Lukas";
    }

    for (int tries = 0; tries < 16; ++tries) {
        const auto& cand = rng.pick(std::span<const std::string>(pool));
        if (!used_names.contains(cand)) {
            return cand;
        }
    }
    for (const auto& cand : pool) {
        if (!used_names.contains(cand)) {
            return cand;
        }
    }
    return rng.pick(std::span<const std::string>(pool));
}

std::string pickTrait(const data::ConfigRegistry& config, core::Rng& rng) {
    if (config.traits().empty()) {
        return "Pragmatic";
    }
    return rng.pick(std::span<const std::string>(config.traits()));
}

std::string pickOtherHouse(const data::ConfigRegistry& config, core::Rng& rng, std::string_view exclude_house) {
    const auto& houses = config.houses();
    if (houses.empty()) {
        return "Vaelen";
    }
    for (int tries = 0; tries < 8; ++tries) {
        const auto& h = rng.pick(std::span<const std::string>(houses));
        if (h != exclude_house) {
            return h;
        }
    }
    return houses.front();
}

SkillRank nextSkillRank(SkillRank r) noexcept {
    const int v = static_cast<int>(r);
    if (v >= static_cast<int>(SkillRank::Emperor)) {
        return SkillRank::Emperor;
    }
    return static_cast<SkillRank>(v + 1);
}

double promotionProbability(SkillRank current, bool has_tutor) noexcept {
    double base = 0.0;
    switch (current) {
        case SkillRank::None: base = 0.16; break;
        case SkillRank::Beginner: base = 0.22; break;
        case SkillRank::Intermediate: base = 0.12; break;
        case SkillRank::Advanced: base = 0.055; break;
        case SkillRank::Saint: base = 0.018; break;
        case SkillRank::King: base = 0.005; break;
        case SkillRank::Emperor: base = 0.0; break;
    }
    return has_tutor ? base * 1.65 : base;
}

void pushMemorial(std::vector<ecs::MemorialEntry>& archive, ecs::MemorialEntry entry) {
    if (archive.size() >= 200) {
        archive.erase(archive.begin());
    }
    archive.push_back(std::move(entry));
}

void capTheoremEntities(entt::registry& registry) {
    std::vector<entt::entity> non_charters;
    auto view = registry.view<ecs::ActiveLawComponent>();
    for (const entt::entity le : view) {
        if (view.get<ecs::ActiveLawComponent>(le).kind != "charter") {
            non_charters.push_back(le);
        }
    }
    if (non_charters.size() <= 24) {
        return;
    }
    std::sort(non_charters.begin(), non_charters.end(), [&](entt::entity a, entt::entity b) {
        return view.get<ecs::ActiveLawComponent>(a).enacted_year <
               view.get<ecs::ActiveLawComponent>(b).enacted_year;
    });
    const std::size_t to_remove = non_charters.size() - 24;
    for (std::size_t i = 0; i < to_remove; ++i) {
        registry.destroy(non_charters[i]);
    }
}

entt::entity createNobleMember(entt::registry& registry,
                               std::string name,
                               int age,
                               int birth_month,
                               int birth_day,
                               bool is_female,
                               std::string trait,
                               entt::entity seat,
                               std::string title,
                               std::string house,
                               bool is_clergy,
                               DynastyRole role,
                               entt::entity head_entity,
                               entt::entity mother_entity,
                               entt::entity father_entity,
                               bool is_bastard,
                               double ambition,
                               int backing,
                               RiteType rite,
                               double mana_capacity,
                               std::string sword_style,
                               SkillRank sword_rank,
                               std::string magic_school,
                               SkillRank magic_rank,
                               std::string note,
                               PersonId person_id,
                               int regnal_number,
                               std::string given_name,
                               std::string patronymic) {
    const entt::entity e = registry.create();
    registry.emplace<ecs::NameComponent>(e, std::move(name));
    registry.emplace<ecs::CharacterComponent>(e, ecs::CharacterComponent{
        age,
        is_female,
        true,
        std::move(trait),
        seat,
        birth_month,
        birth_day
    });
    registry.emplace<ecs::SocialStatusComponent>(e, ecs::SocialStatusComponent{
        std::move(title),
        std::move(house),
        is_clergy,
        role == DynastyRole::HeadOfHouse ? 65 : 32
    });
    registry.emplace<ecs::MartialMagicComponent>(e, ecs::MartialMagicComponent{
        std::move(sword_style),
        sword_rank,
        std::move(magic_school),
        magic_rank,
        mana_capacity,
        false
    });
    registry.emplace<ecs::FaithComponent>(e, ecs::FaithComponent{
        rite,
        0.72,
        rite == RiteType::ImperialOrthodox ? 0.72 : 0.32
    });
    registry.emplace<ecs::FamilyComponent>(e, ecs::FamilyComponent{
        head_entity == entt::null ? e : head_entity,
        mother_entity,
        father_entity,
        role,
        is_bastard,
        ambition,
        backing,
        0,
        15,
        std::move(note),
        person_id,
        regnal_number,
        std::move(given_name),
        std::move(patronymic),
        ""
    });
    return e;
}

std::unordered_set<std::string> collectHouseholdGivenNames(const entt::registry& registry,
                                                            entt::entity r_ent,
                                                            std::string_view r_given) {
    std::unordered_set<std::string> used;
    used.insert(std::string(r_given));
    if (registry.all_of<ecs::HouseholdComponent>(r_ent)) {
        for (const entt::entity me : registry.get<ecs::HouseholdComponent>(r_ent).member_entities) {
            if (!registry.valid(me) || !registry.all_of<ecs::CharacterComponent, ecs::FamilyComponent>(me)) {
                continue;
            }
            if (!registry.get<ecs::CharacterComponent>(me).alive) {
                continue;
            }
            const auto& mf = registry.get<ecs::FamilyComponent>(me);
            if (!mf.given_name.empty()) {
                used.insert(mf.given_name);
            } else {
                used.insert(FamilyTree::extractGivenName(registry.get<ecs::NameComponent>(me).value));
            }
        }
    }
    return used;
}

} // namespace

void DynastySystem::refreshCourtOffices(entt::registry& registry,
                                        const core::SimClock& clock,
                                        HistoryLog& log) {
    for (const entt::entity s_ent : registry.view<ecs::SettlementComponent>()) {
        auto& sett = registry.get<ecs::SettlementComponent>(s_ent);
        const std::string s_name = registry.all_of<ecs::NameComponent>(s_ent)
            ? registry.get<ecs::NameComponent>(s_ent).value
            : std::string("Seat");

        sett.sword_instructor_entity = entt::null;
        sett.court_mage_entity = entt::null;
        sett.court_chaplain_entity = entt::null;

        const entt::entity r_ent = sett.ruler_entity;
        if (r_ent == entt::null || !registry.valid(r_ent) ||
            !registry.all_of<ecs::CharacterComponent>(r_ent) ||
            !registry.get<ecs::CharacterComponent>(r_ent).alive) {
            sett.regent_entity = entt::null;
            continue;
        }

        const auto& r_ch = registry.get<ecs::CharacterComponent>(r_ent);
        const std::string r_name = registry.all_of<ecs::NameComponent>(r_ent)
            ? registry.get<ecs::NameComponent>(r_ent).value
            : std::string("Ruler");

        if (r_ch.age >= 16 && sett.regent_entity != entt::null) {
            sett.regent_entity = entt::null;
            log.add(clock.year(), clock.season(), clock.month(), clock.day(),
                    LogCategory::Dynasty,
                    std::format("{} came of age at {} and assumed full personal rule.", r_name, s_name));
        }

        if (!registry.all_of<ecs::HouseholdComponent>(r_ent)) {
            continue;
        }
        const auto& hh = registry.get<ecs::HouseholdComponent>(r_ent);

        int best_sword_score = -1;
        double best_mage_score = -1.0;
        double best_chap_score = -1.0;
        int best_regent_backing = -1;
        entt::entity best_regent_cand = entt::null;

        for (const entt::entity me : hh.member_entities) {
            if (me == r_ent || me == entt::null || !registry.valid(me) ||
                !registry.all_of<ecs::CharacterComponent, ecs::FamilyComponent>(me)) {
                continue;
            }
            const auto& m_ch = registry.get<ecs::CharacterComponent>(me);
            const auto& m_fam = registry.get<ecs::FamilyComponent>(me);
            if (!m_ch.alive || m_ch.age < 16) {
                continue;
            }

            if (registry.all_of<ecs::MartialMagicComponent>(me)) {
                const auto& m_mm = registry.get<ecs::MartialMagicComponent>(me);
                if (m_mm.sword_rank >= SkillRank::Intermediate || m_fam.role == DynastyRole::RetainerTutor) {
                    const int sc = static_cast<int>(m_mm.sword_rank) * 10 +
                                   (m_fam.role == DynastyRole::RetainerTutor ? 2 : 0);
                    if (sc > best_sword_score) {
                        best_sword_score = sc;
                        sett.sword_instructor_entity = me;
                    }
                }
                if (m_mm.magic_rank >= SkillRank::Beginner || m_mm.mana_capacity >= 70.0) {
                    const double mc = static_cast<double>(m_mm.magic_rank) * 100.0 + m_mm.mana_capacity;
                    if (mc > best_mage_score) {
                        best_mage_score = mc;
                        sett.court_mage_entity = me;
                    }
                }
            }

            if (registry.all_of<ecs::FaithComponent>(me)) {
                const auto& m_faith = registry.get<ecs::FaithComponent>(me);
                if (m_fam.role == DynastyRole::ClergyMember || m_faith.zeal >= 0.72) {
                    const double cs = (m_fam.role == DynastyRole::ClergyMember ? 2.0 : 0.0) + m_faith.zeal;
                    if (cs > best_chap_score) {
                        best_chap_score = cs;
                        sett.court_chaplain_entity = me;
                    }
                }
            }

            if (m_fam.faction_backing > best_regent_backing) {
                best_regent_backing = m_fam.faction_backing;
                best_regent_cand = me;
            }
        }

        if (r_ch.age < 16) {
            const bool regent_valid =
                sett.regent_entity != entt::null &&
                registry.valid(sett.regent_entity) &&
                registry.all_of<ecs::CharacterComponent>(sett.regent_entity) &&
                registry.get<ecs::CharacterComponent>(sett.regent_entity).alive &&
                registry.get<ecs::CharacterComponent>(sett.regent_entity).age >= 16;
            if (!regent_valid && best_regent_cand != entt::null) {
                sett.regent_entity = best_regent_cand;
            }
        }
    }
}

void DynastySystem::tickDaily(entt::registry& registry,
                              const data::ConfigRegistry& config,
                              const core::SimClock& clock,
                              core::Rng& rng,
                              FamilyTree& family_tree,
                              HistoryLog& log) {
    const int today_m = clock.month();
    const int today_d = clock.day();

    // 1. Character birthdays: age & annual martial/magic/mana growth on each individual's exact birth date
    std::vector<entt::entity> birthday_chars;
    const auto char_view = registry.view<ecs::CharacterComponent>();
    for (const entt::entity ce : char_view) {
        const auto& ch = char_view.get<ecs::CharacterComponent>(ce);
        if (ch.alive && ch.birth_month == today_m && ch.birth_day == today_d) {
            birthday_chars.push_back(ce);
        }
    }

    if (!birthday_chars.empty()) {
        auto seatHasTutor = [&](entt::entity seat) -> bool {
            if (seat == entt::null || !registry.valid(seat)) return false;
            const auto& sett = registry.get<ecs::SettlementComponent>(seat);
            if (sett.sword_instructor_entity != entt::null && registry.valid(sett.sword_instructor_entity)) {
                return true;
            }
            if (sett.ruler_entity == entt::null || !registry.all_of<ecs::HouseholdComponent>(sett.ruler_entity)) {
                return false;
            }
            for (const entt::entity me : registry.get<ecs::HouseholdComponent>(sett.ruler_entity).member_entities) {
                if (registry.valid(me) && registry.all_of<ecs::FamilyComponent>(me) &&
                    registry.get<ecs::FamilyComponent>(me).role == DynastyRole::RetainerTutor) {
                    return true;
                }
            }
            return false;
        };

        for (const entt::entity ce : birthday_chars) {
            if (!registry.valid(ce)) continue;
            auto& ch = registry.get<ecs::CharacterComponent>(ce);
            if (!ch.alive) continue;

            ++ch.age;

            if (registry.all_of<ecs::MartialMagicComponent, ecs::FamilyComponent>(ce)) {
                auto& mm = registry.get<ecs::MartialMagicComponent>(ce);
                auto& fam = registry.get<ecs::FamilyComponent>(ce);

                entt::entity seat_mage = entt::null;
                entt::entity seat_instructor = entt::null;
                int seat_mages_ready = 0;
                if (ch.seat_settlement != entt::null && registry.valid(ch.seat_settlement) &&
                    registry.all_of<ecs::SettlementComponent>(ch.seat_settlement)) {
                    const auto& seat_sett = registry.get<ecs::SettlementComponent>(ch.seat_settlement);
                    seat_mages_ready = seat_sett.mages_ready;
                    if (seat_sett.court_mage_entity != entt::null && registry.valid(seat_sett.court_mage_entity)) {
                        seat_mage = seat_sett.court_mage_entity;
                    }
                    if (seat_sett.sword_instructor_entity != entt::null && registry.valid(seat_sett.sword_instructor_entity)) {
                        seat_instructor = seat_sett.sword_instructor_entity;
                    }
                }

                // Childhood mana training window (ages 3..10) & literary trait imprinting at age 10
                if (ch.age >= 3 && ch.age <= 10 &&
                    (fam.role == DynastyRole::LegitimateChild || fam.role == DynastyRole::BastardChild)) {
                    const bool has_mages = (seat_mages_ready > 0 || seat_mage != entt::null);
                    if (has_mages || mm.mana_capacity >= 40.0) {
                        const double mentor_bonus = (seat_mage != entt::null) ? 3.5 : 0.0;
                        mm.mana_capacity = std::min(220.0, mm.mana_capacity + rng.nextRange(6.0, 15.0) + mentor_bonus);
                    }
                    if (mm.magic_school == "None" && seat_mage != entt::null &&
                        registry.all_of<ecs::MartialMagicComponent>(seat_mage)) {
                        const auto& mage_school = registry.get<ecs::MartialMagicComponent>(seat_mage).magic_school;
                        if (mage_school != "None" && !mage_school.empty()) {
                            mm.magic_school = mage_school;
                        }
                    }
                    if (mm.mana_capacity >= 65.0 && mm.magic_rank == SkillRank::None) {
                        if (mm.magic_school == "None" && !config.magicSchools().empty()) {
                            mm.magic_school = rng.pick(std::span<const data::MagicSchoolDef>(config.magicSchools())).name;
                        }
                        mm.magic_rank = SkillRank::Beginner;
                    }
                    if (ch.age == 10 && ch.seat_settlement != entt::null) {
                        for (const entt::entity le : registry.view<ecs::LiteraryWorkComponent>()) {
                            const auto& work = registry.get<ecs::LiteraryWorkComponent>(le);
                            if (work.holder_settlement == ch.seat_settlement && !work.trait_imprint.empty()) {
                                ch.trait = work.trait_imprint;
                                break;
                            }
                        }
                    }
                }

                // Martial & magic rank-ups (ages 9..48)
                if (ch.age >= 9 && ch.age <= 48) {
                    const bool has_tutor = seatHasTutor(ch.seat_settlement);
                    const bool has_sword_mentor = has_tutor || (seat_instructor != entt::null);
                    const bool has_magic_mentor = has_tutor || (seat_mage != entt::null);

                    if (mm.sword_rank < SkillRank::Emperor &&
                        (!registry.all_of<ecs::SocialStatusComponent>(ce) || !registry.get<ecs::SocialStatusComponent>(ce).is_clergy)) {
                        if (rng.nextDouble() < promotionProbability(mm.sword_rank, has_sword_mentor)) {
                            if (mm.sword_style == "None" && seat_instructor != entt::null &&
                                registry.all_of<ecs::MartialMagicComponent>(seat_instructor)) {
                                const auto& inst_style = registry.get<ecs::MartialMagicComponent>(seat_instructor).sword_style;
                                if (inst_style != "None" && !inst_style.empty()) {
                                    mm.sword_style = inst_style;
                                }
                            }
                            if (mm.sword_style == "None" && !config.swordStyles().empty()) {
                                mm.sword_style = rng.pick(std::span<const data::SwordStyleDef>(config.swordStyles())).name;
                            }
                            mm.sword_rank = nextSkillRank(mm.sword_rank);
                            if (mm.sword_rank >= SkillRank::Saint) {
                                const auto& c_name = registry.get<ecs::NameComponent>(ce).value;
                                log.add(clock.year(), clock.season(), today_m, today_d,
                                        LogCategory::Dynasty,
                                        std::format("{} attained the {} tier in {}!",
                                                    c_name, toString(mm.sword_rank), mm.sword_style));
                            }
                        }
                    }

                    if (mm.mana_capacity >= 50.0 && mm.magic_rank < SkillRank::Emperor) {
                        if (rng.nextDouble() < promotionProbability(mm.magic_rank, has_magic_mentor)) {
                            if (mm.magic_school == "None" && seat_mage != entt::null &&
                                registry.all_of<ecs::MartialMagicComponent>(seat_mage)) {
                                const auto& mage_school = registry.get<ecs::MartialMagicComponent>(seat_mage).magic_school;
                                if (mage_school != "None" && !mage_school.empty()) {
                                    mm.magic_school = mage_school;
                                }
                            }
                            if (mm.magic_school == "None" && !config.magicSchools().empty()) {
                                mm.magic_school = rng.pick(std::span<const data::MagicSchoolDef>(config.magicSchools())).name;
                            }
                            mm.magic_rank = nextSkillRank(mm.magic_rank);
                        }
                    }

                    // Rare Incantationless Magic discovery
                    if (mm.magic_rank >= SkillRank::Advanced && mm.mana_capacity >= 125.0 &&
                        !mm.incantationless && rng.nextDouble() < 0.045) {
                        mm.incantationless = true;
                        const auto& c_name = registry.get<ecs::NameComponent>(ce).value;
                        const std::string theorem_name = std::format("{}'s Silent Casting Theorem of {}", c_name, clock.year());
                        const entt::entity law_ent = registry.create();
                        registry.emplace<ecs::NameComponent>(law_ent, theorem_name);
                        registry.emplace<ecs::ActiveLawComponent>(law_ent, ecs::ActiveLawComponent{
                            theorem_name,
                            c_name,
                            clock.year(),
                            (ch.seat_settlement != entt::null && registry.valid(ch.seat_settlement))
                                ? registry.get<ecs::NameComponent>(ch.seat_settlement).value
                                : std::string("Nordmark"),
                            std::format("Codifies incantationless {} breakthroughs.", mm.magic_school),
                            "theorem"
                        });
                        if (ch.seat_settlement != entt::null && registry.valid(ch.seat_settlement)) {
                            registry.get<ecs::SettlementComponent>(ch.seat_settlement).mages_ready += 1;
                        }
                        log.add(clock.year(), clock.season(), today_m, today_d,
                                LogCategory::Eponym,
                                std::format("{} achieved Incantationless {} Magic ({})!",
                                            c_name, mm.magic_school, theorem_name));
                    }
                }

                // Cognomen / epithet assignment
                if (fam.epithet.empty()) {
                    if (mm.incantationless) {
                        fam.epithet = "the Silent";
                    } else if (mm.sword_rank >= SkillRank::Emperor) {
                        fam.epithet = "the Blade-Emperor";
                    } else if (mm.sword_rank == SkillRank::King) {
                        fam.epithet = "the Sword-King";
                    } else if (fam.role == DynastyRole::HeadOfHouse && ch.age >= 62 && !config.epithets().empty()) {
                        fam.epithet = rng.pick(std::span<const std::string>(config.epithets()));
                    }
                }

                // Sync living ECS fields -> persistent FamilyTree node
                if (PersonNode* pnode = family_tree.findMutable(fam.person_id)) {
                    pnode->current_age = ch.age;
                    pnode->trait = ch.trait;
                    pnode->sword_style = mm.sword_style;
                    pnode->sword_rank = mm.sword_rank;
                    pnode->magic_school = mm.magic_school;
                    pnode->magic_rank = mm.magic_rank;
                    pnode->mana_capacity = mm.mana_capacity;
                    pnode->incantationless = mm.incantationless;
                    pnode->epithet = fam.epithet;
                    if (registry.all_of<ecs::SocialStatusComponent>(ce)) {
                        const auto& soc = registry.get<ecs::SocialStatusComponent>(ce);
                        pnode->title = soc.title;
                        pnode->house = soc.house;
                    }
                }
            }
        }
    }

    // 2. Daily Birth Deliveries on the mother's due day
    struct DueBirth {
        entt::entity seat = entt::null;
        entt::entity ruler = entt::null;
        entt::entity mother = entt::null;
        PersonId mother_pid = kInvalidPersonId;
        bool is_bastard = false;
    };
    std::vector<DueBirth> due_births;

    const auto sett_view = registry.view<ecs::SettlementComponent>();
    for (const entt::entity s_ent : sett_view) {
        const entt::entity r_ent = sett_view.get<ecs::SettlementComponent>(s_ent).ruler_entity;
        if (r_ent == entt::null || !registry.valid(r_ent) ||
            !registry.all_of<ecs::CharacterComponent, ecs::HouseholdComponent>(r_ent) ||
            !registry.get<ecs::CharacterComponent>(r_ent).alive) {
            continue;
        }
        const auto& hh = registry.get<ecs::HouseholdComponent>(r_ent);
        for (const entt::entity me : hh.member_entities) {
            if (!registry.valid(me) || !registry.all_of<ecs::CharacterComponent, ecs::FamilyComponent>(me)) {
                continue;
            }
            const auto& m_ch = registry.get<ecs::CharacterComponent>(me);
            auto& m_fam = registry.get<ecs::FamilyComponent>(me);
            if (m_ch.alive && m_ch.is_female &&
                (m_fam.role == DynastyRole::LegitimateSpouse || m_fam.role == DynastyRole::Concubine) &&
                m_fam.pregnancy_months == 1 &&
                today_d == std::clamp(m_fam.pregnancy_due_day, 1, 30)) {
                m_fam.pregnancy_months = 0;
                due_births.push_back(DueBirth{
                    s_ent,
                    r_ent,
                    me,
                    m_fam.person_id,
                    m_fam.role == DynastyRole::Concubine
                });
            }
        }
    }

    for (const auto& db : due_births) {
        const auto& s_name = registry.get<ecs::NameComponent>(db.seat).value;
        const auto& r_soc = registry.get<ecs::SocialStatusComponent>(db.ruler);
        const auto& r_faith = registry.get<ecs::FaithComponent>(db.ruler);
        const auto& r_name = registry.get<ecs::NameComponent>(db.ruler).value;
        const auto& r_fam = registry.get<ecs::FamilyComponent>(db.ruler);
        const PersonId r_pid = r_fam.person_id;
        const PersonNode* r_pnode = family_tree.find(r_pid);
        const std::string r_given = !r_fam.given_name.empty()
            ? r_fam.given_name
            : FamilyTree::extractGivenName(r_name);
        const int r_gen = r_pnode ? r_pnode->generation : 1;

        auto used_names = collectHouseholdGivenNames(registry, db.ruler, r_given);
        const bool girl = rng.nextDouble() < 0.48;
        const std::string child_first = pickDistinctFirstName(
            config, rng, girl, used_names, &family_tree, r_pid, !db.is_bastard);

        const std::string pat = FamilyTree::makePatronymic(r_given, girl, r_soc.house);
        std::string child_name;
        std::string bastard_cog;
        if (db.is_bastard) {
            bastard_cog = config.bastardCognomens().empty()
                ? std::string("Snow")
                : rng.pick(std::span<const std::string>(config.bastardCognomens()));
            child_name = std::format("{} '{}' {}", child_first, bastard_cog, r_soc.house);
        } else {
            child_name = child_first + " " + r_soc.house;
        }

        const bool prodigy = rng.nextDouble() < 0.12;
        const double mana = prodigy ? rng.nextRange(112.0, 168.0) : rng.nextRange(22.0, 88.0);
        const std::string child_trait = pickTrait(config, rng);
        const std::string note = prodigy
            ? "High mana prodigy"
            : (db.is_bastard ? std::format("Natural child ({})", bastard_cog) : std::string("Noble scion"));
        const std::string role_title = db.is_bastard ? "Natural Child" : "Noble Heir";
        const std::string magic_school = mana >= 70.0 ? "Frost/Water" : "None";

        PersonNode c_node;
        c_node.given_name = child_first;
        c_node.patronymic = pat;
        c_node.house = r_soc.house;
        c_node.birth_house = r_soc.house;
        c_node.title = role_title;
        c_node.seat_name = s_name;
        c_node.generation = r_gen + 1;
        c_node.is_female = girl;
        c_node.alive = true;
        c_node.is_bastard = db.is_bastard;
        c_node.was_ruler = false;
        c_node.birth_year = clock.year();
        c_node.birth_month = today_m;
        c_node.birth_day = today_d;
        c_node.current_age = 0;
        c_node.trait = child_trait;
        c_node.magic_school = magic_school;
        c_node.mana_capacity = mana;
        c_node.father_id = r_pid;
        c_node.mother_id = db.mother_pid;
        const PersonId child_pid = family_tree.registerPerson(std::move(c_node));
        family_tree.addChild(r_pid, db.mother_pid, child_pid);

        const entt::entity child = createNobleMember(
            registry,
            child_name,
            0,
            today_m,
            today_d,
            girl,
            child_trait,
            db.seat,
            role_title,
            r_soc.house,
            false,
            db.is_bastard ? DynastyRole::BastardChild : DynastyRole::LegitimateChild,
            db.ruler,
            db.mother,
            db.ruler,
            db.is_bastard,
            db.is_bastard ? 0.55 : 0.40,
            db.is_bastard ? 40 : 55,
            r_faith.rite,
            mana,
            "None",
            SkillRank::None,
            magic_school,
            SkillRank::None,
            note,
            child_pid,
            0,
            child_first,
            pat);
        if (auto* pmut = family_tree.findMutable(child_pid)) {
            pmut->live_entity = child;
        }

        registry.get<ecs::HouseholdComponent>(db.ruler).member_entities.push_back(child);
        if (prodigy || !db.is_bastard) {
            log.add(clock.year(), clock.season(), today_m, today_d,
                    LogCategory::Dynasty,
                    std::format("{} ({}) was born on {} {} (M{} D{}) to {} of House {}{}.",
                                child_name, pat, today_d, monthName(today_m), today_m, today_d,
                                r_name, r_soc.house,
                                prodigy ? " (high mana aptitude)" : ""));
        }
    }
}

void DynastySystem::tickMonthly(entt::registry& registry,
                                const data::ConfigRegistry& config,
                                const core::SimClock& clock,
                                core::Rng& rng,
                                FamilyTree& family_tree,
                                HistoryLog& log) {
    std::vector<entt::entity> settlements;
    for (const entt::entity se : registry.view<ecs::SettlementComponent>()) {
        settlements.push_back(se);
    }

    for (const entt::entity s_ent : settlements) {
        const auto& sett = registry.get<ecs::SettlementComponent>(s_ent);
        const entt::entity r_ent = sett.ruler_entity;
        if (r_ent == entt::null || !registry.valid(r_ent) ||
            !registry.all_of<ecs::CharacterComponent, ecs::SocialStatusComponent, ecs::FaithComponent>(r_ent)) {
            continue;
        }

        const auto& r_ch = registry.get<ecs::CharacterComponent>(r_ent);
        if (!r_ch.alive) {
            continue;
        }
        const auto& s_name = registry.get<ecs::NameComponent>(s_ent).value;
        const auto& r_soc = registry.get<ecs::SocialStatusComponent>(r_ent);
        const auto& r_faith = registry.get<ecs::FaithComponent>(r_ent);
        const auto& r_name = registry.get<ecs::NameComponent>(r_ent).value;
        const auto& r_fam = registry.get<ecs::FamilyComponent>(r_ent);
        const PersonId r_pid = r_fam.person_id;
        const PersonNode* r_pnode = family_tree.find(r_pid);
        const std::string r_given = !r_fam.given_name.empty()
            ? r_fam.given_name
            : FamilyTree::extractGivenName(r_name);
        const int r_gen = r_pnode ? r_pnode->generation : 1;

        if (!registry.all_of<ecs::HouseholdComponent>(r_ent)) {
            registry.emplace<ecs::HouseholdComponent>(r_ent);
        }

        std::vector<entt::entity> members;
        {
            auto& hh = registry.get<ecs::HouseholdComponent>(r_ent);
            std::erase_if(hh.member_entities, [&](entt::entity me) {
                return me == entt::null || !registry.valid(me) ||
                       !registry.all_of<ecs::CharacterComponent, ecs::FamilyComponent>(me) ||
                       !registry.get<ecs::CharacterComponent>(me).alive;
            });
            members = hh.member_entities;
        }
        std::unordered_set<std::string> used_names = collectHouseholdGivenNames(registry, r_ent, r_given);

        bool has_spouse = false;
        bool has_concubine = false;
        int living_children = 0;

        for (const entt::entity me : members) {
            auto& m_ch = registry.get<ecs::CharacterComponent>(me);
            auto& m_fam = registry.get<ecs::FamilyComponent>(me);

            if (m_fam.role == DynastyRole::LegitimateSpouse) {
                has_spouse = true;
            } else if (m_fam.role == DynastyRole::Concubine) {
                has_concubine = true;
            } else if (m_fam.role == DynastyRole::LegitimateChild || m_fam.role == DynastyRole::BastardChild) {
                ++living_children;
            }

            if (m_ch.is_female && (m_fam.role == DynastyRole::LegitimateSpouse || m_fam.role == DynastyRole::Concubine)) {
                if (m_fam.pregnancy_months > 1) {
                    --m_fam.pregnancy_months;
                } else if (m_fam.pregnancy_months == 0 && m_ch.age >= 18 && m_ch.age <= 42 &&
                           living_children < 4 && r_ch.age >= 18) {
                    if (rng.nextDouble() < 0.04) {
                        m_fam.pregnancy_months = 9;
                        m_fam.pregnancy_due_day = rng.nextInt(1, 30);
                    }
                }
            }
        }

        // Arrange marriage for unmarried secular rulers (prefer cross-house scions across settlements)
        if (!r_soc.is_clergy && !has_spouse && r_ch.age >= 18 && r_ch.age <= 58 && rng.nextDouble() < 0.25) {
            const bool spouse_female = !r_ch.is_female;

            struct MarriageCandidate {
                entt::entity scion = entt::null;
                entt::entity natal_seat = entt::null;
                entt::entity natal_ruler = entt::null;
            };
            MarriageCandidate diff_house_match;
            MarriageCandidate same_house_match;

            for (const entt::entity other_s : settlements) {
                if (other_s == s_ent || !registry.valid(other_s)) {
                    continue;
                }
                const entt::entity other_r = registry.get<ecs::SettlementComponent>(other_s).ruler_entity;
                if (other_r == entt::null || !registry.valid(other_r) ||
                    !registry.all_of<ecs::HouseholdComponent>(other_r)) {
                    continue;
                }
                for (const entt::entity cand : registry.get<ecs::HouseholdComponent>(other_r).member_entities) {
                    if (!registry.valid(cand) ||
                        !registry.all_of<ecs::CharacterComponent, ecs::SocialStatusComponent, ecs::FamilyComponent>(cand)) {
                        continue;
                    }
                    const auto& cand_ch = registry.get<ecs::CharacterComponent>(cand);
                    const auto& cand_soc = registry.get<ecs::SocialStatusComponent>(cand);
                    const auto& cand_fam = registry.get<ecs::FamilyComponent>(cand);
                    if (!cand_ch.alive || cand_ch.is_female != spouse_female ||
                        cand_ch.age < 18 || cand_ch.age > 40) {
                        continue;
                    }
                    if (cand_fam.role != DynastyRole::LegitimateChild &&
                        cand_fam.role != DynastyRole::BastardChild) {
                        continue;
                    }
                    if (cand_fam.father_entity == r_ent || cand_fam.mother_entity == r_ent) {
                        continue;
                    }
                    if (cand_soc.house != r_soc.house) {
                        if (diff_house_match.scion == entt::null) {
                            diff_house_match = MarriageCandidate{cand, other_s, other_r};
                        }
                    } else if (same_house_match.scion == entt::null) {
                        same_house_match = MarriageCandidate{cand, other_s, other_r};
                    }
                }
                if (diff_house_match.scion != entt::null) {
                    break;
                }
            }

            const MarriageCandidate chosen = (diff_house_match.scion != entt::null)
                ? diff_house_match
                : same_house_match;

            if (chosen.scion != entt::null) {
                auto& natal_hh = registry.get<ecs::HouseholdComponent>(chosen.natal_ruler).member_entities;
                std::erase(natal_hh, chosen.scion);

                auto& c_ch = registry.get<ecs::CharacterComponent>(chosen.scion);
                auto& c_soc = registry.get<ecs::SocialStatusComponent>(chosen.scion);
                auto& c_fam = registry.get<ecs::FamilyComponent>(chosen.scion);
                const std::string c_name = registry.get<ecs::NameComponent>(chosen.scion).value;
                const std::string natal_house = c_soc.house;
                const std::string natal_seat_name = registry.get<ecs::NameComponent>(chosen.natal_seat).value;
                const std::string sp_role = spouse_female ? "Legitimate Wife" : "Consort";

                c_ch.seat_settlement = s_ent;
                c_fam.head_entity = r_ent;
                c_fam.role = DynastyRole::LegitimateSpouse;
                c_fam.is_bastard = false;
                c_fam.note = std::format("Alliance w/ {} ({})", natal_house, natal_seat_name);
                c_soc.title = sp_role;
                c_soc.prestige += 12;

                if (PersonNode* cnode = family_tree.findMutable(c_fam.person_id)) {
                    cnode->seat_name = s_name;
                    cnode->title = sp_role;
                    cnode->is_bastard = false;
                }
                family_tree.addSpouse(r_pid, c_fam.person_id);

                registry.get<ecs::HouseholdComponent>(r_ent).member_entities.push_back(chosen.scion);
                auto& here_sett = registry.get<ecs::SettlementComponent>(s_ent);
                auto& other_sett = registry.get<ecs::SettlementComponent>(chosen.natal_seat);
                here_sett.prosperity = std::min(0.99, here_sett.prosperity + 0.02);
                other_sett.prosperity = std::min(0.99, other_sett.prosperity + 0.02);

                log.add(clock.year(), clock.season(), clock.month(), clock.day(),
                        LogCategory::Dynasty,
                        std::format("Cross-house marriage: {} of {} wed {} of House {} (from {}).",
                                    r_name, s_name, c_name, natal_house, natal_seat_name));
            } else {
                const std::string spouse_house = pickOtherHouse(config, rng, r_soc.house);
                const std::string sp_first = pickDistinctFirstName(config, rng, spouse_female, used_names);
                used_names.insert(sp_first);

                const std::string spouse_name = (spouse_female ? "Lady " : "Lord ") + sp_first + " " + spouse_house;
                const std::string sp_role = spouse_female ? "Legitimate Wife" : "Consort";
                const std::string sp_trait = pickTrait(config, rng);
                const int spouse_age = std::clamp(r_ch.age - rng.nextInt(-2, 5), 18, 38);
                const int sp_bmonth = rng.nextInt(1, 12);
                const int sp_bday = rng.nextInt(1, 30);
                const double sp_mana = rng.nextRange(24.0, 72.0);
                const std::string sp_pat = FamilyTree::makePatronymic(
                    pickFirstName(config, rng, false), spouse_female, spouse_house);

                PersonNode sp_node;
                sp_node.given_name = sp_first;
                sp_node.patronymic = sp_pat;
                sp_node.house = r_soc.house;
                sp_node.birth_house = spouse_house;
                sp_node.title = sp_role;
                sp_node.seat_name = s_name;
                sp_node.generation = r_gen;
                sp_node.is_female = spouse_female;
                sp_node.alive = true;
                sp_node.birth_year = clock.year() - spouse_age;
                sp_node.birth_month = sp_bmonth;
                sp_node.birth_day = sp_bday;
                sp_node.current_age = spouse_age;
                sp_node.trait = sp_trait;
                sp_node.mana_capacity = sp_mana;
                const PersonId sp_pid = family_tree.registerPerson(std::move(sp_node));
                family_tree.addSpouse(r_pid, sp_pid);

                const entt::entity spouse = createNobleMember(
                    registry,
                    spouse_name,
                    spouse_age,
                    sp_bmonth,
                    sp_bday,
                    spouse_female,
                    sp_trait,
                    s_ent,
                    sp_role,
                    spouse_house,
                    false,
                    DynastyRole::LegitimateSpouse,
                    r_ent,
                    entt::null,
                    entt::null,
                    false,
                    0.52,
                    62,
                    r_faith.rite,
                    sp_mana,
                    "None",
                    SkillRank::None,
                    "None",
                    SkillRank::None,
                    "House " + spouse_house + " alliance",
                    sp_pid,
                    0,
                    sp_first,
                    sp_pat);
                if (auto* pmut = family_tree.findMutable(sp_pid)) {
                    pmut->live_entity = spouse;
                }
                registry.get<ecs::HouseholdComponent>(r_ent).member_entities.push_back(spouse);
                log.add(clock.year(), clock.season(), clock.month(), clock.day(),
                        LogCategory::Dynasty,
                        std::format("{} wed {} of House {}.", r_name, spouse_name, spouse_house));
            }
        }

        // Ambitious / high-prestige secular lords may take a concubine with guild/faction backing
        if (!r_soc.is_clergy && !r_ch.is_female && !has_concubine && r_ch.age >= 22 && r_ch.age <= 54 &&
            clock.month() == 6 &&
            (r_soc.prestige >= 70 || r_ch.trait == "Ambitious" || r_ch.trait == "Scheming" || r_ch.trait == "Mercantile") &&
            rng.nextDouble() < 0.22) {
            const std::string c_first = pickDistinctFirstName(config, rng, true, used_names);
            used_names.insert(c_first);
            const std::string c_birth_house = pickOtherHouse(config, rng, r_soc.house);
            const int c_age = std::clamp(r_ch.age - rng.nextInt(4, 12), 19, 32);
            const int c_bmonth = rng.nextInt(1, 12);
            const int c_bday = rng.nextInt(1, 30);
            const double c_mana = rng.nextRange(20.0, 60.0);
            const std::string c_pat = FamilyTree::makePatronymic(
                pickFirstName(config, rng, false), true, c_birth_house);

            PersonNode conc_node;
            conc_node.given_name = c_first;
            conc_node.patronymic = c_pat;
            conc_node.house = r_soc.house;
            conc_node.birth_house = c_birth_house;
            conc_node.title = "Concubine";
            conc_node.seat_name = s_name;
            conc_node.generation = r_gen;
            conc_node.is_female = true;
            conc_node.alive = true;
            conc_node.birth_year = clock.year() - c_age;
            conc_node.birth_month = c_bmonth;
            conc_node.birth_day = c_bday;
            conc_node.current_age = c_age;
            conc_node.trait = "Scheming";
            conc_node.mana_capacity = c_mana;
            const PersonId c_pid = family_tree.registerPerson(std::move(conc_node));
            family_tree.addConcubine(r_pid, c_pid);

            const entt::entity conc = createNobleMember(
                registry,
                c_first + " of " + c_birth_house,
                c_age,
                c_bmonth,
                c_bday,
                true,
                "Scheming",
                s_ent,
                "Concubine",
                r_soc.house,
                false,
                DynastyRole::Concubine,
                r_ent,
                entt::null,
                entt::null,
                false,
                rng.nextRange(0.65, 0.90),
                rng.nextInt(55, 85),
                r_faith.rite,
                c_mana,
                "None",
                SkillRank::None,
                "None",
                SkillRank::None,
                "Merchant faction backing",
                c_pid,
                0,
                c_first,
                c_pat);
            if (auto* pmut = family_tree.findMutable(c_pid)) {
                pmut->live_entity = conc;
            }
            registry.get<ecs::HouseholdComponent>(r_ent).member_entities.push_back(conc);
        }
    }

    refreshCourtOffices(registry, clock, log);
}

void DynastySystem::tickYearly(entt::registry& registry,
                               const data::ConfigRegistry& config,
                               const world::GeneratedWorldInfo& world_info,
                               const core::SimClock& clock,
                               core::Rng& rng,
                               FamilyTree& family_tree,
                               std::vector<ecs::MemorialEntry>& memorial_archive,
                               HistoryLog& log) {
    // 1. Natural mortality check (aging & annual skill progression occur on individual birthdays in tickDaily)
    std::vector<entt::entity> chars;
    for (const entt::entity e : registry.view<ecs::CharacterComponent>()) {
        chars.push_back(e);
    }

    for (const entt::entity ce : chars) {
        if (!registry.valid(ce)) continue;
        auto& ch = registry.get<ecs::CharacterComponent>(ce);
        if (!ch.alive) continue;

        bool dies = false;
        if (ch.age >= 92) {
            dies = true;
        } else if (ch.age >= 60) {
            const double hazard = 0.025 + static_cast<double>(ch.age - 60) * 0.008;
            if (rng.nextDouble() < hazard) {
                dies = true;
            }
        }

        if (dies) {
            ch.alive = false;
            const auto& c_name = registry.get<ecs::NameComponent>(ce).value;
            const auto& soc = registry.get<ecs::SocialStatusComponent>(ce);
            const auto& mm = registry.get<ecs::MartialMagicComponent>(ce);
            const auto& fam = registry.get<ecs::FamilyComponent>(ce);

            if (PersonNode* pnode = family_tree.findMutable(fam.person_id)) {
                pnode->alive = false;
                pnode->current_age = ch.age;
                pnode->death_year = clock.year();
                pnode->age_at_death = ch.age;
                pnode->live_entity = entt::null;
            }

            if (fam.role == DynastyRole::HeadOfHouse || mm.sword_rank >= SkillRank::King || mm.incantationless) {
                const std::string ep_str = fam.epithet.empty() ? std::string() : (" '" + fam.epithet + "'");
                const std::string mem_name = (fam.regnal_number >= 2 && c_name.find(" " + FamilyTree::toRoman(fam.regnal_number)) == std::string::npos)
                    ? (c_name + " " + FamilyTree::toRoman(fam.regnal_number) + ep_str)
                    : (c_name + ep_str);

                std::string epitaph = mm.incantationless
                    ? "Master of Silent Magic"
                    : (mm.sword_rank >= SkillRank::Saint
                        ? std::format("{} {}", mm.sword_style, toString(mm.sword_rank))
                        : std::format("{} ruler of House {}", ch.trait, soc.house));
                pushMemorial(memorial_archive, ecs::MemorialEntry{
                    mem_name,
                    soc.title,
                    soc.house,
                    clock.year(),
                    ch.age,
                    std::move(epitaph)
                });
            }
        }
    }

    // 2. Process succession for every settlement whose ruler died or became invalid
    std::vector<entt::entity> settlements;
    for (const entt::entity se : registry.view<ecs::SettlementComponent>()) {
        settlements.push_back(se);
    }

    for (const entt::entity s_ent : settlements) {
        auto& sett = registry.get<ecs::SettlementComponent>(s_ent);
        const entt::entity old_ruler = sett.ruler_entity;
        const bool need_succession = (old_ruler == entt::null || !registry.valid(old_ruler) ||
                                      !registry.get<ecs::CharacterComponent>(old_ruler).alive);
        if (!need_succession) {
            continue;
        }

        const auto& s_name = registry.get<ecs::NameComponent>(s_ent).value;
        std::string old_name = "Former Lord";
        std::string old_title = "Lord";
        std::string old_house = "Vaelen";
        bool is_bishopric = (sett.type == SettlementType::Bishopric);
        RiteType old_rite = is_bishopric ? RiteType::HesychastMystic : RiteType::NorthernAutocephalous;
        PersonId old_ruler_pid = kInvalidPersonId;
        int old_gen = 1;

        if (old_ruler != entt::null && registry.valid(old_ruler)) {
            old_name = registry.get<ecs::NameComponent>(old_ruler).value;
            const auto& old_soc = registry.get<ecs::SocialStatusComponent>(old_ruler);
            old_title = old_soc.title;
            old_house = old_soc.house;
            old_rite = registry.get<ecs::FaithComponent>(old_ruler).rite;
            if (registry.all_of<ecs::FamilyComponent>(old_ruler)) {
                old_ruler_pid = registry.get<ecs::FamilyComponent>(old_ruler).person_id;
                if (const PersonNode* on = family_tree.find(old_ruler_pid)) {
                    old_gen = on->generation;
                }
            }
        }

        std::vector<entt::entity> old_members;
        if (old_ruler != entt::null && registry.valid(old_ruler) &&
            registry.all_of<ecs::HouseholdComponent>(old_ruler)) {
            old_members = registry.get<ecs::HouseholdComponent>(old_ruler).member_entities;
        }

        entt::entity chosen_heir = entt::null;
        double best_score = -1e9;
        bool has_legit_child = false;
        std::string best_legit_name;

        if (!is_bishopric) {
            for (const entt::entity me : old_members) {
                if (!registry.valid(me) || !registry.all_of<ecs::CharacterComponent, ecs::FamilyComponent>(me)) {
                    continue;
                }
                const auto& m_ch = registry.get<ecs::CharacterComponent>(me);
                const auto& m_fam = registry.get<ecs::FamilyComponent>(me);
                if (!m_ch.alive || (m_fam.role != DynastyRole::LegitimateChild && m_fam.role != DynastyRole::BastardChild)) {
                    continue;
                }

                if (m_fam.role == DynastyRole::LegitimateChild) {
                    has_legit_child = true;
                    best_legit_name = registry.get<ecs::NameComponent>(me).value;
                }

                const auto& m_mm = registry.get<ecs::MartialMagicComponent>(me);
                double score = m_fam.is_bastard ? 15.0 : 50.0;
                score += static_cast<double>(m_mm.sword_rank) * 14.0;
                score += static_cast<double>(m_mm.magic_rank) * 12.0;
                score += m_mm.mana_capacity * 0.12;
                if (m_ch.age < 12) {
                    score -= 18.0;
                }

                if (m_fam.is_bastard && m_fam.mother_entity != entt::null && registry.valid(m_fam.mother_entity) &&
                    registry.all_of<ecs::FamilyComponent>(m_fam.mother_entity)) {
                    const auto& mom_fam = registry.get<ecs::FamilyComponent>(m_fam.mother_entity);
                    score += mom_fam.ambition * static_cast<double>(mom_fam.faction_backing) * 0.65;
                }

                if (score > best_score) {
                    best_score = score;
                    chosen_heir = me;
                }
            }
        }

        if (chosen_heir != entt::null) {
            auto& h_fam = registry.get<ecs::FamilyComponent>(chosen_heir);
            auto& h_soc = registry.get<ecs::SocialStatusComponent>(chosen_heir);
            auto& h_name_comp = registry.get<ecs::NameComponent>(chosen_heir).value;
            const auto& h_ch = registry.get<ecs::CharacterComponent>(chosen_heir);

            const bool bastard_coup = h_fam.is_bastard && has_legit_child;
            if (bastard_coup && h_soc.house.find('-') == std::string::npos && !config.cadetSuffixes().empty()) {
                h_soc.house = h_soc.house + "-" + rng.pick(std::span<const std::string>(config.cadetSuffixes()));
            }

            const std::string heir_given = !h_fam.given_name.empty()
                ? h_fam.given_name
                : FamilyTree::extractGivenName(h_name_comp);
            const int r_num = family_tree.nextRegnalNumber(s_name, heir_given);

            h_fam.role = DynastyRole::HeadOfHouse;
            h_fam.head_entity = chosen_heir;
            h_fam.is_bastard = false;
            h_fam.regnal_number = r_num;
            h_fam.given_name = heir_given;
            h_soc.title = old_title;
            h_soc.prestige = std::max(55, h_soc.prestige + 15);
            h_name_comp = heir_given + (r_num >= 2 ? (" " + FamilyTree::toRoman(r_num)) : std::string()) + " " + h_soc.house;

            if (PersonNode* hnode = family_tree.findMutable(h_fam.person_id)) {
                hnode->was_ruler = true;
                hnode->seat_name = s_name;
                hnode->title = old_title;
                hnode->house = h_soc.house;
                hnode->regnal_number = r_num;
                hnode->current_age = h_ch.age;
                hnode->is_bastard = false;
                if (bastard_coup) {
                    hnode->bastard_coup_founder = true;
                }
                if (hnode->predecessor_ruler_id == kInvalidPersonId) {
                    hnode->predecessor_ruler_id = old_ruler_pid;
                }
            }

            ecs::HouseholdComponent new_hh;
            for (const entt::entity me : old_members) {
                if (new_hh.member_entities.size() >= 4) {
                    break;
                }
                if (me != chosen_heir && registry.valid(me) &&
                    registry.all_of<ecs::CharacterComponent, ecs::FamilyComponent>(me) &&
                    registry.get<ecs::CharacterComponent>(me).alive) {
                    auto& m_fam = registry.get<ecs::FamilyComponent>(me);
                    m_fam.head_entity = chosen_heir;
                    if (m_fam.role == DynastyRole::LegitimateSpouse || m_fam.role == DynastyRole::Concubine) {
                        m_fam.role = DynastyRole::RetainerTutor;
                        m_fam.pregnancy_months = 0;
                        registry.get<ecs::SocialStatusComponent>(me).title = "Dowager";
                    }
                    new_hh.member_entities.push_back(me);
                }
            }
            registry.emplace_or_replace<ecs::HouseholdComponent>(chosen_heir, std::move(new_hh));
            sett.ruler_entity = chosen_heir;
            sett.regent_entity = entt::null;

            if (bastard_coup) {
                if (registry.all_of<ecs::DemographicsComponent>(s_ent)) {
                    for (auto& c : registry.get<ecs::DemographicsComponent>(s_ent).cohorts) {
                        c.unrest = std::min(1.0, c.unrest + 0.12);
                    }
                }
                log.add(clock.year(), clock.season(), clock.month(), clock.day(),
                        LogCategory::Dynasty,
                        std::format("Succession coup in {}: concubine faction elevated {} (House {}) over {}!",
                                    s_name, h_name_comp, h_soc.house, best_legit_name));
            } else {
                log.add(clock.year(), clock.season(), clock.month(), clock.day(),
                        LogCategory::Dynasty,
                        std::format("{} passed; {} succeeded as {} of {}.",
                                    old_name, h_name_comp, old_title, s_name));
            }

            if (h_ch.age < 16) {
                entt::entity picked_regent = entt::null;
                int best_back = -1;
                for (const entt::entity me : registry.get<ecs::HouseholdComponent>(chosen_heir).member_entities) {
                    if (!registry.valid(me) || !registry.all_of<ecs::CharacterComponent, ecs::FamilyComponent>(me)) {
                        continue;
                    }
                    const auto& m_ch = registry.get<ecs::CharacterComponent>(me);
                    const auto& m_fam = registry.get<ecs::FamilyComponent>(me);
                    if (m_ch.alive && m_ch.age >= 16 && m_fam.faction_backing > best_back) {
                        best_back = m_fam.faction_backing;
                        picked_regent = me;
                    }
                }
                if (picked_regent == entt::null) {
                    const std::string reg_first = pickFirstName(config, rng, false);
                    const int reg_age = rng.nextInt(32, 48);
                    const int reg_bm = rng.nextInt(1, 12);
                    const int reg_bd = rng.nextInt(1, 30);
                    const std::string reg_pat = FamilyTree::makePatronymic(
                        pickFirstName(config, rng, false), false, h_soc.house);
                    PersonNode reg_node;
                    reg_node.given_name = reg_first;
                    reg_node.patronymic = reg_pat;
                    reg_node.house = h_soc.house;
                    reg_node.birth_house = h_soc.house;
                    reg_node.title = "Lord Regent";
                    reg_node.seat_name = s_name;
                    reg_node.generation = old_gen;
                    reg_node.alive = true;
                    reg_node.birth_year = clock.year() - reg_age;
                    reg_node.birth_month = reg_bm;
                    reg_node.birth_day = reg_bd;
                    reg_node.current_age = reg_age;
                    reg_node.trait = pickTrait(config, rng);
                    reg_node.mana_capacity = rng.nextRange(25.0, 65.0);
                    const PersonId reg_pid = family_tree.registerPerson(std::move(reg_node));
                    picked_regent = createNobleMember(
                        registry,
                        "Regent " + reg_first + " " + h_soc.house,
                        reg_age,
                        reg_bm,
                        reg_bd,
                        false,
                        "Pragmatic",
                        s_ent,
                        "Lord Regent",
                        h_soc.house,
                        false,
                        DynastyRole::RetainerTutor,
                        chosen_heir,
                        entt::null,
                        entt::null,
                        false,
                        0.60,
                        65,
                        old_rite,
                        35.0,
                        "Water-Guard",
                        SkillRank::Intermediate,
                        "None",
                        SkillRank::None,
                        "Appointed kinsman regent",
                        reg_pid,
                        0,
                        reg_first,
                        reg_pat);
                    if (auto* pmut = family_tree.findMutable(reg_pid)) {
                        pmut->live_entity = picked_regent;
                    }
                    registry.get<ecs::HouseholdComponent>(chosen_heir).member_entities.push_back(picked_regent);
                }
                sett.regent_entity = picked_regent;
                const auto& reg_name = registry.get<ecs::NameComponent>(picked_regent).value;
                log.add(clock.year(), clock.season(), clock.month(), clock.day(),
                        LogCategory::Dynasty,
                        std::format("Regency established in {} for minor ruler {} (age {}) under {}.",
                                    s_name, h_name_comp, h_ch.age, reg_name));
            }
        } else {
            // Collateral kinsman or monastic election
            std::unordered_set<std::string> avoid;
            avoid.insert(FamilyTree::extractGivenName(old_name));
            const std::string new_first = is_bishopric
                ? pickClergyName(config, rng)
                : pickDistinctFirstName(config, rng, false, avoid);
            const std::string new_house = is_bishopric ? pickOtherHouse(config, rng, "") : old_house;
            const int r_num = family_tree.nextRegnalNumber(s_name, new_first);
            const std::string roman = (r_num >= 2) ? (" " + FamilyTree::toRoman(r_num)) : std::string();
            const std::string new_name = (is_bishopric ? std::string("Metropolitan ") : std::string()) +
                                         new_first + roman +
                                         (is_bishopric ? std::string() : (" " + new_house));
            const int new_age = rng.nextInt(26, 44);
            const int new_bmonth = rng.nextInt(1, 12);
            const int new_bday = rng.nextInt(2, 29);
            const std::string new_trait = is_bishopric ? std::string("Ascetic") : pickTrait(config, rng);
            const std::string sword_style = is_bishopric || config.swordStyles().empty()
                ? "None"
                : rng.pick(std::span<const data::SwordStyleDef>(config.swordStyles())).name;
            const double new_mana = is_bishopric ? rng.nextRange(95.0, 155.0) : rng.nextRange(25.0, 75.0);
            const SkillRank new_sword_rank = is_bishopric ? SkillRank::None : SkillRank::Intermediate;
            const std::string new_magic_school = is_bishopric ? std::string("Healing/Restoration") : std::string("None");
            const SkillRank new_magic_rank = is_bishopric ? SkillRank::Advanced : SkillRank::None;
            const std::string new_pat = FamilyTree::makePatronymic(
                pickFirstName(config, rng, false), false, new_house);

            PersonNode s_node;
            s_node.given_name = new_first;
            s_node.patronymic = new_pat;
            s_node.house = new_house;
            s_node.birth_house = new_house;
            s_node.title = old_title;
            s_node.seat_name = s_name;
            s_node.regnal_number = r_num;
            s_node.generation = old_gen + 1;
            s_node.is_female = false;
            s_node.alive = true;
            s_node.was_ruler = true;
            s_node.birth_year = clock.year() - new_age;
            s_node.birth_month = new_bmonth;
            s_node.birth_day = new_bday;
            s_node.current_age = new_age;
            s_node.trait = new_trait;
            s_node.sword_style = sword_style;
            s_node.sword_rank = new_sword_rank;
            s_node.magic_school = new_magic_school;
            s_node.magic_rank = new_magic_rank;
            s_node.mana_capacity = new_mana;
            s_node.predecessor_ruler_id = old_ruler_pid;
            const PersonId suc_pid = family_tree.registerPerson(std::move(s_node));

            const entt::entity successor = createNobleMember(
                registry,
                new_name,
                new_age,
                new_bmonth,
                new_bday,
                false,
                new_trait,
                s_ent,
                old_title,
                new_house,
                is_bishopric,
                DynastyRole::HeadOfHouse,
                entt::null,
                entt::null,
                entt::null,
                false,
                0.45,
                60,
                old_rite,
                new_mana,
                sword_style,
                new_sword_rank,
                new_magic_school,
                new_magic_rank,
                is_bishopric ? "Elected by Northern Synod" : "Collateral House succession",
                suc_pid,
                r_num,
                new_first,
                new_pat);
            if (auto* pmut = family_tree.findMutable(suc_pid)) {
                pmut->live_entity = successor;
            }

            ecs::HouseholdComponent new_hh;
            for (const entt::entity me : old_members) {
                if (new_hh.member_entities.size() >= 4) {
                    break;
                }
                if (registry.valid(me) && registry.all_of<ecs::CharacterComponent, ecs::FamilyComponent>(me) &&
                    registry.get<ecs::CharacterComponent>(me).alive) {
                    auto& m_fam = registry.get<ecs::FamilyComponent>(me);
                    m_fam.head_entity = successor;
                    if (m_fam.role == DynastyRole::LegitimateSpouse || m_fam.role == DynastyRole::Concubine) {
                        m_fam.role = DynastyRole::RetainerTutor;
                        m_fam.pregnancy_months = 0;
                        registry.get<ecs::SocialStatusComponent>(me).title = "Dowager";
                    }
                    new_hh.member_entities.push_back(me);
                }
            }
            registry.emplace<ecs::HouseholdComponent>(successor, std::move(new_hh));
            sett.ruler_entity = successor;
            sett.regent_entity = entt::null;

            log.add(clock.year(), clock.season(), clock.month(), clock.day(),
                    LogCategory::Dynasty,
                    std::format("{} of {} died; {} took office in {}.",
                                old_name, old_house, new_name, s_name));
        }

        // Recompute settlement court mages based on living household
        int court_mages = (s_ent == world_info.capital_entity) ? 3 : (is_bishopric ? 4 : 1);
        const entt::entity cur_ruler = sett.ruler_entity;
        if (cur_ruler != entt::null && registry.valid(cur_ruler)) {
            if (registry.get<ecs::MartialMagicComponent>(cur_ruler).magic_rank >= SkillRank::Intermediate) {
                ++court_mages;
            }
            if (registry.all_of<ecs::HouseholdComponent>(cur_ruler)) {
                for (const entt::entity me : registry.get<ecs::HouseholdComponent>(cur_ruler).member_entities) {
                    if (registry.valid(me) && registry.all_of<ecs::MartialMagicComponent>(me) &&
                        registry.get<ecs::MartialMagicComponent>(me).magic_rank >= SkillRank::Intermediate) {
                        ++court_mages;
                    }
                }
            }
        }
        sett.mages_ready = court_mages;
    }

    // 2b. Regency Usurpations & Rival Claimant Banishments
    for (const entt::entity s_ent : settlements) {
        auto& sett = registry.get<ecs::SettlementComponent>(s_ent);
        if (sett.type == SettlementType::Bishopric || sett.ruler_entity == entt::null ||
            !registry.valid(sett.ruler_entity) ||
            !registry.all_of<ecs::CharacterComponent, ecs::SocialStatusComponent, ecs::FamilyComponent>(sett.ruler_entity)) {
            continue;
        }
        const auto& s_name = registry.get<ecs::NameComponent>(s_ent).value;

        // Check Regency Usurpation for underage rulers
        if (registry.get<ecs::CharacterComponent>(sett.ruler_entity).age < 16 &&
            sett.regent_entity != entt::null && registry.valid(sett.regent_entity) &&
            registry.all_of<ecs::CharacterComponent, ecs::FamilyComponent, ecs::SocialStatusComponent>(sett.regent_entity)) {
            const entt::entity regent = sett.regent_entity;
            const auto& reg_ch = registry.get<ecs::CharacterComponent>(regent);
            const auto& reg_fam = registry.get<ecs::FamilyComponent>(regent);
            if (reg_ch.alive && reg_fam.ambition >= 0.72 && reg_fam.faction_backing >= 68 &&
                rng.nextDouble() < 0.08) {
                const entt::entity ward = sett.ruler_entity;
                auto& ward_fam = registry.get<ecs::FamilyComponent>(ward);
                auto& ward_soc = registry.get<ecs::SocialStatusComponent>(ward);
                const std::string ward_name = registry.get<ecs::NameComponent>(ward).value;
                const std::string seized_title = ward_soc.title;
                const PersonId ward_pid = ward_fam.person_id;

                auto& new_rfam = registry.get<ecs::FamilyComponent>(regent);
                auto& new_rsoc = registry.get<ecs::SocialStatusComponent>(regent);
                auto& new_rname = registry.get<ecs::NameComponent>(regent).value;
                const std::string r_given = !new_rfam.given_name.empty()
                    ? new_rfam.given_name
                    : FamilyTree::extractGivenName(new_rname);
                const int r_num = family_tree.nextRegnalNumber(s_name, r_given);

                new_rfam.role = DynastyRole::HeadOfHouse;
                new_rfam.head_entity = regent;
                new_rfam.regnal_number = r_num;
                new_rsoc.title = seized_title;
                new_rsoc.prestige += 15;
                new_rname = r_given + (r_num >= 2 ? (" " + FamilyTree::toRoman(r_num)) : std::string()) + " " + new_rsoc.house;

                if (PersonNode* rnode = family_tree.findMutable(new_rfam.person_id)) {
                    rnode->was_ruler = true;
                    rnode->seat_name = s_name;
                    rnode->title = seized_title;
                    rnode->regnal_number = r_num;
                    rnode->predecessor_ruler_id = ward_pid;
                }

                ward_fam.role = DynastyRole::LegitimateChild;
                ward_fam.head_entity = regent;
                ward_soc.title = "Deposed Ward";
                if (PersonNode* wnode = family_tree.findMutable(ward_pid)) {
                    wnode->title = "Deposed Ward";
                }

                ecs::HouseholdComponent new_hh;
                new_hh.member_entities.push_back(ward);
                if (registry.all_of<ecs::HouseholdComponent>(ward)) {
                    for (const entt::entity me : registry.get<ecs::HouseholdComponent>(ward).member_entities) {
                        if (new_hh.member_entities.size() >= 4) break;
                        if (me != regent && me != ward && registry.valid(me) &&
                            registry.all_of<ecs::CharacterComponent, ecs::FamilyComponent>(me) &&
                            registry.get<ecs::CharacterComponent>(me).alive) {
                            registry.get<ecs::FamilyComponent>(me).head_entity = regent;
                            new_hh.member_entities.push_back(me);
                        }
                    }
                    registry.remove<ecs::HouseholdComponent>(ward);
                }
                registry.emplace_or_replace<ecs::HouseholdComponent>(regent, std::move(new_hh));
                sett.ruler_entity = regent;
                sett.regent_entity = entt::null;

                if (registry.all_of<ecs::DemographicsComponent>(s_ent)) {
                    for (auto& c : registry.get<ecs::DemographicsComponent>(s_ent).cohorts) {
                        c.unrest = std::min(1.0, c.unrest + 0.10);
                    }
                }
                log.add(clock.year(), clock.season(), clock.month(), clock.day(),
                        LogCategory::Dynasty,
                        std::format("Regent {} usurped {}'s seat at {}!", new_rname, ward_name, s_name));
            }
        }

        // Check Rival Claimant Banishment for adult rulers
        const entt::entity r_ent = sett.ruler_entity;
        if (registry.get<ecs::CharacterComponent>(r_ent).age >= 18 &&
            registry.all_of<ecs::HouseholdComponent>(r_ent)) {
            auto& hh = registry.get<ecs::HouseholdComponent>(r_ent);
            entt::entity banish_target = entt::null;
            for (const entt::entity me : hh.member_entities) {
                if (!registry.valid(me) ||
                    !registry.all_of<ecs::CharacterComponent, ecs::FamilyComponent>(me)) {
                    continue;
                }
                const auto& m_ch = registry.get<ecs::CharacterComponent>(me);
                const auto& m_fam = registry.get<ecs::FamilyComponent>(me);
                if (m_ch.alive && m_ch.age >= 19 &&
                    (m_fam.role == DynastyRole::LegitimateChild || m_fam.role == DynastyRole::BastardChild) &&
                    m_fam.ambition >= 0.75 && m_fam.faction_backing >= 60 &&
                    rng.nextDouble() < 0.06) {
                    banish_target = me;
                    break;
                }
            }
            if (banish_target != entt::null) {
                const auto& r_name = registry.get<ecs::NameComponent>(r_ent).value;
                const auto& b_name = registry.get<ecs::NameComponent>(banish_target).value;
                const PersonId b_pid = registry.get<ecs::FamilyComponent>(banish_target).person_id;
                if (PersonNode* bnode = family_tree.findMutable(b_pid)) {
                    bnode->title = "Exiled Claimant";
                    bnode->seat_name = "Ostmark March";
                    bnode->live_entity = entt::null;
                }
                std::erase(hh.member_entities, banish_target);
                registry.destroy(banish_target);

                if (world_info.rival_duchy_entity != entt::null && registry.valid(world_info.rival_duchy_entity)) {
                    auto& rival = registry.get<ecs::OffMapEmpireComponent>(world_info.rival_duchy_entity);
                    rival.relations = std::max(-100, rival.relations - 4);
                }
                log.add(clock.year(), clock.season(), clock.month(), clock.day(),
                        LogCategory::Dynasty,
                        std::format("{} banished rival claimant {} from {} to the Ostmark marches!",
                                    r_name, b_name, s_name));
            }
        }
    }

    refreshCourtOffices(registry, clock, log);

    // 3. Garbage-collect deceased unreferenced ECS entities; their full ancestry persists in FamilyTree
    std::unordered_set<entt::entity> keep;
    for (const entt::entity se : settlements) {
        const entt::entity re = registry.get<ecs::SettlementComponent>(se).ruler_entity;
        if (re != entt::null) {
            keep.insert(re);
            if (registry.all_of<ecs::HouseholdComponent>(re)) {
                for (const entt::entity me : registry.get<ecs::HouseholdComponent>(re).member_entities) {
                    keep.insert(me);
                }
            }
        }
    }
    for (const entt::entity ce : chars) {
        if (registry.valid(ce) && !registry.get<ecs::CharacterComponent>(ce).alive && !keep.contains(ce)) {
            registry.destroy(ce);
        }
    }

    capTheoremEntities(registry);
}

} // namespace sovereignia::sim
