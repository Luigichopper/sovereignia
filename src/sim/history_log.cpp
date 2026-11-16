#include "sovereignia/sim/history_log.hpp"

namespace sovereignia::sim {

void HistoryLog::add(int year,
                     Season season,
                     int month,
                     int day,
                     LogCategory category,
                     std::string message) {
    if (entries_.size() >= kMaxEntries) {
        entries_.pop_front();
    }
    entries_.push_back(ChronicleEntry{
        year,
        season,
        month,
        day,
        category,
        std::move(message)
    });
}

std::vector<ChronicleEntry> HistoryLog::recent(std::size_t count) const {
    std::vector<ChronicleEntry> out;
    if (entries_.empty() || count == 0) {
        return out;
    }
    const std::size_t n = std::min(count, entries_.size());
    out.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        out.push_back(entries_[entries_.size() - 1 - i]);
    }
    return out;
}

std::vector<ChronicleEntry> HistoryLog::recentFiltered(LogCategory category, std::size_t count) const {
    return slice(count, 0, static_cast<int>(category));
}

std::vector<ChronicleEntry> HistoryLog::slice(std::size_t count,
                                              std::size_t scroll_offset,
                                              int category_filter) const {
    std::vector<ChronicleEntry> out;
    if (entries_.empty() || count == 0) {
        return out;
    }
    out.reserve(count);
    std::size_t skipped = 0;
    for (auto it = entries_.rbegin(); it != entries_.rend() && out.size() < count; ++it) {
        if (category_filter >= 0 && static_cast<int>(it->category) != category_filter) {
            continue;
        }
        if (skipped < scroll_offset) {
            ++skipped;
            continue;
        }
        out.push_back(*it);
    }
    return out;
}

} // namespace sovereignia::sim

