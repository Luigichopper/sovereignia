#include "sovereignia/sim/family_tree.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <utility>

namespace sovereignia::sim {

namespace {

void appendUnique(std::vector<PersonId>& vec, PersonId id) {
    if (id == kInvalidPersonId) {
        return;
    }
    if (std::find(vec.begin(), vec.end(), id) == vec.end()) {
        vec.push_back(id);
    }
}

bool isSlavicHouse(std::string_view house) noexcept {
    static constexpr std::array<std::string_view, 5> kSlavic{
        "Vaelen", "Morozov", "Korsakov", "Vorontsov", "Rostov"
    };
    for (const auto prefix : kSlavic) {
        if (house == prefix || house.starts_with(std::string(prefix) + "-")) {
            return true;
        }
    }
    return false;
}

} // namespace

void FamilyTree::clear() noexcept {
    nodes_.clear();
}

PersonId FamilyTree::registerPerson(PersonNode node) {
    const PersonId id = static_cast<PersonId>(nodes_.size() + 1);
    node.id = id;
    nodes_.push_back(std::move(node));
    return id;
}

PersonNode* FamilyTree::findMutable(PersonId id) noexcept {
    if (id == kInvalidPersonId || static_cast<std::size_t>(id) > nodes_.size()) {
        return nullptr;
    }
    return &nodes_[static_cast<std::size_t>(id) - 1];
}

const PersonNode* FamilyTree::find(PersonId id) const noexcept {
    if (id == kInvalidPersonId || static_cast<std::size_t>(id) > nodes_.size()) {
        return nullptr;
    }
    return &nodes_[static_cast<std::size_t>(id) - 1];
}

void FamilyTree::addSpouse(PersonId a, PersonId b) {
    if (a == kInvalidPersonId || b == kInvalidPersonId || a == b) {
        return;
    }
    if (auto* na = findMutable(a)) {
        appendUnique(na->spouse_ids, b);
    }
    if (auto* nb = findMutable(b)) {
        appendUnique(nb->spouse_ids, a);
    }
}

void FamilyTree::addConcubine(PersonId ruler, PersonId concubine) {
    if (ruler == kInvalidPersonId || concubine == kInvalidPersonId || ruler == concubine) {
        return;
    }
    if (auto* nr = findMutable(ruler)) {
        appendUnique(nr->concubine_ids, concubine);
    }
    if (auto* nc = findMutable(concubine)) {
        appendUnique(nc->spouse_ids, ruler);
    }
}

void FamilyTree::addChild(PersonId father, PersonId mother, PersonId child) {
    if (child == kInvalidPersonId) {
        return;
    }
    if (auto* nchild = findMutable(child)) {
        if (father != kInvalidPersonId) {
            nchild->father_id = father;
        }
        if (mother != kInvalidPersonId) {
            nchild->mother_id = mother;
        }
    }
    if (auto* nf = findMutable(father)) {
        appendUnique(nf->children_ids, child);
    }
    if (auto* nm = findMutable(mother)) {
        appendUnique(nm->children_ids, child);
    }
}

int FamilyTree::nextRegnalNumber(std::string_view seat_name, std::string_view given_name) const {
    if (given_name.empty()) {
        return 1;
    }
    int count = 0;
    for (const auto& node : nodes_) {
        if (node.was_ruler && node.seat_name == seat_name && node.given_name == given_name) {
            count = std::max(count, std::max(1, node.regnal_number));
        }
    }
    return count + 1;
}

std::vector<PersonId> FamilyTree::ancestorChain(PersonId id, std::size_t max_depth) const {
    std::vector<PersonId> chain;
    std::vector<PersonId> visited;
    visited.push_back(id);

    const PersonNode* cur = find(id);
    while (cur != nullptr && chain.size() < max_depth) {
        PersonId next = kInvalidPersonId;
        if (cur->father_id != kInvalidPersonId) {
            next = cur->father_id;
        } else if (cur->mother_id != kInvalidPersonId) {
            next = cur->mother_id;
        } else if (cur->predecessor_ruler_id != kInvalidPersonId) {
            next = cur->predecessor_ruler_id;
        }
        if (next == kInvalidPersonId ||
            std::find(visited.begin(), visited.end(), next) != visited.end()) {
            break;
        }
        visited.push_back(next);
        chain.push_back(next);
        cur = find(next);
    }

    std::reverse(chain.begin(), chain.end());
    return chain;
}

std::vector<PersonId> FamilyTree::siblingsOf(PersonId id) const {
    std::vector<PersonId> sibs;
    const PersonNode* self = find(id);
    if (self == nullptr) {
        return sibs;
    }
    if (const PersonNode* dad = find(self->father_id)) {
        for (const PersonId cid : dad->children_ids) {
            if (cid != id) {
                appendUnique(sibs, cid);
            }
        }
    }
    if (const PersonNode* mom = find(self->mother_id)) {
        for (const PersonId cid : mom->children_ids) {
            if (cid != id) {
                appendUnique(sibs, cid);
            }
        }
    }
    return sibs;
}

int FamilyTree::houseTotalRulers(std::string_view house) const noexcept {
    int count = 0;
    for (const auto& node : nodes_) {
        if (node.was_ruler && node.house == house) {
            ++count;
        }
    }
    return count;
}

int FamilyTree::houseTotalMembers(std::string_view house) const noexcept {
    int count = 0;
    for (const auto& node : nodes_) {
        if (node.house == house || node.birth_house == house) {
            ++count;
        }
    }
    return count;
}

std::string FamilyTree::formatDisplayName(PersonId id, bool include_patronymic) const {
    const PersonNode* node = find(id);
    if (node == nullptr) {
        return "Unknown";
    }
    std::string out = node->given_name.empty() ? std::string("Unknown") : node->given_name;
    if (node->was_ruler && node->regnal_number >= 2) {
        out += " " + toRoman(node->regnal_number);
    } else if (include_patronymic && !node->patronymic.empty()) {
        out += " " + node->patronymic;
    }
    if (!node->house.empty() && !node->is_bastard) {
        out += " " + node->house;
    } else if (node->is_bastard && !node->house.empty()) {
        out += " (" + node->house + ")";
    }
    if (!node->epithet.empty()) {
        out += " '" + node->epithet + "'";
    }
    return out;
}

std::string FamilyTree::formatBirthDate(PersonId id) const {
    const PersonNode* node = find(id);
    if (node == nullptr) {
        return "b.?";
    }
    return std::format("b.{} M{} D{}", node->birth_year, node->birth_month, node->birth_day);
}

std::string FamilyTree::toRoman(int number) {
    if (number <= 0) {
        return "";
    }
    static constexpr std::array<std::pair<int, std::string_view>, 9> kTable{{
        {100, "C"}, {90, "XC"}, {50, "L"}, {40, "XL"},
        {10, "X"}, {9, "IX"}, {5, "V"}, {4, "IV"}, {1, "I"}
    }};
    std::string out;
    int rem = number;
    for (const auto& [val, sym] : kTable) {
        while (rem >= val) {
            out += sym;
            rem -= val;
        }
    }
    return out;
}

std::string FamilyTree::makePatronymic(std::string_view father_given, bool is_female, std::string_view house) {
    if (father_given.empty()) {
        return {};
    }
    std::string root(father_given);
    if (isSlavicHouse(house)) {
        const char back = root.back();
        if (back == 'i' || back == 'y') {
            root.pop_back();
            return root + (is_female ? "evna" : "evich");
        }
        if (back == 'a' || back == 'e') {
            root.pop_back();
            return root + (is_female ? "ichna" : "ich");
        }
        return root + (is_female ? "ovna" : "ovich");
    }

    if (root.back() == 's' || root.back() == 'S') {
        return root + (is_female ? "dottir" : "son");
    }
    return root + (is_female ? "sdottir" : "sson");
}

std::string FamilyTree::extractGivenName(std::string_view full_name) {
    static constexpr std::array<std::string_view, 14> kPrefixes{{
        "Forester-Lord ",
        "Metropolitan ",
        "Archimandrite ",
        "Wood-Reeve ",
        "Castellan ",
        "Baroness ",
        "Margrave ",
        "Alderman ",
        "Consort ",
        "Bailiff ",
        "Brother ",
        "Father ",
        "Sister ",
        "Lady "
    }};
    static constexpr std::array<std::string_view, 3> kShortPrefixes{{
        "Lord ",
        "Baron ",
        "Ser "
    }};

    std::string_view view = full_name;
    bool stripped = true;
    while (stripped) {
        stripped = false;
        for (const auto p : kPrefixes) {
            if (view.starts_with(p)) {
                view.remove_prefix(p.size());
                stripped = true;
                break;
            }
        }
        if (!stripped) {
            for (const auto p : kShortPrefixes) {
                if (view.starts_with(p)) {
                    view.remove_prefix(p.size());
                    stripped = true;
                    break;
                }
            }
        }
    }
    while (!view.empty() && view.front() == ' ') {
        view.remove_prefix(1);
    }
    const auto sp = view.find(' ');
    if (sp != std::string_view::npos) {
        view = view.substr(0, sp);
    }
    return std::string(view);
}

} // namespace sovereignia::sim

