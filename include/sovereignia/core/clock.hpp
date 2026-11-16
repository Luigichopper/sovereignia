#pragma once

#include "sovereignia/core/types.hpp"

#include <cstdint>
#include <string>

namespace sovereignia::core {

struct TickFlags {
    bool daily = true;
    bool monthly = false;
    bool seasonal = false;
    bool yearly = false;
};

/// Four-frequency simulation calendar: 30 days/month, 3 months/season, 4 seasons/year.
class SimClock {
public:
    static constexpr int kDaysPerMonth = 30;
    static constexpr int kMonthsPerSeason = 3;
    static constexpr int kMonthsPerYear = 12;
    static constexpr int kDaysPerYear = kDaysPerMonth * kMonthsPerYear;

    explicit SimClock(int start_year = 480) noexcept : year_(start_year) {}

    void reset(int start_year = 480) noexcept {
        year_ = start_year;
        month_ = 1;
        day_ = 1;
        total_days_ = 0;
    }

    TickFlags advanceDay() noexcept;

    int year() const noexcept { return year_; }
    int month() const noexcept { return month_; }
    int day() const noexcept { return day_; }
    std::uint64_t totalDays() const noexcept { return total_days_; }

    Season season() const noexcept {
        const int idx = (month_ - 1) / kMonthsPerSeason;
        return static_cast<Season>(idx);
    }

    std::string formatFullDate() const;
    std::string formatCompactDate() const;

private:
    int year_ = 480;
    int month_ = 1; // 1..12
    int day_ = 1;   // 1..30
    std::uint64_t total_days_ = 0;
};

} // namespace sovereignia::core

