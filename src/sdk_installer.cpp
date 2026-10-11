#include "sdk_installer.h"

#include <engine/project/sdk_manifest.h>

#include <algorithm>
#include <system_error>
#include <utility>

namespace launcher {
namespace {

constexpr char kWorkPrefix[] = ".installing-";
// The bytes of the archive hashed per frame: a few milliseconds.
constexpr std::size_t kHashSlice = 8u << 20;

}

std::filesystem::path installed_sdk_directory(const std::filesystem::path& install_dir, std::string_view version) {
    return install_dir / ("sdk-" + std::string(version));
}

void remove_interrupted_installs(const std::filesystem::path& install_dir) {
    std::error_code error;
    if (install_dir.empty() || !std::filesystem::is_directory(install_dir, error)) {
        return;
    }
    for (const std::filesystem::directory_entry& child : std::filesystem::directory_iterator(install_dir, error)) {
        if (child.is_directory(error) && child.path().filename().string().starts_with(kWorkPrefix)) {
            std::filesystem::remove_all(child.path(), error);
        }
    }
}

SdkInstaller::SdkInstaller(IDownloader& downloader, IExtractor& extractor, Platform platform)
    : downloader_(&downloader)
    , extractor_(&extractor)
    , platform_(std::move(platform)) {}

std::expected<void, std::string> SdkInstaller::start(const SdkRelease& release, const std::filesystem::path& install_dir) {
    if (running()) {
        return std::unexpected("Another SDK is being installed. Wait for it or cancel it.");
    }
    if (install_dir.empty()) {
        return std::unexpected("There is no install directory on this machine.");
    }
    std::error_code error;
    if (std::filesystem::exists(installed_sdk_directory(install_dir, release.version), error)) {
        return std::unexpected("SDK " + release.version + " is installed already.");
    }
    const std::filesystem::path work = install_dir / (kWorkPrefix + release.version);
    std::filesystem::remove_all(work, error);
    std::filesystem::create_directories(work / "download", error);
    if (error) {
        return std::unexpected("Cannot create " + work.generic_string() + ": " + error.message());
    }

    release_ = release;
    install_dir_ = install_dir;
    work_dir_ = work;
    archive_path_ = work / "download" / release.archive_name;
    expected_hash_.clear();
    hasher_.reset();
    outcome_.reset();
    progress_ = InstallProgress{.phase = InstallPhase::Checksum, .version = release.version};

    DownloadRequest request;
    request.url = release.checksum_url;
    request.output = work / "download" / (release.archive_name + ".sha256");
    downloader_->start(std::move(request));
    return {};
}

void SdkInstaller::poll() {
    switch (progress_.phase) {
        case InstallPhase::Idle:
            return;
        case InstallPhase::Checksum:
            if (const std::optional<DownloadResult> answer = downloader_->poll()) {
                advance_checksum(*answer);
            }
            return;
        case InstallPhase::Download: {
            std::error_code error;
            const std::uintmax_t size = std::filesystem::file_size(archive_path_, error);
            if (!error) {
                progress_.done = size;
            }
            if (const std::optional<DownloadResult> answer = downloader_->poll()) {
                advance_download(*answer);
            }
            return;
        }
        case InstallPhase::Verify:
            verify_slice();
            return;
        case InstallPhase::Unpack:
            if (const std::optional<ExtractResult> answer = extractor_->poll()) {
                advance_unpack(*answer);
            }
            return;
    }
}

void SdkInstaller::advance_checksum(const DownloadResult& answer) {
    if (!answer.ok()) {
        fail("Could not download the checksum: " + answer.error);
        return;
    }
    expected_hash_ = parse_checksum_file(read_text_file(work_dir_ / "download" / (release_.archive_name + ".sha256")));
    if (expected_hash_.empty()) {
        fail("The checksum file of " + release_.archive_name + " is not a SHA-256.");
        return;
    }
    progress_.phase = InstallPhase::Download;
    progress_.done = 0;
    progress_.total = release_.size;
    DownloadRequest request;
    request.url = release_.archive_url;
    request.output = archive_path_;
    downloader_->start(std::move(request));
}

void SdkInstaller::advance_download(const DownloadResult& answer) {
    if (!answer.ok()) {
        fail("Could not download " + release_.archive_name + ": " + answer.error);
        return;
    }
    hasher_ = std::make_unique<FileHasher>(archive_path_);
    std::error_code error;
    const std::uintmax_t size = std::filesystem::file_size(archive_path_, error);
    progress_.phase = InstallPhase::Verify;
    progress_.done = 0;
    progress_.total = error ? 0 : size;
}

void SdkInstaller::verify_slice() {
    if (!hasher_->step(kHashSlice)) {
        progress_.done = std::min<std::uint64_t>(progress_.total, progress_.done + kHashSlice);
        return;
    }
    if (hasher_->failed()) {
        fail("Could not read the downloaded " + release_.archive_name + ".");
        return;
    }
    if (hasher_->hex() != expected_hash_) {
        fail("The download of " + release_.archive_name + " is damaged: its SHA-256 is not the published one.");
        return;
    }
    hasher_.reset();
    progress_.phase = InstallPhase::Unpack;
    extractor_->start(ExtractRequest{.archive = archive_path_,
            .destination = work_dir_ / "extract",
            .top_directory = sdk_top_directory(release_.version, platform_)});
}

void SdkInstaller::advance_unpack(const ExtractResult& answer) {
    if (!answer.ok()) {
        fail("Could not unpack " + release_.archive_name + ": " + answer.error);
        return;
    }
    finish_install();
}

void SdkInstaller::finish_install() {
    const std::filesystem::path unpacked = work_dir_ / "extract" / sdk_top_directory(release_.version, platform_);
    const auto manifest = engine::read_sdk_manifest(unpacked);
    if (!manifest) {
        fail("The SDK in " + release_.archive_name + " has no readable sdk.toml: " + engine::describe(manifest.error()));
        return;
    }
    if (manifest->version != release_.version) {
        fail("The SDK in " + release_.archive_name + " is version " + manifest->version + ", not " + release_.version + ".");
        return;
    }
    std::error_code error;
    if (!std::filesystem::is_regular_file(editor_executable(SdkEntry{.root = unpacked}), error)) {
        fail("The SDK in " + release_.archive_name + " has no editor (bin/wind_editor).");
        return;
    }
    const std::filesystem::path target = installed_sdk_directory(install_dir_, release_.version);
    if (std::filesystem::exists(target, error)) {
        fail("SDK " + release_.version + " is installed already.");
        return;
    }
    // One rename inside the install directory: the SDK appears whole or not at all.
    std::filesystem::rename(unpacked, target, error);
    if (error) {
        fail("Could not move the SDK to " + target.generic_string() + ": " + error.message());
        return;
    }
    std::filesystem::remove_all(work_dir_, error);
    end(InstallOutcome{.version = release_.version, .installed = true, .message = "Installed SDK " + release_.version + "."});
}

void SdkInstaller::fail(std::string message) {
    end(InstallOutcome{.version = release_.version, .message = std::move(message)});
}

void SdkInstaller::end(InstallOutcome outcome) {
    // Whatever was running ends, and the temporary directory goes with the partial download.
    downloader_->cancel();
    extractor_->cancel();
    hasher_.reset();
    std::error_code error;
    std::filesystem::remove_all(work_dir_, error);
    progress_ = InstallProgress{};
    outcome_ = std::move(outcome);
}

void SdkInstaller::cancel() {
    if (!running()) {
        return;
    }
    end(InstallOutcome{.version = release_.version, .cancelled = true, .message = "Install of SDK " + release_.version + " cancelled."});
}

std::optional<InstallOutcome> SdkInstaller::take_outcome() {
    return std::exchange(outcome_, std::nullopt);
}

}
