#include "json.h"

#include <cmath>
#include <cstdlib>

namespace launcher {
namespace {

constexpr int kMaxDepth = 64;

class Parser {
public:
    explicit Parser(std::string_view text)
        : text_(text) {}

    std::expected<JsonValue, std::string> parse() {
        skip_space();
        JsonValue value;
        if (!parse_value(value, 0)) {
            return std::unexpected(error_);
        }
        skip_space();
        if (pos_ != text_.size()) {
            fail("unexpected text after the value");
            return std::unexpected(error_);
        }
        return value;
    }

private:
    bool fail(const std::string& what) {
        if (error_.empty()) {
            error_ = "JSON: " + what + " at byte " + std::to_string(pos_);
        }
        return false;
    }

    void skip_space() {
        while (pos_ < text_.size() &&
                (text_[pos_] == ' ' || text_[pos_] == '\t' || text_[pos_] == '\n' || text_[pos_] == '\r')) {
            ++pos_;
        }
    }

    bool literal(std::string_view word) {
        if (text_.substr(pos_, word.size()) != word) {
            return fail("invalid value");
        }
        pos_ += word.size();
        return true;
    }

    bool parse_value(JsonValue& out, int depth) {
        if (depth > kMaxDepth) {
            return fail("nested too deep");
        }
        if (pos_ >= text_.size()) {
            return fail("unexpected end");
        }
        switch (text_[pos_]) {
            case '{':
                return parse_object(out, depth);
            case '[':
                return parse_array(out, depth);
            case '"':
                out.kind = JsonValue::Kind::String;
                return parse_string(out.string);
            case 't':
                out.kind = JsonValue::Kind::Bool;
                out.boolean = true;
                return literal("true");
            case 'f':
                out.kind = JsonValue::Kind::Bool;
                out.boolean = false;
                return literal("false");
            case 'n':
                out.kind = JsonValue::Kind::Null;
                return literal("null");
            default:
                return parse_number(out);
        }
    }

    bool parse_number(JsonValue& out) {
        const std::size_t start = pos_;
        if (pos_ < text_.size() && text_[pos_] == '-') {
            ++pos_;
        }
        const auto digits = [&] {
            const std::size_t before = pos_;
            while (pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9') {
                ++pos_;
            }
            return pos_ > before;
        };
        if (pos_ < text_.size() && text_[pos_] == '0') {
            ++pos_;
        } else if (!digits()) {
            pos_ = start;
            return fail("invalid value");
        }
        if (pos_ < text_.size() && text_[pos_] == '.') {
            ++pos_;
            if (!digits()) {
                return fail("invalid number");
            }
        }
        if (pos_ < text_.size() && (text_[pos_] == 'e' || text_[pos_] == 'E')) {
            ++pos_;
            if (pos_ < text_.size() && (text_[pos_] == '+' || text_[pos_] == '-')) {
                ++pos_;
            }
            if (!digits()) {
                return fail("invalid number");
            }
        }
        const std::string copy(text_.substr(start, pos_ - start));
        out.kind = JsonValue::Kind::Number;
        out.number = std::strtod(copy.c_str(), nullptr);
        if (!std::isfinite(out.number)) {
            return fail("number out of range");
        }
        return true;
    }

    static void append_utf8(std::string& out, unsigned code) {
        if (code < 0x80) {
            out += static_cast<char>(code);
        } else if (code < 0x800) {
            out += static_cast<char>(0xc0 | (code >> 6));
            out += static_cast<char>(0x80 | (code & 0x3f));
        } else if (code < 0x10000) {
            out += static_cast<char>(0xe0 | (code >> 12));
            out += static_cast<char>(0x80 | ((code >> 6) & 0x3f));
            out += static_cast<char>(0x80 | (code & 0x3f));
        } else {
            out += static_cast<char>(0xf0 | (code >> 18));
            out += static_cast<char>(0x80 | ((code >> 12) & 0x3f));
            out += static_cast<char>(0x80 | ((code >> 6) & 0x3f));
            out += static_cast<char>(0x80 | (code & 0x3f));
        }
    }

