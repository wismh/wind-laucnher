#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace launcher {

// SHA-256 (FIPS 180-4), written out so that no hashing tool has to be on the machine.
class Sha256 {
public:
    Sha256();

    void update(const void* data, std::size_t size);
    void update(std::string_view data) {
        update(data.data(), data.size());
    }
    // The digest as 64 lowercase hex digits. The object is finished after this; do not update it again.
    [[nodiscard]] std::string finish_hex();

private:
    void compress(const unsigned char* block);

    std::array<std::uint32_t, 8> state_;
    std::array<unsigned char, 64> buffer_{};
    std::size_t buffered_ = 0;
    std::uint64_t total_ = 0;
};

[[nodiscard]] std::string sha256_hex(std::string_view data);

// Hashes a file a slice per call, so a 130 MB archive never holds a frame: step() in every frame until it says done.
class FileHasher {
public:
    explicit FileHasher(const std::filesystem::path& path);

    // Reads at most `budget` bytes. True when the file is done or could not be read.
    bool step(std::size_t budget);
    [[nodiscard]] bool failed() const {
        return failed_;
    }
    // The digest once step() returned true and !failed().
    [[nodiscard]] const std::string& hex() const {
        return hex_;
    }

private:
    std::ifstream in_;
    Sha256 hash_;
    std::string hex_;
    bool failed_ = false;
};

// The hash in a `<hex>  <name>` or `<hex> *<name>` line (sha256sum, text and binary mode), lowercased, or an empty
// string when the text is not one. The name is not checked: the file is the hash of what was downloaded under it.
[[nodiscard]] std::string parse_checksum_file(std::string_view text);

}
