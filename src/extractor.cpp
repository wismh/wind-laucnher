#include "extractor.h"

#include <algorithm>
#include <system_error>

namespace launcher {
namespace {

std::string last_line(const std::vector<std::string>& lines) {
    for (auto it = lines.rbegin(); it != lines.rend(); ++it) {
        if (!it->empty()) {
            return *it;
        }
    }
    return {};
}

// The components of a '/'-separated name; a trailing separator and "." components are dropped.
std::vector<std::string> components(std::string_view name) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (start <= name.size()) {
        std::size_t end = name.find('/', start);
        if (end == std::string_view::npos) {
            end = name.size();
        }
        const std::string_view part = name.substr(start, end - start);
        if (!part.empty() && part != ".") {
            parts.emplace_back(part);
        }
        start = end + 1;
    }
    return parts;
}

}

std::string check_archive_entries(const std::vector<std::string>& entries, std::string_view top_directory) {
    bool any = false;
    for (std::string entry : entries) {
        while (!entry.empty() && (entry.back() == '\r' || entry.back() == '\n')) {
            entry.pop_back();
        }
        if (entry.empty()) {
            continue;
        }
        any = true;
        if (std::ranges::any_of(entry, [](unsigned char c) { return c < 0x20 || c == 0x7f; })) {
            return "the archive has an entry with a control character in its name";
        }
        std::ranges::replace(entry, '\\', '/');
        if (entry.front() == '/') {
            return "the archive has an absolute path: " + entry;
        }
        if (entry.size() >= 2 && entry[1] == ':') {
            return "the archive has a path on a drive: " + entry;
        }
        const std::vector<std::string> parts = components(entry);
        if (std::ranges::find(parts, "..") != parts.end()) {
            return "the archive has a path that leaves its folder: " + entry;
        }
        if (parts.empty() || parts.front() != top_directory) {
            return "the archive has an entry outside " + std::string(top_directory) + ": " + entry;
        }
    }
    return any ? std::string{} : "the archive is empty";
}

std::string check_extracted_tree(const std::filesystem::path& destination, std::string_view top_directory) {
    std::error_code error;
    const std::filesystem::path root = destination / std::string(top_directory);
    if (!std::filesystem::is_directory(root, error)) {
        return "the archive did not unpack to " + std::string(top_directory);
    }
    const std::filesystem::path normal_root = root.lexically_normal();
    for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(
                 root, std::filesystem::directory_options::skip_permission_denied, error)) {
        std::error_code entry_error;
        if (!entry.is_symlink(entry_error)) {
            continue;
        }
        // Where the link points, read as text relative to its own folder, so a link to a file that does not exist is
        // judged too.
        const std::filesystem::path target = std::filesystem::read_symlink(entry.path(), entry_error);
        if (entry_error) {
            return "cannot read a link in the archive: " + entry.path().generic_string();
        }
        const std::filesystem::path resolved =
                (target.is_absolute() ? target : entry.path().parent_path() / target).lexically_normal();
        const std::filesystem::path relative = resolved.lexically_relative(normal_root);
        if (relative.empty() || *relative.begin() == "..") {
            return "the archive has a link that points out of its folder: " + entry.path().generic_string();
        }
    }
    return {};
}

TarExtractor::TarExtractor(engine::IProcessLauncher& processes)
    : processes_(&processes) {}

bool TarExtractor::busy() const {
    return step_ != Step::Idle;
}

void TarExtractor::start(ExtractRequest request) {
    request_ = std::move(request);
    output_.clear();
    step_ = Step::Listing;
    call_ = processes_->run(engine::ProcessDesc{
            .program = "tar", .arguments = {"-tf", request_.archive.generic_string()}});
}

std::optional<ExtractResult> TarExtractor::fail(std::string error) {
    step_ = Step::Idle;
    return ExtractResult{.error = std::move(error)};
}

std::optional<ExtractResult> TarExtractor::poll() {
    if (step_ == Step::Idle) {
        return std::nullopt;
    }
    for (std::string& line : call_.take_output()) {
        output_.push_back(std::move(line));
    }
    const std::optional<engine::ProcessResult> result = call_.take();
    if (!result) {
        return std::nullopt;
    }
    if (!*result) {
        return fail(result->error() == engine::ProcessError::NotFound
                            ? "tar is not installed. Wind unpacks SDKs with tar; install it and try again."
                            : "tar could not start.");
    }
    if (result->value().code != 0) {
        const std::string reason = last_line(output_);
        return fail("tar could not read the archive" + (reason.empty() ? std::string{} : ": " + reason));
    }

    if (step_ == Step::Listing) {
        if (std::string problem = check_archive_entries(output_, request_.top_directory); !problem.empty()) {
            return fail(std::move(problem));
        }
        std::error_code error;
        std::filesystem::create_directories(request_.destination, error);
        if (error) {
            return fail("cannot create " + request_.destination.generic_string() + ": " + error.message());
        }
        output_.clear();
        step_ = Step::Unpacking;
        call_ = processes_->run(engine::ProcessDesc{.program = "tar",
                .arguments = {"-xf", request_.archive.generic_string(), "-C", request_.destination.generic_string()}});
        return std::nullopt;
    }

    step_ = Step::Idle;
    if (std::string problem = check_extracted_tree(request_.destination, request_.top_directory); !problem.empty()) {
        return ExtractResult{.error = std::move(problem)};
    }
    return ExtractResult{};
}

void TarExtractor::cancel() {
    call_.cancel();
    step_ = Step::Idle;
}

}
