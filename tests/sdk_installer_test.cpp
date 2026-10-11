#include "sdk_installer.h"
#include "sha256.h"
#include "test_support.h"

#include <engine/process/process_launcher.h>

#include <gtest/gtest.h>

using launcher::DownloadRequest;
using launcher::DownloadResult;
using launcher::ExtractRequest;
using launcher::ExtractResult;
using launcher_test::FakeDownloader;
using launcher_test::FakeExtractor;
using launcher_test::TempDir;

namespace {

const launcher::Platform kPlatform = launcher::current_platform();
constexpr char kVersion[] = "0.1.2a";
constexpr char kArchiveBytes[] = "pretend this is a gzip stream";

launcher::SdkRelease make_release(std::string_view version = kVersion) {
    launcher::SdkRelease release;
    release.version = std::string(version);
    release.parsed = *engine::parse_engine_version(version);
    release.archive_name = launcher::sdk_archive_name(version, kPlatform);
    release.archive_url = "https://github.com/wismh/wind-engine/releases/download/v" + std::string(version) + "/" +
                          release.archive_name;
    release.checksum_url = release.archive_url + ".sha256";
    release.size = std::string_view(kArchiveBytes).size();
    return release;
}

std::filesystem::path editor_name() {
    return launcher::editor_executable(launcher::SdkEntry{.root = "."});
}

// What a good archive holds.
void unpack_good_sdk(const ExtractRequest& request, std::string_view version = kVersion) {
    const std::filesystem::path root = request.destination / request.top_directory;
    std::filesystem::create_directories(root / "bin");
    std::ofstream(root / "sdk.toml") << "version = \"" << version << "\"\ncommit = \"0123456789abcdef\"\ndirty = false\n"
                                     << "config = \"Release\"\nbuild_id = \"aa\"\n";
    std::ofstream(root / editor_name()) << "editor";
}

class InstallerTest : public ::testing::Test {
protected:
    InstallerTest()
        : downloader_([this](const DownloadRequest& request) { return download(request); })
        , extractor_([this](const ExtractRequest& request) { return unpack(request); })
        , installer_(downloader_, extractor_, kPlatform)
        , install_dir_(dir_.path() / "Sdks") {
        std::filesystem::create_directories(install_dir_);
    }

    DownloadResult download(const DownloadRequest& request) {
        if (request.url.ends_with(".sha256")) {
            if (fail_checksum_) {
                return DownloadResult{.error = "(22) The requested URL returned error: 404"};
            }
            std::ofstream(request.output) << checksum_text_;
            return {};
        }
        if (fail_archive_) {
            return DownloadResult{.error = "(28) Operation timed out"};
        }
        std::ofstream(request.output, std::ios::binary) << archive_bytes_;
        return {};
    }

    ExtractResult unpack(const ExtractRequest& request) {
        if (!extract_error_.empty()) {
            return ExtractResult{.error = extract_error_};
        }
        if (layout_) {
            layout_(request);
        } else {
            unpack_good_sdk(request);
        }
        return {};
    }

    // Starts the install and polls it to the end.
    std::optional<launcher::InstallOutcome> install(const launcher::SdkRelease& release) {
        const auto started = installer_.start(release, install_dir_);
        EXPECT_TRUE(started.has_value()) << (started ? "" : started.error());
        return finish();
    }

    std::optional<launcher::InstallOutcome> finish() {
        for (int i = 0; i < 1000 && installer_.running(); ++i) {
            installer_.poll();
        }
        EXPECT_FALSE(installer_.running());
        return installer_.take_outcome();
    }

    // Nothing but what was there: no temporary directory, no SDK.
    void expect_untouched() const {
        std::vector<std::string> names;
        for (const auto& entry : std::filesystem::directory_iterator(install_dir_)) {
            names.push_back(entry.path().filename().string());
        }
        EXPECT_TRUE(names.empty()) << "left behind: " << names.front();
    }

