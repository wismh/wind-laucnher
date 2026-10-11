#include "sha256.h"
#include "test_support.h"

#include <gtest/gtest.h>

#include <string>

using launcher_test::TempDir;

TEST(Sha256, TheStandardVectors) {
    EXPECT_EQ(launcher::sha256_hex(""), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    EXPECT_EQ(launcher::sha256_hex("abc"), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    EXPECT_EQ(launcher::sha256_hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
            "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    EXPECT_EQ(launcher::sha256_hex(std::string(1000000, 'a')),
            "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

TEST(Sha256, TheBlockEdges) {
    // 55, 56, 63, 64, and 65 bytes: the padding of the last block changes shape around 56.
    EXPECT_EQ(launcher::sha256_hex(std::string(55, 'a')), "9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318");
    EXPECT_EQ(launcher::sha256_hex(std::string(56, 'a')), "b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a");
    EXPECT_EQ(launcher::sha256_hex(std::string(64, 'a')), "ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb");
}

TEST(Sha256, InPiecesIsTheSameAsInOne) {
    const std::string text(1000, 'x');
    launcher::Sha256 hash;
    for (std::size_t at = 0; at < text.size(); at += 37) {
        hash.update(std::string_view(text).substr(at, 37));
    }
    EXPECT_EQ(hash.finish_hex(), launcher::sha256_hex(text));
}

TEST(Sha256, AFileIsHashedASliceAtATime) {
    const TempDir dir;
    const std::string text(100000, 'q');
    dir.write("big.bin", text);
    launcher::FileHasher hasher(dir.path() / "big.bin");
    int slices = 0;
    while (!hasher.step(4096)) {
        ++slices;
        ASSERT_LT(slices, 1000);
    }
    EXPECT_GE(slices, 20) << "a slice is a bounded amount of work";
    ASSERT_FALSE(hasher.failed());
    EXPECT_EQ(hasher.hex(), launcher::sha256_hex(text));
}

TEST(Sha256, AnEmptyFileAndAMissingFile) {
    const TempDir dir;
    dir.write("empty.bin", "");
    launcher::FileHasher empty(dir.path() / "empty.bin");
    EXPECT_TRUE(empty.step(10));
    EXPECT_FALSE(empty.failed());
    EXPECT_EQ(empty.hex(), launcher::sha256_hex(""));

    launcher::FileHasher missing(dir.path() / "nope.bin");
    EXPECT_TRUE(missing.step(10));
    EXPECT_TRUE(missing.failed());
}

TEST(Sha256, ChecksumFileLines) {
    const std::string hex = launcher::sha256_hex("abc");
    EXPECT_EQ(launcher::parse_checksum_file(hex + "  wind-sdk-0.1.0a-linux-x86_64.tar.gz\n"), hex);
    EXPECT_EQ(launcher::parse_checksum_file(hex + " *wind-sdk-0.1.0a-linux-x86_64.tar.gz\r\n"), hex);
    EXPECT_EQ(launcher::parse_checksum_file(hex), hex);
    EXPECT_EQ(launcher::parse_checksum_file("\n  " + hex + "  a\n"), hex);
    std::string upper = hex;
    for (char& c : upper) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    EXPECT_EQ(launcher::parse_checksum_file(upper + "  a"), hex) << "lowercased";
}

TEST(Sha256, NotAChecksumFile) {
    EXPECT_EQ(launcher::parse_checksum_file(""), "");
    EXPECT_EQ(launcher::parse_checksum_file("<html>Not Found</html>"), "");
    EXPECT_EQ(launcher::parse_checksum_file(std::string(63, 'a') + "  x"), "");
    EXPECT_EQ(launcher::parse_checksum_file(std::string(64, 'g') + "  x"), "");
    EXPECT_EQ(launcher::parse_checksum_file(std::string(65, 'a') + "  x"), "") << "a longer hex run is not a SHA-256";
}
