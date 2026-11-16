#include "sovereignia/core/paths.hpp"

#include <array>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace sovereignia::core {

namespace {

std::filesystem::path executableDirectory() {
#ifdef _WIN32
    std::wstring buffer(32768, L'\0');
    DWORD len = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (len > 0 && len < buffer.size()) {
        buffer.resize(len);
        return std::filesystem::path(buffer).parent_path();
    }
#endif
    return std::filesystem::current_path();
}

} // namespace

std::filesystem::path resolveDataDir() {
    const std::filesystem::path cwd = std::filesystem::current_path();
    const std::filesystem::path exe_dir = executableDirectory();

    const std::array<std::filesystem::path, 6> candidates = {
        cwd / "data",
        exe_dir / "data",
        exe_dir.parent_path() / "data",
        exe_dir.parent_path().parent_path() / "data",
        cwd.parent_path() / "data",
        exe_dir.parent_path().parent_path().parent_path() / "data"
    };

    for (const auto& candidate : candidates) {
        std::error_code ec;
        if (std::filesystem::exists(candidate, ec) &&
            std::filesystem::is_directory(candidate, ec) &&
            std::filesystem::exists(candidate / "biomes.json", ec)) {
            return std::filesystem::weakly_canonical(candidate, ec);
        }
    }

    throw std::runtime_error("Unable to locate Sovereignia data/ directory containing biomes.json");
}

std::filesystem::path resolveDataPath(std::string_view filename) {
    const std::filesystem::path dir = resolveDataDir();
    const std::filesystem::path target = dir / std::filesystem::path(filename);
    std::error_code ec;
    if (!std::filesystem::exists(target, ec)) {
        throw std::runtime_error("Missing data file: " + std::string(filename));
    }
    return target;
}

} // namespace sovereignia::core

