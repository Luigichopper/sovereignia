#pragma once

#include <filesystem>
#include <string_view>

namespace sovereignia::core {

/// Locates the data directory safely using UTF-16 executable paths on Windows.
std::filesystem::path resolveDataDir();

/// Resolves a relative filename inside the project data directory.
std::filesystem::path resolveDataPath(std::string_view filename);

} // namespace sovereignia::core

