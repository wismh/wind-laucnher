#include "project_entry.h"

#include <fstream>
#include <iterator>
#include <utility>

namespace launcher {
namespace {

bool is_blank(char c) {
    return c == ' ' || c == '\t';
}

}

ProjectEntry read_project_entry(const std::filesystem::path& directory) {
    ProjectEntry entry{.directory = directory, .project = std::nullopt, .problem = {}};
    if (auto project = engine::read_wind_project(directory)) {
        entry.project = std::move(*project);
    } else {
        entry.problem = engine::describe(project.error());
    }
    return entry;
}

std::expected<void, std::string> set_project_engine(const std::filesystem::path& directory, std::string_view version) {
    const std::filesystem::path file = directory / engine::kWindProjectFile;
    std::string text;
    {
        std::ifstream in(file, std::ios::binary);
        if (!in) {
            return std::unexpected("Cannot read " + file.generic_string() + ".");
        }
        text.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }
    const std::string value = "engine = \"" + std::string(version) + "\"";

    bool replaced = false;
    for (std::size_t start = 0; start < text.size() && !replaced;) {
        std::size_t end = text.find('\n', start);
        if (end == std::string::npos) {
            end = text.size();
        }
        std::size_t key = start;
        while (key < end && is_blank(text[key])) {
            ++key;
        }
        // The key `engine`, not `engine_note`: the name is followed by blanks and `=`.
        std::size_t after = key + 6;
        if (text.compare(key, 6, "engine") == 0) {
            while (after < end && is_blank(text[after])) {
                ++after;
            }
            if (after < end && text[after] == '=') {
                // The key and its quoted value; a trailing comment and the line ending stay.
                const std::size_t quote = text.find('"', after);
                const std::size_t closing = quote < end ? text.find('"', quote + 1) : std::string::npos;
                if (closing < end) {
                    text.replace(key, closing + 1 - key, value);
                    replaced = true;
                }
            }
        }
        start = end + 1;
    }
    if (!replaced) {
        if (!text.empty() && text.back() != '\n') {
            text += '\n';
        }
        text += value + "\n";
    }

    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    if (!out || !(out << text)) {
        return std::unexpected("Cannot write " + file.generic_string() + ".");
    }
    return {};
}

}
