#include "sha256.h"

#include <algorithm>
#include <cctype>
#include <vector>

namespace launcher {
namespace {

constexpr std::array<std::uint32_t, 64> kRound = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98,
        0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
        0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8,
        0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
        0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819,
        0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
        0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7,
        0xc67178f2};

constexpr std::uint32_t rotr(std::uint32_t value, unsigned bits) {
    return (value >> bits) | (value << (32 - bits));
}

}

Sha256::Sha256()
    : state_{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19} {}

void Sha256::compress(const unsigned char* block) {
    std::array<std::uint32_t, 64> w{};
    for (std::size_t i = 0; i < 16; ++i) {
        w[i] = (std::uint32_t(block[i * 4]) << 24) | (std::uint32_t(block[i * 4 + 1]) << 16) |
               (std::uint32_t(block[i * 4 + 2]) << 8) | std::uint32_t(block[i * 4 + 3]);
    }
    for (std::size_t i = 16; i < 64; ++i) {
        const std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    std::uint32_t a = state_[0], b = state_[1], c = state_[2], d = state_[3];
    std::uint32_t e = state_[4], f = state_[5], g = state_[6], h = state_[7];
    for (std::size_t i = 0; i < 64; ++i) {
        const std::uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        const std::uint32_t choice = (e & f) ^ (~e & g);
        const std::uint32_t t1 = h + s1 + choice + kRound[i] + w[i];
        const std::uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        const std::uint32_t t2 = s0 + majority;
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
    state_[4] += e;
    state_[5] += f;
    state_[6] += g;
    state_[7] += h;
}

void Sha256::update(const void* data, std::size_t size) {
    const auto* bytes = static_cast<const unsigned char*>(data);
    total_ += size;
    if (buffered_ > 0) {
        const std::size_t take = std::min(size, buffer_.size() - buffered_);
        std::copy_n(bytes, take, buffer_.begin() + static_cast<std::ptrdiff_t>(buffered_));
        buffered_ += take;
        bytes += take;
        size -= take;
        if (buffered_ == buffer_.size()) {
            compress(buffer_.data());
            buffered_ = 0;
        }
    }
    while (size >= buffer_.size()) {
        compress(bytes);
        bytes += buffer_.size();
        size -= buffer_.size();
    }
    if (size > 0) {
        std::copy_n(bytes, size, buffer_.begin());
        buffered_ = size;
    }
}

std::string Sha256::finish_hex() {
    const std::uint64_t bits = total_ * 8;
    const unsigned char one = 0x80;
    update(&one, 1);
    const unsigned char zero = 0;
    while (buffered_ != 56) {
        update(&zero, 1);
    }
    std::array<unsigned char, 8> length{};
    for (std::size_t i = 0; i < 8; ++i) {
        length[i] = static_cast<unsigned char>(bits >> (56 - 8 * i));
    }
    update(length.data(), length.size());

    constexpr char kDigits[] = "0123456789abcdef";
    std::string hex;
    hex.reserve(64);
    for (const std::uint32_t word : state_) {
        for (int shift = 28; shift >= 0; shift -= 4) {
            hex += kDigits[(word >> shift) & 0xf];
        }
    }
    return hex;
}

std::string sha256_hex(std::string_view data) {
    Sha256 hash;
    hash.update(data);
    return hash.finish_hex();
}

FileHasher::FileHasher(const std::filesystem::path& path)
    : in_(path, std::ios::binary) {
    failed_ = !in_.is_open();
}

bool FileHasher::step(std::size_t budget) {
    if (failed_ || !hex_.empty()) {
        return true;
    }
    std::vector<char> chunk(std::min<std::size_t>(budget, 1u << 20));
    while (budget > 0) {
        in_.read(chunk.data(), static_cast<std::streamsize>(std::min(budget, chunk.size())));
        const std::size_t got = static_cast<std::size_t>(in_.gcount());
        hash_.update(chunk.data(), got);
        budget -= std::min(budget, got);
        if (in_.eof()) {
            hex_ = hash_.finish_hex();
            return true;
        }
        if (in_.bad() || (in_.fail() && got == 0)) {
            failed_ = true;
            return true;
        }
    }
    return false;
}

std::string parse_checksum_file(std::string_view text) {
    std::size_t begin = 0;
    while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin])) != 0) {
        ++begin;
    }
    if (text.size() - begin < 64) {
        return {};
    }
    std::string hex(text.substr(begin, 64));
    for (char& c : hex) {
        if (std::isxdigit(static_cast<unsigned char>(c)) == 0) {
            return {};
        }
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    // After the hash: nothing, or white space and then the (possibly `*`-marked) name.
    if (begin + 64 < text.size() && std::isspace(static_cast<unsigned char>(text[begin + 64])) == 0) {
        return {};
    }
    return hex;
}

}
