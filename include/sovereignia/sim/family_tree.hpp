#pragma once

#include "sovereignia/core/types.hpp"

#include <entt/entt.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace sovereignia::sim {

using PersonId = std::uint32_t;
inline constexpr PersonId kInvalidPersonId = 0;

struct PersonNode {
    PersonId id = kInvalidPersonId;
    entt::entity live_entity = entt::null;
    std::string given_name;
    std::string patronymic;
    std::string house;
    std::string birth_house;
    std::string title;
    std::string epithet;
    std::string seat_name;
    int regnal_number = 0;
    int generation = 1;
    bool is_female = false;
    bool alive = true;
    bool is_bastard = false;
    bool was_ruler = false;
    bool bastard_coup_founder = false;
    int birth_year = 440;
    int birth_month = 1; // 1..12
    int birth_day = 1;   // 1..30
    int current_age = 35;
    int death_year = 0;
    int age_at_death = 0;
    std::string trait;
    std::string sword_style = "None";
    SkillRank sword_rank = SkillRank::None;
    std::string magic_school = "None";
    SkillRank magic_rank = SkillRank::None;
    double mana_capacity = 20.0;
    bool incantationless = false;
    PersonId father_id = kInvalidPersonId;
    PersonId mother_id = kInvalidPersonId;
    PersonId predecessor_ruler_id = kInvalidPersonId;
    std::vector<PersonId> spouse_ids;
    std::vector<PersonId> concubine_ids;
    std::vector<PersonId> children_ids;
};

class FamilyTree {
public:
    void clear() noexcept;

    PersonId registerPerson(PersonNode node);
    PersonNode* findMutable(PersonId id) noexcept;
    const PersonNode* find(PersonId id) const noexcept;

    std::size_t totalPersons() const noexcept { return nodes_.size(); }
    const std::vector<PersonNode>& nodes() const noexcept { return nodes_; }

    void addSpouse(PersonId a, PersonId b);
    void addConcubine(PersonId ruler, PersonId concubine);
    void addChild(PersonId father, PersonId mother, PersonId child);

    int nextRegnalNumber(std::string_view seat_name, std::string_view given_name) const;
    std::vector<PersonId> ancestorChain(PersonId id, std::size_t max_depth = 12) const;
    std::vector<PersonId> siblingsOf(PersonId id) const;

    int houseTotalRulers(std::string_view house) const noexcept;
    int houseTotalMembers(std::string_view house) const noexcept;

    std::string formatDisplayName(PersonId id, bool include_patronymic = false) const;
    std::string formatBirthDate(PersonId id) const;

    static std::string toRoman(int number);
    static std::string makePatronymic(std::string_view father_given, bool is_female, std::string_view house);
    static std::string extractGivenName(std::string_view full_name);

private:
    std::vector<PersonNode> nodes_;
};

} // namespace sovereignia::sim

