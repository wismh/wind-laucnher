#pragma once

#include "downloader.h"
#include "extractor.h"
#include "sdk_release.h"
#include "sha256.h"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

namespace launcher {

enum class InstallPhase {
    Idle,
    // The `.sha256` file.
    Checksum,
    // The archive. `done` of `total` bytes.
    Download,
    // The SHA-256 of the archive. `done` of `total` bytes.
    Verify,
    Unpack,
};

struct InstallProgress {
    InstallPhase phase = InstallPhase::Idle;
    std::string version;
    std::uint64_t done = 0;
    std::uint64_t total = 0;
};

// How an install ended.
struct InstallOutcome {
    std::string version;
    bool installed = false;
    bool cancelled = false;
    // For the row: why it failed, or what was done.
    std::string message;
};

// The directory an SDK of `version` is installed in: `<install_dir>/sdk-<version>`.
[[nodiscard]] std::filesystem::path installed_sdk_directory(
        const std::filesystem::path& install_dir, std::string_view version);

// Removes what an install that was interrupted (the launcher closed, the machine lost power) left in `install_dir`:
// the `.installing-*` directories. Whatever is still in use stays.
void remove_interrupted_installs(const std::filesystem::path& install_dir);

// Downloads an SDK release, checks it, and installs it: the checksum file, the archive, its SHA-256, the unpacking into
// a temporary directory inside the install directory, the checks of what was unpacked (the one top directory,
// `sdk.toml` of that version, the editor binary), and a rename to `sdk-<version>`. One install at a time. Nothing blocks
// the frame: the transfers and tar run as processes and the hash is computed a slice per poll(). Any failure and a
// cancel remove the temporary directory and leave the SDK list as it was. The archive is never executed. Main thread
// only.
class SdkInstaller {
public:
    SdkInstaller(IDownloader& downloader, IExtractor& extractor, Platform platform = current_platform());

    // Starts installing `release`. The reason when nothing started: another install runs, the SDK is there already, or
    // the install directory cannot be used.
    [[nodiscard]] std::expected<void, std::string> start(
            const SdkRelease& release, const std::filesystem::path& install_dir);
    // Advances the install. Call every frame.
    void poll();
    void cancel();

    [[nodiscard]] bool running() const {
        return progress_.phase != InstallPhase::Idle;
    }
    [[nodiscard]] const InstallProgress& progress() const {
        return progress_;
    }
    // How the last install ended, once.
    [[nodiscard]] std::optional<InstallOutcome> take_outcome();

private:
    void advance_checksum(const DownloadResult& answer);
    void advance_download(const DownloadResult& answer);
    void verify_slice();
    void advance_unpack(const ExtractResult& answer);
    void finish_install();
    void fail(std::string message);
    void end(InstallOutcome outcome);

    IDownloader* downloader_;
    IExtractor* extractor_;
    Platform platform_;

    InstallProgress progress_;
    SdkRelease release_;
    std::filesystem::path install_dir_;
    std::filesystem::path work_dir_;
    std::filesystem::path archive_path_;
    std::string expected_hash_;
    std::unique_ptr<FileHasher> hasher_;
    std::optional<InstallOutcome> outcome_;
};

}
