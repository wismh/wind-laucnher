#include "extractor.h"
#include "test_support.h"

#include <gtest/gtest.h>

using launcher_test::TarEntry;
using launcher_test::TempDir;
using launcher_test::write_tar;

TEST(ArchiveEntries, ARegularSdkIsFine) {
    EXPECT_EQ(launcher::check_archive_entries({"wind-sdk-1/", "wind-sdk-1/sdk.toml", "wind-sdk-1/bin/wind_editor"},
                      "wind-sdk-1"),
            "");
    EXPECT_EQ(launcher::check_archive_entries({"./wind-sdk-1/", "./wind-sdk-1/sdk.toml"}, "wind-sdk-1"), "");
    EXPECT_EQ(launcher::check_archive_entries({"wind-sdk-1\\sdk.toml"}, "wind-sdk-1"), "") << "a Windows separator";
}

TEST(ArchiveEntries, ZipSlipAndAbsolutePathsAreRefused) {
    const std::string top = "wind-sdk-1";
    for (const char* entry : {"../evil.txt", "wind-sdk-1/../../evil.txt", "wind-sdk-1/a/../../../x", "/etc/passwd",
                 "\\windows\\x", "C:/Windows/x", "c:evil", "wind-sdk-1/..\\..\\x", "./../x"}) {
        EXPECT_NE(launcher::check_archive_entries({"wind-sdk-1/sdk.toml", entry}, top), "") << entry;
    }
}

TEST(ArchiveEntries, EverythingMustBeInTheOneTopDirectory) {
    EXPECT_NE(launcher::check_archive_entries({"wind-sdk-1/a", "other/b"}, "wind-sdk-1"), "");
    EXPECT_NE(launcher::check_archive_entries({"wind-sdk-12/a"}, "wind-sdk-1"), "") << "a longer name is another folder";
    EXPECT_NE(launcher::check_archive_entries({"a"}, "wind-sdk-1"), "");
    EXPECT_NE(launcher::check_archive_entries({"wind-sdk-1"}, "wind-sdk-2"), "");
}

TEST(ArchiveEntries, EmptyOrOddNamesAreRefused) {
    EXPECT_NE(launcher::check_archive_entries({}, "wind-sdk-1"), "");
    EXPECT_NE(launcher::check_archive_entries({"", "\r"}, "wind-sdk-1"), "");
    EXPECT_NE(launcher::check_archive_entries({"wind-sdk-1/a\x01" "b"}, "wind-sdk-1"), "");
    EXPECT_NE(launcher::check_archive_entries({"wind-sdk-1/a\nb"}, "wind-sdk-1"), "");
}

TEST(ExtractedTree, TheTopDirectoryMustBeThere) {
    const TempDir dir;
    EXPECT_NE(launcher::check_extracted_tree(dir.path(), "wind-sdk-1"), "");
    dir.write("wind-sdk-1/sdk.toml", "x");
    EXPECT_EQ(launcher::check_extracted_tree(dir.path(), "wind-sdk-1"), "");
}

TEST(ExtractedTree, LinksInsideTheSdkAreFineAndLinksOutAreNot) {
    const TempDir dir;
    dir.write("wind-sdk-1/lib/libx.so.1", "x");
    std::error_code error;
    std::filesystem::create_symlink("libx.so.1", dir.path() / "wind-sdk-1/lib/libx.so", error);
    if (error) {
        GTEST_SKIP() << "no symbolic links here: " << error.message();
    }
    EXPECT_EQ(launcher::check_extracted_tree(dir.path(), "wind-sdk-1"), "");
    std::filesystem::create_symlink("../../outside", dir.path() / "wind-sdk-1/lib/escape", error);
    EXPECT_NE(launcher::check_extracted_tree(dir.path(), "wind-sdk-1"), "");
}

TEST(ExtractedTree, AnAbsoluteLinkIsOutside) {
    const TempDir dir;
    dir.write("wind-sdk-1/a", "x");
    std::error_code error;
    std::filesystem::create_symlink(std::filesystem::temp_directory_path(), dir.path() / "wind-sdk-1/tmp", error);
    if (error) {
        GTEST_SKIP() << "no symbolic links here";
    }
    EXPECT_NE(launcher::check_extracted_tree(dir.path(), "wind-sdk-1"), "");
}