    TempDir dir_;
    FakeDownloader downloader_;
    FakeExtractor extractor_;
    launcher::SdkInstaller installer_;
    std::filesystem::path install_dir_;
    std::string archive_bytes_ = kArchiveBytes;
    std::string checksum_text_ = launcher::sha256_hex(kArchiveBytes) + "  " + launcher::sdk_archive_name(kVersion, kPlatform) + "\n";
    bool fail_checksum_ = false;
    bool fail_archive_ = false;
    std::string extract_error_;
    std::function<void(const ExtractRequest&)> layout_;
};

}

TEST_F(InstallerTest, InstallsAVerifiedArchiveAsSdkVersion) {
    const auto outcome = install(make_release());
    ASSERT_TRUE(outcome.has_value());
    EXPECT_TRUE(outcome->installed) << outcome->message;
    EXPECT_EQ(outcome->version, kVersion);

    const std::filesystem::path sdk = install_dir_ / "sdk-0.1.2a";
    EXPECT_TRUE(std::filesystem::exists(sdk / "sdk.toml"));
    EXPECT_TRUE(std::filesystem::exists(sdk / editor_name()));
    // Only the SDK is left: the temporary directory and the download are gone.
    int entries = 0;
    for (const auto& entry : std::filesystem::directory_iterator(install_dir_)) {
        ++entries;
        EXPECT_EQ(entry.path().filename(), "sdk-0.1.2a");
    }
    EXPECT_EQ(entries, 1);

    // The installed list sees it at once.
    std::vector<std::string> problems;
    const auto sdks = launcher::find_sdks(install_dir_, {}, problems);
    ASSERT_EQ(sdks.size(), 1u);
    EXPECT_EQ(sdks[0].manifest.version, kVersion);
    EXPECT_FALSE(installer_.running());
    EXPECT_EQ(installer_.take_outcome(), std::nullopt) << "the outcome is given once";
}

TEST_F(InstallerTest, DownloadsTheChecksumAndTheArchiveIntoTheTemporaryDirectory) {
    const launcher::SdkRelease release = make_release();
    ASSERT_TRUE(installer_.start(release, install_dir_).has_value());
    ASSERT_EQ(downloader_.requests.size(), 1u);
    EXPECT_EQ(downloader_.requests[0].url, release.checksum_url);
    EXPECT_TRUE(downloader_.requests[0].download);
    EXPECT_EQ(downloader_.requests[0].bearer_token, "") << "the release assets are public";
    EXPECT_EQ(downloader_.requests[0].output.parent_path().parent_path().parent_path(), install_dir_)
            << "inside the install directory, so the final rename stays on one volume";
    EXPECT_TRUE(downloader_.requests[0].output.parent_path().parent_path().filename().string().starts_with(".installing-"));
    installer_.poll();
    ASSERT_EQ(downloader_.requests.size(), 2u);
    EXPECT_EQ(downloader_.requests[1].url, release.archive_url);
    EXPECT_EQ(downloader_.requests[1].output.filename(), release.archive_name);
}

TEST_F(InstallerTest, ProgressIsBytesOfTheSizeTheApiGave) {
    downloader_.polls_before_answer = 2;
    launcher::SdkRelease release = make_release();
    release.size = 1000;
    ASSERT_TRUE(installer_.start(release, install_dir_).has_value());
    EXPECT_EQ(installer_.progress().phase, launcher::InstallPhase::Checksum);
    for (int i = 0; i < 10 && installer_.progress().phase == launcher::InstallPhase::Checksum; ++i) {
        installer_.poll();
    }
    ASSERT_EQ(installer_.progress().phase, launcher::InstallPhase::Download);
    installer_.poll();
    EXPECT_EQ(installer_.progress().total, 1000u);
    EXPECT_EQ(installer_.progress().done, std::string_view(kArchiveBytes).size()) << "the size of the file so far";
    EXPECT_EQ(installer_.progress().version, kVersion);
    static_cast<void>(finish());
}

