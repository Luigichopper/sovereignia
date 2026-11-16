#include "sovereignia/core/clock.hpp"

#include <format>

namespace sovereignia::core {

TickFlags SimClock::advanceDay() noexcept {
    TickFlags flags{};
    flags.daily = true;
    ++total_days_;
    ++day_;

    if (day_ > kDaysPerMonth) {
        day_ = 1;
        ++month_;
        flags.monthly = true;

        if (((month_ - 1) % kMonthsPerSeason) == 0) {
            flags.seasonal = true;
        }

        if (month_ > kMonthsPerYear) {
            month_ = 1;
            ++year_;
            flags.yearly = true;
        }
    }

    return flags;
}

std::string SimClock::formatFullDate() const {
    return std::format("Year {}, {} (M{} D{})", year_, toString(season()), month_, day_);
}

std::string SimClock::formatCompactDate() const {
    return std::format("[{}, {}]", year_, toString(season()));
}

} // namespace sovereignia::core