namespace {

// The real extraction path: tar through the real ProcessLauncher.
class RealTar : public ::testing::Test {
protected:
    launcher::ExtractResult extract(const std::filesystem::path& archive, const std::string& top) {
        launcher::TarExtractor extractor(processes_);
        extractor.start(launcher::ExtractRequest{.archive = archive, .destination = dir_.path() / "out",
                .top_directory = top});
        const auto answer = launcher_test::wait_for(processes_, [&] { return extractor.poll(); });
        EXPECT_TRUE(answer.has_value()) << "tar did not finish";
        return answer.value_or(launcher::ExtractResult{.error = "timeout"});
    }

    TempDir dir_;
    engine::ProcessLauncher processes_;
};

}

TEST_F(RealTar, UnpacksATarArchive) {
    write_tar(dir_.path() / "sdk.tar", {{.name = "wind-sdk-1/", .type = '5'},
                                               {.name = "wind-sdk-1/sdk.toml", .content = "version = \"1\"\n"},
                                               {.name = "wind-sdk-1/bin/", .type = '5'},
                                               {.name = "wind-sdk-1/bin/wind_editor", .content = "ELF"}});
    const launcher::ExtractResult result = extract(dir_.path() / "sdk.tar", "wind-sdk-1");
    EXPECT_TRUE(result.ok()) << result.error;
    EXPECT_EQ(launcher_test::read_file(dir_.path() / "out/wind-sdk-1/sdk.toml"), "version = \"1\"\n");
    EXPECT_TRUE(std::filesystem::exists(dir_.path() / "out/wind-sdk-1/bin/wind_editor"));
}

TEST_F(RealTar, UnpacksTheArchiveTheReleaseWorkflowMakes) {
    // A directory packed with the platform's tool, the way the engine's release does it: tar -czf, or a zip.
    dir_.write("src/wind-sdk-2/sdk.toml", "version = \"2\"\n");
    dir_.write("src/wind-sdk-2/bin/wind_editor", "editor");
    dir_.write("src/wind-sdk-2/assets/engine/catalog.toml", "[catalog]\n");
#if defined(_WIN32)
    const std::filesystem::path archive = dir_.path() / "sdk.zip";
    const std::vector<std::string> pack = {"-a", "-cf", archive.generic_string(), "-C", (dir_.path() / "src").generic_string(), "wind-sdk-2"};
#else
    const std::filesystem::path archive = dir_.path() / "sdk.tar.gz";
    const std::vector<std::string> pack = {"-czf", archive.generic_string(), "-C", (dir_.path() / "src").generic_string(), "wind-sdk-2"};
#endif
    std::vector<std::string> output;
    ASSERT_EQ(launcher_test::run_program(processes_, {.program = "tar", .arguments = pack}, &output), 0);
    const launcher::ExtractResult result = extract(archive, "wind-sdk-2");
    EXPECT_TRUE(result.ok()) << result.error;
    EXPECT_EQ(launcher_test::read_file(dir_.path() / "out/wind-sdk-2/sdk.toml"), "version = \"2\"\n");
    EXPECT_EQ(launcher_test::read_file(dir_.path() / "out/wind-sdk-2/assets/engine/catalog.toml"), "[catalog]\n");
}

#if defined(_WIN32) || defined(__APPLE__)
TEST_F(RealTar, UnpacksAZip) {
    dir_.write("src/wind-sdk-3/sdk.toml", "version = \"3\"\n");
    const std::filesystem::path archive = dir_.path() / "sdk.zip";
    ASSERT_EQ(launcher_test::run_program(processes_,
                      {.program = "tar", .arguments = {"-a", "-cf", archive.generic_string(), "-C",
                                                              (dir_.path() / "src").generic_string(), "wind-sdk-3"}}),
            0);
    const launcher::ExtractResult result = extract(archive, "wind-sdk-3");
    EXPECT_TRUE(result.ok()) << result.error;
    EXPECT_TRUE(std::filesystem::exists(dir_.path() / "out/wind-sdk-3/sdk.toml"));
}
#endif

