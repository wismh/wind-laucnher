#pragma once

#include <expected>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace launcher {

// A parsed JSON document, enough to read the GitHub API: values by kind, objects as an ordered list of members.
struct JsonValue {
    enum class Kind {
        Null,
        Bool,
        Number,
        String,
        Array,
        Object,
    };

    Kind kind = Kind::Null;
    bool boolean = false;
    double number = 0.0;
    std::string string;
    std::vector<JsonValue> array;
    std::vector<std::pair<std::string, JsonValue>> object;

    // The member `key` of an object (the first one of that name), or nullptr for another kind or a missing key.
    [[nodiscard]] const JsonValue* find(std::string_view key) const;
    [[nodiscard]] bool is(Kind other) const {
        return kind == other;
    }
};

// RFC 8259. The error names the byte offset. Nesting deeper than 64 levels, a number that is not finite, text after
// the value, and invalid UTF-8 escapes (a lone surrogate) are errors.
[[nodiscard]] std::expected<JsonValue, std::string> parse_json(std::string_view text);

}
