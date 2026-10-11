#pragma once

#include <engine/project/wind_project.h>

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace launcher {

// One remembered project as the launcher shows it: its wind_project.toml, or why it cannot be read (moved,
// deleted, broken).
struct ProjectEntry {
    std::filesystem::path directory;
    std::optional<engine::WindProject> project;
    std::string problem;
};

[[nodiscard]] ProjectEntry read_project_entry(const std::filesystem::path& directory);

// Writes `engine = "<version>"` into `<directory>/wind_project.toml`: replaces the value of the `engine` key in place, or
// adds the key at the end when there is none. Everything else in the file, comments and line endings included, stays.
[[nodiscard]] std::expected<void, std::string> set_project_engine(
        const std::filesystem::path& directory, std::string_view version);

}