// The engine's SDK carries four macOS symbolic links in source/. A link cannot be created on Windows without a privilege:
// tar says so and exits 1, but the rest is there, and that is not a failed install.
TEST_F(RealTar, ALinkThatCannotBeCreatedDoesNotFailTheUnpacking) {
    write_tar(dir_.path() / "links.tar", {{.name = "wind-sdk-1/sdk.toml", .content = "x"},
                                          {.name = "wind-sdk-1/Current", .type = '2', .link = "sdk.toml"}});
    const launcher::ExtractResult result = extract(dir_.path() / "links.tar", "wind-sdk-1");
    EXPECT_TRUE(result.ok()) << result.error;
    EXPECT_TRUE(std::filesystem::exists(dir_.path() / "out/wind-sdk-1/sdk.toml"));
}

TEST_F(RealTar, AZipSlipArchiveIsRefusedBeforeAnythingIsWritten) {
    write_tar(dir_.path() / "evil.tar", {{.name = "wind-sdk-1/sdk.toml", .content = "x"},
                                                {.name = "../evil.txt", .content = "pwned"}});
    const launcher::ExtractResult result = extract(dir_.path() / "evil.tar", "wind-sdk-1");
    EXPECT_FALSE(result.ok());
    EXPECT_FALSE(std::filesystem::exists(dir_.path() / "evil.txt"));
    EXPECT_FALSE(std::filesystem::exists(dir_.path() / "out/wind-sdk-1/sdk.toml")) << "refused before the unpacking";
}

TEST_F(RealTar, AnAbsolutePathIsRefused) {
    write_tar(dir_.path() / "abs.tar", {{.name = "wind-sdk-1/sdk.toml", .content = "x"},
                                               {.name = "/tmp/wind-launcher-test-evil.txt", .content = "pwned"}});
    const launcher::ExtractResult result = extract(dir_.path() / "abs.tar", "wind-sdk-1");
    EXPECT_FALSE(result.ok());
    EXPECT_FALSE(std::filesystem::exists("/tmp/wind-launcher-test-evil.txt"));
}

TEST_F(RealTar, AnotherTopDirectoryIsRefused) {
    write_tar(dir_.path() / "other.tar", {{.name = "something-else/sdk.toml", .content = "x"}});
    const launcher::ExtractResult result = extract(dir_.path() / "other.tar", "wind-sdk-1");
    EXPECT_FALSE(result.ok());
    EXPECT_NE(result.error.find("outside"), std::string::npos) << result.error;
}

TEST_F(RealTar, ALinkOutOfTheFolderIsRefusedAfterTheUnpacking) {
#if defined(_WIN32)
    GTEST_SKIP() << "a symbolic link needs a privilege on Windows";
#endif
    write_tar(dir_.path() / "link.tar", {{.name = "wind-sdk-1/sdk.toml", .content = "x"},
                                                {.name = "wind-sdk-1/escape", .type = '2', .link = "../../../outside"}});
    const launcher::ExtractResult result = extract(dir_.path() / "link.tar", "wind-sdk-1");
    EXPECT_FALSE(result.ok());
    EXPECT_NE(result.error.find("link"), std::string::npos) << result.error;
}

TEST_F(RealTar, NotAnArchiveAndAMissingFile) {
    dir_.write("junk.tar.gz", "this is not an archive");
    const launcher::ExtractResult junk = extract(dir_.path() / "junk.tar.gz", "wind-sdk-1");
    EXPECT_FALSE(junk.ok());
    EXPECT_NE(junk.error.find("tar"), std::string::npos) << junk.error;
    EXPECT_FALSE(extract(dir_.path() / "missing.tar.gz", "wind-sdk-1").ok());
}

TEST_F(RealTar, CancelEndsTheWorkWithoutAnAnswer) {
    write_tar(dir_.path() / "sdk.tar", {{.name = "wind-sdk-1/sdk.toml", .content = "x"}});
    launcher::TarExtractor extractor(processes_);
    extractor.start(launcher::ExtractRequest{.archive = dir_.path() / "sdk.tar", .destination = dir_.path() / "out",
            .top_directory = "wind-sdk-1"});
    EXPECT_TRUE(extractor.busy());
    extractor.cancel();
    EXPECT_FALSE(extractor.busy());
    processes_.poll();
    EXPECT_EQ(extractor.poll(), std::nullopt);
}
