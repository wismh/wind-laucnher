#include "downloader.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <sstream>

namespace launcher {
namespace {

std::string lower(std::string_view text) {
    std::string out(text);
    std::ranges::transform(out, out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

// The lines of the last response in a header dump: after the last status line, up to the blank line.
std::vector<std::string_view> last_response(std::string_view dump) {
    std::vector<std::string_view> lines;
    std::size_t pos = 0;
    while (pos <= dump.size()) {
        std::size_t end = dump.find('\n', pos);
        if (end == std::string_view::npos) {
            end = dump.size();
        }
        std::string_view line = dump.substr(pos, end - pos);
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }
        if (line.starts_with("HTTP/")) {
            lines.clear();
        }
        if (!line.empty()) {
            lines.push_back(line);
        }
        pos = end + 1;
    }
    return lines;
}

}

std::string read_text_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return {};
    }
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

int parse_http_status(std::string_view header_dump) {
    const std::vector<std::string_view> lines = last_response(header_dump);
    if (lines.empty() || !lines.front().starts_with("HTTP/")) {
        return 0;
    }
    const std::size_t space = lines.front().find(' ');
    if (space == std::string_view::npos) {
        return 0;
    }
    int status = 0;
    for (std::size_t i = space + 1; i < lines.front().size() && std::isdigit(static_cast<unsigned char>(lines.front()[i]));
            ++i) {
        status = status * 10 + (lines.front()[i] - '0');
    }
    return status;
}

std::optional<std::string> find_header(std::string_view header_dump, std::string_view name) {
    const std::string wanted = lower(name);
    for (const std::string_view line : last_response(header_dump)) {
        const std::size_t colon = line.find(':');
        if (colon == std::string_view::npos || lower(line.substr(0, colon)) != wanted) {
            continue;
        }
        std::string_view value = line.substr(colon + 1);
        while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
            value.remove_prefix(1);
        }
        return std::string(value);
    }
    return std::nullopt;
}

std::vector<std::string> curl_arguments(const DownloadRequest& request, const std::filesystem::path& header_input) {
    std::vector<std::string> args{"--silent", "--show-error", "--proto", "=https", "--retry", "3", "--connect-timeout", "20"};
    if (request.download) {
        // Follow redirects (a release asset is served from another host), but only to https, fail on 4xx and 5xx, and
        // give up on a transfer that stalls for a minute.
        args.insert(args.end(), {"--location", "--proto-redir", "=https", "--fail", "--speed-limit", "1000",
                                        "--speed-time", "60"});
    } else {
        args.insert(args.end(), {"--max-time", "60"});
    }
    if (!header_input.empty()) {
        args.push_back("-H");
        args.push_back("@" + header_input.generic_string());
    }
    if (!request.header_output.empty()) {
        args.push_back("--dump-header");
        args.push_back(request.header_output.generic_string());
    }
    args.push_back("--output");
    args.push_back(request.output.generic_string());
    args.push_back(request.url);
    return args;
}

CurlDownloader::CurlDownloader(engine::IProcessLauncher& processes)
    : processes_(&processes) {}

bool CurlDownloader::busy() const {
    return active_;
}

void CurlDownloader::start(DownloadRequest request) {
    request_ = std::move(request);
    output_.clear();
    header_input_.clear();
    std::error_code error;
    std::filesystem::remove(request_.output, error);
    if (!request_.header_output.empty()) {
        std::filesystem::remove(request_.header_output, error);
    }
    if (!request_.headers.empty() || !request_.bearer_token.empty()) {
        // The headers go in a file, so a token is not on the command line (visible to every user of the machine).
        header_input_ = request_.output;
        header_input_ += ".request-headers";
        std::ofstream out(header_input_, std::ios::binary | std::ios::trunc);
        for (const std::string& header : request_.headers) {
            out << header << '\n';
        }
        if (!request_.bearer_token.empty()) {
            out << "Authorization: Bearer " << request_.bearer_token << '\n';
        }
    }
    attempts_ = 0;
    active_ = true;
    run_curl();
}

void CurlDownloader::run_curl() {
    ++attempts_;
    output_.clear();
    std::error_code error;
    std::filesystem::remove(request_.output, error);
    call_ = processes_->run(engine::ProcessDesc{.program = "curl", .arguments = curl_arguments(request_, header_input_)});
}

std::optional<DownloadResult> CurlDownloader::poll() {
    for (std::string& line : call_.take_output()) {
        output_.push_back(std::move(line));
    }
    const std::optional<engine::ProcessResult> result = call_.take();
    if (!result) {
        return std::nullopt;
    }
    constexpr int kMaxAttempts = 3;
    constexpr int kHttpError = 22;
    if (*result && result->value().code != 0 && result->value().code != kHttpError && attempts_ < kMaxAttempts) {
        run_curl();
        return std::nullopt;
    }
    active_ = false;
    std::error_code error;
    if (!header_input_.empty()) {
        std::filesystem::remove(header_input_, error);
    }

    DownloadResult answer;
    if (!request_.header_output.empty()) {
        answer.status = parse_http_status(read_text_file(request_.header_output));
    }
    if (!*result) {
        answer.error = result->error() == engine::ProcessError::NotFound
                               ? "curl is not installed. Wind downloads SDKs with curl; install it and try again."
                               : "curl could not start.";
        return answer;
    }
    if (result->value().code != 0) {
        // curl's own message (`curl: (6) Could not resolve host: ...`) is the last line it wrote.
        std::string reason = output_.empty() ? "curl failed with exit code " + std::to_string(result->value().code)
                                             : output_.back();
        if (reason.starts_with("curl: ")) {
            reason.erase(0, 6);
        }
        answer.error = std::move(reason);
    }
    return answer;
}

void CurlDownloader::cancel() {
    active_ = false;
    call_.cancel();
    std::error_code error;
    if (!header_input_.empty()) {
        std::filesystem::remove(header_input_, error);
    }
}

}