TEST_F(InstallerTest, TheHashIsComputedAsliceAtATime) {
    archive_bytes_.assign(20u << 20, 'z');
    checksum_text_ = launcher::sha256_hex(archive_bytes_) + "  x\n";
    launcher::SdkRelease release = make_release();
    release.size = archive_bytes_.size();
    ASSERT_TRUE(installer_.start(release, install_dir_).has_value());
    int verify_polls = 0;
    for (int i = 0; i < 100 && installer_.running(); ++i) {
        installer_.poll();
        if (installer_.progress().phase == launcher::InstallPhase::Verify) {
            ++verify_polls;
        }
    }
    EXPECT_GE(verify_polls, 2) << "20 MB are not hashed in one frame";
    EXPECT_TRUE(installer_.take_outcome()->installed);
}

TEST_F(InstallerTest, AChecksumMismatchRejectsAndLeavesNothing) {
    archive_bytes_ = "tampered";
    const auto outcome = install(make_release());
    ASSERT_TRUE(outcome.has_value());
    EXPECT_FALSE(outcome->installed);
    EXPECT_NE(outcome->message.find("SHA-256"), std::string::npos) << outcome->message;
    EXPECT_TRUE(extractor_.requests.empty()) << "a bad archive is never unpacked";
    expect_untouched();
}

TEST_F(InstallerTest, AChecksumFileThatIsNotAHashIsRejected) {
    checksum_text_ = "<html>Not Found</html>";
    const auto outcome = install(make_release());
    ASSERT_TRUE(outcome.has_value());
    EXPECT_FALSE(outcome->installed);
    EXPECT_TRUE(downloader_.requests.size() == 1u) << "the archive is not even fetched";
    expect_untouched();
}

TEST_F(InstallerTest, DownloadFailuresShowTheReason) {
    fail_checksum_ = true;
    auto outcome = install(make_release());
    ASSERT_TRUE(outcome.has_value());
    EXPECT_FALSE(outcome->installed);
    EXPECT_NE(outcome->message.find("404"), std::string::npos) << outcome->message;
    expect_untouched();

    fail_checksum_ = false;
    fail_archive_ = true;
    outcome = install(make_release());
    ASSERT_TRUE(outcome.has_value());
    EXPECT_FALSE(outcome->installed);
    EXPECT_NE(outcome->message.find("timed out"), std::string::npos) << outcome->message;
    expect_untouched();
}

TEST_F(InstallerTest, AnExtractionFailureCleansUp) {
    extract_error_ = "tar could not read the archive: Unrecognized archive format";
    const auto outcome = install(make_release());
    ASSERT_TRUE(outcome.has_value());
    EXPECT_FALSE(outcome->installed);
    EXPECT_NE(outcome->message.find("Unrecognized"), std::string::npos) << outcome->message;
    expect_untouched();
}

TEST_F(InstallerTest, ZipSlipEntriesAreAnExtractionFailureWhichCleansUp) {
    extract_error_ = launcher::check_archive_entries({"wind-sdk/a", "../../evil"}, "wind-sdk");
    ASSERT_FALSE(extract_error_.empty());
    const auto outcome = install(make_release());
    ASSERT_TRUE(outcome.has_value());
    EXPECT_FALSE(outcome->installed);
    expect_untouched();
}

TEST_F(InstallerTest, AWrongTopDirectoryIsRefused) {
    layout_ = [](const ExtractRequest& request) {
        const std::filesystem::path root = request.destination / "something-else";
        std::filesystem::create_directories(root / "bin");
        std::ofstream(root / "sdk.toml") << "version = \"0.1.2a\"\n";
    };
    const auto outcome = install(make_release());
    ASSERT_TRUE(outcome.has_value());
    EXPECT_FALSE(outcome->installed);
    expect_untouched();
}

