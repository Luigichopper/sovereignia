#pragma once

#include <array>
#include <cstdint>
#include <span>

namespace sovereignia::core {

/// Deterministic Xoshiro256** pseudo-random generator seeded via SplitMix64.
class Rng {
public:
    explicit Rng(std::uint64_t seed = 482ULL) { reseed(seed); }

    void reseed(std::uint64_t seed) noexcept {
        std::uint64_t z = seed + 0x9E3779B97F4A7C15ULL;
        for (auto& s : state_) {
            z += 0x9E3779B97F4A7C15ULL;
            std::uint64_t x = z;
            x = (x ^ (x >> 30U)) * 0xBF58476D1CE4E5B9ULL;
            x = (x ^ (x >> 27U)) * 0x94D049BB133111EBULL;
            s = x ^ (x >> 31U);
        }
        if (state_[0] == 0 && state_[1] == 0 && state_[2] == 0 && state_[3] == 0) {
            state_[0] = 0x123456789ABCDEF0ULL;
        }
    }

    std::uint64_t nextU64() noexcept {
        const std::uint64_t result = rotl(state_[1] * 5ULL, 7) * 9ULL;
        const std::uint64_t t = state_[1] << 17U;
        state_[2] ^= state_[0];
        state_[3] ^= state_[1];
        state_[1] ^= state_[2];
        state_[0] ^= state_[3];
        state_[2] ^= t;
        state_[3] = rotl(state_[3], 45);
        return result;
    }

    /// Returns a double in [0.0, 1.0).
    double nextDouble() noexcept {
        constexpr double inv = 1.0 / static_cast<double>(1ULL << 53U);
        return static_cast<double>(nextU64() >> 11U) * inv;
    }

    /// Returns a double in [min_val, max_val).
    double nextRange(double min_val, double max_val) noexcept {
        return min_val + nextDouble() * (max_val - min_val);
    }

    /// Returns an inclusive integer in [min_val, max_val].
    int nextInt(int min_val, int max_val) noexcept {
        if (max_val <= min_val) {
            return min_val;
        }
        const auto span = static_cast<std::uint64_t>(max_val - min_val) + 1ULL;
        return min_val + static_cast<int>(nextU64() % span);
    }

    template <typename T>
    const T& pick(std::span<const T> items) noexcept {
        const std::size_t idx = static_cast<std::size_t>(nextU64() % items.size());
        return items[idx];
    }

private:
    static constexpr std::uint64_t rotl(std::uint64_t x, int k) noexcept {
        return (x << k) | (x >> (64 - k));
    }

    std::array<std::uint64_t, 4> state_{};
};

} // namespace sovereignia::core