    bool hex4(unsigned& code) {
        if (pos_ + 4 > text_.size()) {
            return fail("short \\u escape");
        }
        code = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = text_[pos_++];
            code <<= 4;
            if (c >= '0' && c <= '9') {
                code |= static_cast<unsigned>(c - '0');
            } else if (c >= 'a' && c <= 'f') {
                code |= static_cast<unsigned>(c - 'a' + 10);
            } else if (c >= 'A' && c <= 'F') {
                code |= static_cast<unsigned>(c - 'A' + 10);
            } else {
                return fail("bad \\u escape");
            }
        }
        return true;
    }

    bool parse_string(std::string& out) {
        out.clear();
        ++pos_;
        while (pos_ < text_.size()) {
            const char c = text_[pos_++];
            if (c == '"') {
                return true;
            }
            if (static_cast<unsigned char>(c) < 0x20) {
                return fail("control character in a string");
            }
            if (c != '\\') {
                out += c;
                continue;
            }
            if (pos_ >= text_.size()) {
                break;
            }
            const char escape = text_[pos_++];
            switch (escape) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    unsigned code = 0;
                    if (!hex4(code)) {
                        return false;
                    }
                    if (code >= 0xd800 && code <= 0xdbff) {
                        unsigned low = 0;
                        if (text_.substr(pos_, 2) != "\\u") {
                            return fail("lone surrogate");
                        }
                        pos_ += 2;
                        if (!hex4(low)) {
                            return false;
                        }
                        if (low < 0xdc00 || low > 0xdfff) {
                            return fail("lone surrogate");
                        }
                        code = 0x10000 + ((code - 0xd800) << 10) + (low - 0xdc00);
                    } else if (code >= 0xdc00 && code <= 0xdfff) {
                        return fail("lone surrogate");
                    }
                    append_utf8(out, code);
                    break;
                }
                default:
                    return fail("bad escape");
            }
        }
        return fail("unterminated string");
    }

    bool parse_array(JsonValue& out, int depth) {
        out.kind = JsonValue::Kind::Array;
        ++pos_;
        skip_space();
        if (pos_ < text_.size() && text_[pos_] == ']') {
            ++pos_;
            return true;
        }
        while (true) {
            skip_space();
            JsonValue element;
            if (!parse_value(element, depth + 1)) {
                return false;
            }
            out.array.push_back(std::move(element));
            skip_space();
            if (pos_ < text_.size() && text_[pos_] == ',') {
                ++pos_;
                continue;
            }
            if (pos_ < text_.size() && text_[pos_] == ']') {
                ++pos_;
                return true;
            }
            return fail("expected , or ]");
        }
    }

    bool parse_object(JsonValue& out, int depth) {
        out.kind = JsonValue::Kind::Object;
        ++pos_;
        skip_space();
        if (pos_ < text_.size() && text_[pos_] == '}') {
            ++pos_;
            return true;
        }
        while (true) {
            skip_space();
            if (pos_ >= text_.size() || text_[pos_] != '"') {
                return fail("expected a member name");
            }
            std::string key;
            if (!parse_string(key)) {
                return false;
            }
            skip_space();
            if (pos_ >= text_.size() || text_[pos_] != ':') {
                return fail("expected :");
            }
            ++pos_;
            skip_space();
            JsonValue member;
            if (!parse_value(member, depth + 1)) {
                return false;
            }
            out.object.emplace_back(std::move(key), std::move(member));
            skip_space();
            if (pos_ < text_.size() && text_[pos_] == ',') {
                ++pos_;
                continue;
            }
            if (pos_ < text_.size() && text_[pos_] == '}') {
                ++pos_;
                return true;
            }
            return fail("expected , or }");
        }
    }

    std::string_view text_;
    std::size_t pos_ = 0;
    std::string error_;
};

}

const JsonValue* JsonValue::find(std::string_view key) const {
    if (kind != Kind::Object) {
        return nullptr;
    }
    for (const auto& member : object) {
        if (member.first == key) {
            return &member.second;
        }
    }
    return nullptr;
}

std::expected<JsonValue, std::string> parse_json(std::string_view text) {
    return Parser(text).parse();
}

}