TEST_F(InstallerTest, AMissingSdkTomlIsRefused) {
    layout_ = [](const ExtractRequest& request) {
        std::filesystem::create_directories(request.destination / request.top_directory / "bin");
        std::ofstream(request.destination / request.top_directory / editor_name()) << "x";
    };
    const auto outcome = install(make_release());
    ASSERT_TRUE(outcome.has_value());
    EXPECT_FALSE(outcome->installed);
    EXPECT_NE(outcome->message.find("sdk.toml"), std::string::npos) << outcome->message;
    expect_untouched();
}

TEST_F(InstallerTest, AnSdkTomlOfAnotherVersionIsRefused) {
    layout_ = [](const ExtractRequest& request) { unpack_good_sdk(request, "0.9.9a"); };
    const auto outcome = install(make_release());
    ASSERT_TRUE(outcome.has_value());
    EXPECT_FALSE(outcome->installed);
    EXPECT_NE(outcome->message.find("0.9.9a"), std::string::npos) << outcome->message;
    expect_untouched();
}

TEST_F(InstallerTest, AMissingEditorBinaryIsRefused) {
    layout_ = [](const ExtractRequest& request) {
        const std::filesystem::path root = request.destination / request.top_directory;
        std::filesystem::create_directories(root / "bin");
        std::ofstream(root / "sdk.toml") << "version = \"0.1.2a\"\ncommit = \"0123456789abcdef\"\ndirty = false\n"
                                         << "config = \"Release\"\nbuild_id = \"aa\"\n";
    };
    const auto outcome = install(make_release());
    ASSERT_TRUE(outcome.has_value());
    EXPECT_FALSE(outcome->installed);
    EXPECT_NE(outcome->message.find("editor"), std::string::npos) << outcome->message;
    expect_untouched();
}

TEST_F(InstallerTest, CancelDuringTheDownloadCleansUp) {
    downloader_.polls_before_answer = 5;
    ASSERT_TRUE(installer_.start(make_release(), install_dir_).has_value());
    installer_.poll();
    installer_.cancel();
    EXPECT_FALSE(installer_.running());
    EXPECT_EQ(downloader_.cancels, 1);
    const auto outcome = installer_.take_outcome();
    ASSERT_TRUE(outcome.has_value());
    EXPECT_TRUE(outcome->cancelled);
    EXPECT_FALSE(outcome->installed);
    expect_untouched();
}

TEST_F(InstallerTest, CancelDuringTheUnpackingCleansUp) {
    extractor_.polls_before_answer = 5;
    ASSERT_TRUE(installer_.start(make_release(), install_dir_).has_value());
    for (int i = 0; i < 10 && installer_.progress().phase != launcher::InstallPhase::Unpack; ++i) {
        installer_.poll();
    }
    ASSERT_EQ(installer_.progress().phase, launcher::InstallPhase::Unpack);
    installer_.poll();
    installer_.cancel();
    EXPECT_EQ(extractor_.cancels, 1);
    EXPECT_TRUE(installer_.take_outcome()->cancelled);
    expect_untouched();
}

TEST_F(InstallerTest, CancelWithNothingRunningDoesNothing) {
    installer_.cancel();
    EXPECT_EQ(installer_.take_outcome(), std::nullopt);
}

TEST_F(InstallerTest, ASecondInstallWhileOneRunsIsRefused) {
    downloader_.polls_before_answer = 3;
    ASSERT_TRUE(installer_.start(make_release("0.1.2a"), install_dir_).has_value());
    const auto second = installer_.start(make_release("0.1.3a"), install_dir_);
    ASSERT_FALSE(second.has_value());
    EXPECT_NE(second.error().find("Another SDK"), std::string::npos);
    EXPECT_EQ(installer_.progress().version, "0.1.2a") << "the first goes on";
    EXPECT_FALSE(std::filesystem::exists(install_dir_ / ".installing-0.1.3a"));
    static_cast<void>(finish());
}

