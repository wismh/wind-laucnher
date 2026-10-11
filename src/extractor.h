#pragma once

#include <engine/process/process_call.h>
#include <engine/process/process_launcher.h>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace launcher {

struct ExtractRequest {
    std::filesystem::path archive;
    // Where the archive is unpacked; created when it does not exist.
    std::filesystem::path destination;
    // The one top directory every entry of the archive must be in: `wind-sdk-<version>-<os>-<arch>`.
    std::string top_directory;
};

struct ExtractResult {
    // Why the archive was not unpacked, or empty.
    std::string error;

    [[nodiscard]] bool ok() const {
        return error.empty();
    }
};

// Unpacks an archive without blocking the frame: start(), then poll() every frame until it answers. Main thread only.
class IExtractor {
public:
    virtual ~IExtractor() = default;

    [[nodiscard]] virtual bool busy() const = 0;
    virtual void start(ExtractRequest request) = 0;
    [[nodiscard]] virtual std::optional<ExtractResult> poll() = 0;
    // Ends the work; no answer follows. What was unpacked so far is left to the caller.
    virtual void cancel() = 0;
};

// The system `tar` (Windows 10 and later and macOS unpack a .zip with it, every system a .tar.gz), started through
// IProcessLauncher. The archive is listed first and refused if any entry would leave the destination; the unpacked tree
// is checked again for links that point out of it. The archive is never executed.
class TarExtractor final : public IExtractor {
public:
    explicit TarExtractor(engine::IProcessLauncher& processes);

    [[nodiscard]] bool busy() const override;
    void start(ExtractRequest request) override;
    [[nodiscard]] std::optional<ExtractResult> poll() override;
    void cancel() override;

private:
    enum class Step {
        Idle,
        Listing,
        Unpacking,
    };

    std::optional<ExtractResult> fail(std::string error);

    engine::IProcessLauncher* processes_;
    engine::ProcessCall call_;
    Step step_ = Step::Idle;
    ExtractRequest request_;
    std::vector<std::string> output_;
};

// Why the archive entries (the names `tar -tf` lists) cannot be unpacked, or empty when they can: an absolute path, a
// drive, a `..` component, a control character, or a name outside `top_directory`. An archive without entries is
// refused too.
[[nodiscard]] std::string check_archive_entries(const std::vector<std::string>& entries, std::string_view top_directory);

// Why the tree unpacked under `destination` is not what the archive promised, or empty: a symbolic link whose target
// leaves `destination / top_directory`, or the directory missing.
[[nodiscard]] std::string check_extracted_tree(const std::filesystem::path& destination, std::string_view top_directory);

}
