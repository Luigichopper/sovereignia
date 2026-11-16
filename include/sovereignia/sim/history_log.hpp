#pragma once

#include "sovereignia/core/types.hpp"

#include <cstddef>
#include <deque>
#include <string>
#include <vector>

namespace sovereignia::sim {

struct ChronicleEntry {
    int year = 480;
    Season season = Season::Spring;
    int month = 1;
    int day = 1;
    LogCategory category = LogCategory::Economy;
    std::string message;
};

class HistoryLog {
public:
    static constexpr std::size_t kMaxEntries = 800;

    void add(int year,
             Season season,
             int month,
             int day,
             LogCategory category,
             std::string message);

    const std::deque<ChronicleEntry>& entries() const noexcept { return entries_; }

    std::vector<ChronicleEntry> recent(std::size_t count) const;
    std::vector<ChronicleEntry> recentFiltered(LogCategory category, std::size_t count) const;
    std::vector<ChronicleEntry> slice(std::size_t count,
                                      std::size_t scroll_offset,
                                      int category_filter = -1) const;

    void clear() noexcept { entries_.clear(); }

private:
    std::deque<ChronicleEntry> entries_;
};

} // namespace sovereignia::sim