TEST_F(InstallerTest, AnInstalledVersionIsNotInstalledAgain) {
    dir_.sdk("Sdks/sdk-0.1.2a", "0.1.2a", false);
    const auto started = installer_.start(make_release(), install_dir_);
    ASSERT_FALSE(started.has_value());
    EXPECT_NE(started.error().find("installed already"), std::string::npos);
    EXPECT_TRUE(downloader_.requests.empty());
    EXPECT_TRUE(std::filesystem::exists(install_dir_ / "sdk-0.1.2a/sdk.toml")) << "the installed one is not touched";
}

TEST_F(InstallerTest, NoInstallDirectoryIsAnError) {
    EXPECT_FALSE(installer_.start(make_release(), {}).has_value());
}

TEST_F(InstallerTest, AFailedInstallLeavesTheInstalledListAsItWas) {
    dir_.sdk("Sdks/sdk-0.1.1a", "0.1.1a", false);
    std::vector<std::string> problems;
    const auto before = launcher::find_sdks(install_dir_, {}, problems);
    archive_bytes_ = "tampered";
    static_cast<void>(install(make_release()));
    const auto after = launcher::find_sdks(install_dir_, {}, problems);
    ASSERT_EQ(before.size(), 1u);
    ASSERT_EQ(after.size(), 1u);
    EXPECT_EQ(after[0].root, before[0].root);
}

TEST_F(InstallerTest, AfterAFailureTheNextInstallWorks) {
    archive_bytes_ = "tampered";
    EXPECT_FALSE(install(make_release())->installed);
    archive_bytes_ = kArchiveBytes;
    EXPECT_TRUE(install(make_release())->installed);
}

TEST(InterruptedInstalls, LeftoversAreRemovedOnStart) {
    const TempDir dir;
    dir.write("Sdks/.installing-0.1.0a/download/wind-sdk-0.1.0a-linux-x86_64.tar.gz", "half");
    dir.write("Sdks/.installing-0.1.1a/extract/wind-sdk-0.1.1a-linux-x86_64/sdk.toml", "x");
    dir.sdk("Sdks/sdk-0.1.2a", "0.1.2a", false);
    dir.write("Sdks/notes.txt", "keep");
    launcher::remove_interrupted_installs(dir.path() / "Sdks");
    EXPECT_FALSE(std::filesystem::exists(dir.path() / "Sdks/.installing-0.1.0a"));
    EXPECT_FALSE(std::filesystem::exists(dir.path() / "Sdks/.installing-0.1.1a"));
    EXPECT_TRUE(std::filesystem::exists(dir.path() / "Sdks/sdk-0.1.2a/sdk.toml"));
    EXPECT_TRUE(std::filesystem::exists(dir.path() / "Sdks/notes.txt"));
    launcher::remove_interrupted_installs(dir.path() / "missing");
    launcher::remove_interrupted_installs({});
}

TEST(InstallerPaths, TheDirectoryOfAVersion) {
    EXPECT_EQ(launcher::installed_sdk_directory("/d", "0.1.2a"), std::filesystem::path("/d/sdk-0.1.2a"));
}

namespace {

// The whole path with the real tar: a real archive the way the release makes it, a fake download that copies it, the real
// extractor, and the real checks.
class RealInstall : public ::testing::Test {
protected:
    TempDir dir_;
    engine::ProcessLauncher processes_;
};

}

