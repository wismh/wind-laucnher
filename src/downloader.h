#pragma once

#include <engine/process/process_call.h>
#include <engine/process/process_launcher.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace launcher {

// One HTTPS GET into a file.
struct DownloadRequest {
    std::string url;
    // The body goes here. The directory exists.
    std::filesystem::path output;
    // The response headers go here when it is not empty (the last response, after redirects).
    std::filesystem::path header_output;
    // `Name: value` lines sent with the request.
    std::vector<std::string> headers;
    // A bearer token for the request, or empty. It is never put on a command line.
    std::string bearer_token;
    // A download: follow redirects, and an HTTP status of 400 or more is an error. Off for an API call, whose 304,
    // 403, and 429 answers the caller reads itself.
    bool download = true;
};

struct DownloadResult {
    // The HTTP status of the last response, 0 when none arrived.
    int status = 0;
    // Why nothing was received, or empty. A status of 400 or more of a `download` is an error too.
    std::string error;

    [[nodiscard]] bool ok() const {
        return error.empty();
    }
};

// Fetches one URL at a time without blocking the frame: start(), then poll() every frame until it answers. Main thread
// only.
class IDownloader {
public:
    virtual ~IDownloader() = default;

    [[nodiscard]] virtual bool busy() const = 0;
    virtual void start(DownloadRequest request) = 0;
    // The answer, once, after the transfer ended; nullopt while it runs or when nothing is started.
    [[nodiscard]] virtual std::optional<DownloadResult> poll() = 0;
    // Ends the transfer; no answer follows. The partial output file is left to the caller.
    virtual void cancel() = 0;
};

// The `curl` that ships with Windows 10 1803+, macOS, and every common Linux, started through IProcessLauncher: the
// engine has no HTTP client on Linux and macOS. A transfer that breaks (a reset connection, a timeout) is started again,
// up to three times in all: curl's own `--retry` does not retry a reset, and `--retry-all-errors` is newer than the curl
// of Windows 10 1803. An HTTP error (curl's exit code 22, a 404) and a program that cannot start are not retried.
class CurlDownloader final : public IDownloader {
public:
    explicit CurlDownloader(engine::IProcessLauncher& processes);

    [[nodiscard]] bool busy() const override;
    void start(DownloadRequest request) override;
    [[nodiscard]] std::optional<DownloadResult> poll() override;
    void cancel() override;

private:
    void run_curl();

    engine::IProcessLauncher* processes_;
    engine::ProcessCall call_;
    DownloadRequest request_;
    std::filesystem::path header_input_;
    std::vector<std::string> output_;
    int attempts_ = 0;
    // From start() until poll() has given the answer or cancel(): a finished call is not pending, but is not answered yet.
    bool active_ = false;
};

// The arguments of the `curl` call for `request`; `header_input` is the file with the request headers, passed as
// `-H @file`, or empty. Public for the tests.
[[nodiscard]] std::vector<std::string> curl_arguments(
        const DownloadRequest& request, const std::filesystem::path& header_input);

// The status of the last `HTTP/<v> <code> ...` line of a header dump, 0 when there is none.
[[nodiscard]] int parse_http_status(std::string_view header_dump);

// The value of the header `name` (case-insensitive) in the last response of a header dump, or nullopt.
[[nodiscard]] std::optional<std::string> find_header(std::string_view header_dump, std::string_view name);

// The whole file as text, or empty when it cannot be read.
[[nodiscard]] std::string read_text_file(const std::filesystem::path& path);

}