TEST_F(RealInstall, ARealArchiveIsDownloadedVerifiedUnpackedAndMoved) {
    const std::string top = launcher::sdk_top_directory(kVersion, kPlatform);
    dir_.sdk("src/" + top, kVersion, false);
    dir_.write("src/" + top + "/" + editor_name().generic_string(), "editor");
    dir_.write("src/" + top + "/assets/engine/catalog.toml", "[c]\n");
    const std::filesystem::path archive = dir_.path() / launcher::sdk_archive_name(kVersion, kPlatform);
#if defined(_WIN32)
    const std::vector<std::string> pack = {"-a", "-cf", archive.generic_string(), "-C", (dir_.path() / "src").generic_string(), top};
#else
    const std::vector<std::string> pack = {"-czf", archive.generic_string(), "-C", (dir_.path() / "src").generic_string(), top};
#endif
    ASSERT_EQ(launcher_test::run_program(processes_, {.program = "tar", .arguments = pack}), 0);
    const std::string bytes = launcher_test::read_file(archive);
    const std::string checksum = launcher::sha256_hex(bytes) + "  " + archive.filename().string() + "\n";

    FakeDownloader downloader([&](const DownloadRequest& request) {
        if (request.url.ends_with(".sha256")) {
            std::ofstream(request.output) << checksum;
        } else {
            std::ofstream(request.output, std::ios::binary) << bytes;
        }
        return DownloadResult{};
    });
    launcher::TarExtractor extractor(processes_);
    launcher::SdkInstaller installer(downloader, extractor, kPlatform);
    const std::filesystem::path install_dir = dir_.path() / "Sdks";
    launcher::SdkRelease release = make_release();
    release.size = bytes.size();
    ASSERT_TRUE(installer.start(release, install_dir).has_value());

    std::optional<launcher::InstallOutcome> outcome;
    for (int i = 0; i < 60000 && !outcome; ++i) {
        processes_.poll();
        installer.poll();
        outcome = installer.take_outcome();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    ASSERT_TRUE(outcome.has_value()) << "the install did not finish";
    EXPECT_TRUE(outcome->installed) << outcome->message;
    EXPECT_TRUE(std::filesystem::exists(install_dir / "sdk-0.1.2a/sdk.toml"));
    EXPECT_EQ(launcher_test::read_file(install_dir / "sdk-0.1.2a/assets/engine/catalog.toml"), "[c]\n");
    EXPECT_TRUE(std::filesystem::exists(install_dir / "sdk-0.1.2a" / editor_name()));
    EXPECT_FALSE(std::filesystem::exists(install_dir / ".installing-0.1.2a"));
}

TEST_F(RealInstall, AHostileArchiveInstallsNothing) {
    const std::string top = launcher::sdk_top_directory(kVersion, kPlatform);
    launcher_test::write_tar(dir_.path() / "evil.tar", {{.name = top + "/sdk.toml", .content = "x"},
                                                              {.name = "../../evil.txt", .content = "pwned"}});
    const std::string bytes = launcher_test::read_file(dir_.path() / "evil.tar");
    FakeDownloader downloader([&](const DownloadRequest& request) {
        if (request.url.ends_with(".sha256")) {
            std::ofstream(request.output) << launcher::sha256_hex(bytes) << "  x\n";
        } else {
            std::ofstream(request.output, std::ios::binary) << bytes;
        }
        return DownloadResult{};
    });
    launcher::TarExtractor extractor(processes_);
    launcher::SdkInstaller installer(downloader, extractor, kPlatform);
    const std::filesystem::path install_dir = dir_.path() / "Sdks";
    ASSERT_TRUE(installer.start(make_release(), install_dir).has_value());
    std::optional<launcher::InstallOutcome> outcome;
    for (int i = 0; i < 60000 && !outcome; ++i) {
        processes_.poll();
        installer.poll();
        outcome = installer.take_outcome();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    ASSERT_TRUE(outcome.has_value());
    EXPECT_FALSE(outcome->installed);
    EXPECT_FALSE(std::filesystem::exists(dir_.path() / "evil.txt"));
    EXPECT_FALSE(std::filesystem::exists(install_dir / "sdk-0.1.2a"));
    EXPECT_FALSE(std::filesystem::exists(install_dir / ".installing-0.1.2a"));
}
