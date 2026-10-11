#pragma once

#include "downloader.h"
#include "extractor.h"

#include <engine/process/process_launcher.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace launcher_test {

// A fresh directory per test under the temp directory, removed with the object. ctest runs tests in parallel. Named
// by a hash of the test's name, not the name: a new project's path must stay short (kMaxProjectPath).
class TempDir {
public:
    TempDir()
        : path_(std::filesystem::temp_directory_path() / "wlt" /
                  std::to_string(std::hash<std::string>{}(
                                         std::string(::testing::UnitTest::GetInstance()->current_test_info()->test_suite_name()) +
                                         "." + ::testing::UnitTest::GetInstance()->current_test_info()->name()) %
                                 1000000)) {
        std::filesystem::remove_all(path_);
        std::filesystem::create_directories(path_);
    }
    ~TempDir() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    [[nodiscard]] const std::filesystem::path& path() const {
        return path_;
    }

    void write(const std::filesystem::path& relative, std::string_view text) const {
        std::filesystem::create_directories((path_ / relative).parent_path());
        std::ofstream(path_ / relative, std::ios::binary) << text;
    }

    void sdk(const std::filesystem::path& relative, std::string_view version, bool dirty) const {
        write(relative / "sdk.toml", "version = \"" + std::string(version) + "\"\ncommit = \"0123456789abcdef\"\n" +
                                             "dirty = " + (dirty ? "true" : "false") +
                                             "\nconfig = \"Release\"\nbuild_id = \"aa\"\n");
    }

private:
    std::filesystem::path path_;
};

[[nodiscard]] inline std::string read_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

[[nodiscard]] inline std::string fixture(std::string_view name) {
    return read_file(std::filesystem::path(LAUNCHER_FIXTURES_DIR) / name);
}

// An IDownloader that answers from a function: the function sees the request, writes the files a real transfer would
// (`output`, `header_output`), and returns the answer, which poll() hands over after `polls_before_answer` polls.
class FakeDownloader final : public launcher::IDownloader {
public:
    using Handler = std::function<launcher::DownloadResult(const launcher::DownloadRequest&)>;

    explicit FakeDownloader(Handler handler = {})
        : handler_(std::move(handler)) {}

    [[nodiscard]] bool busy() const override {
        return pending_;
    }
    void start(launcher::DownloadRequest request) override {
        requests.push_back(request);
        pending_ = true;
        remaining_ = polls_before_answer;
        answer_ = handler_ ? handler_(request) : launcher::DownloadResult{};
    }
    [[nodiscard]] std::optional<launcher::DownloadResult> poll() override {
        if (!pending_) {
            return std::nullopt;
        }
        if (remaining_ > 0) {
            --remaining_;
            return std::nullopt;
        }
        pending_ = false;
        return answer_;
    }
    void cancel() override {
        if (pending_) {
            ++cancels;
        }
        pending_ = false;
    }

    std::vector<launcher::DownloadRequest> requests;
    int polls_before_answer = 0;
    int cancels = 0;

private:
    Handler handler_;
    bool pending_ = false;
    int remaining_ = 0;
    launcher::DownloadResult answer_;
};

class FakeExtractor final : public launcher::IExtractor {
public:
    using Handler = std::function<launcher::ExtractResult(const launcher::ExtractRequest&)>;

    explicit FakeExtractor(Handler handler = {})
        : handler_(std::move(handler)) {}

    [[nodiscard]] bool busy() const override {
        return pending_;
    }
    void start(launcher::ExtractRequest request) override {
        requests.push_back(request);
        pending_ = true;
        remaining_ = polls_before_answer;
        answer_ = handler_ ? handler_(request) : launcher::ExtractResult{};
    }
    [[nodiscard]] std::optional<launcher::ExtractResult> poll() override {
        if (!pending_) {
            return std::nullopt;
        }
        if (remaining_ > 0) {
            --remaining_;
            return std::nullopt;
        }
        pending_ = false;
        return answer_;
    }
    void cancel() override {
        if (pending_) {
            ++cancels;
        }
        pending_ = false;
    }

    std::vector<launcher::ExtractRequest> requests;
    int polls_before_answer = 0;
    int cancels = 0;

private:
    Handler handler_;
    bool pending_ = false;
    int remaining_ = 0;
    launcher::ExtractResult answer_;
};

// Runs a program with the real ProcessLauncher and waits for it, for tests that make a real archive. The exit code, or
// -1000 when the program could not start and -1001 after two minutes.
[[nodiscard]] inline int run_program(
        engine::ProcessLauncher& processes, engine::ProcessDesc desc, std::vector<std::string>* output = nullptr) {
    engine::ProcessCall call = processes.run(std::move(desc));
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::minutes(2);
    while (std::chrono::steady_clock::now() < deadline) {
        processes.poll();
        std::vector<std::string> lines = call.take_output();
        if (output != nullptr) {
            output->insert(output->end(), lines.begin(), lines.end());
        }
        if (const std::optional<engine::ProcessResult> result = call.take()) {
            return *result ? result->value().code : -1000;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return -1001;
}

// Polls `work` until it answers (the real ProcessLauncher advances in poll()), for at most two minutes.
template<typename Poll>
[[nodiscard]] auto wait_for(engine::ProcessLauncher& processes, Poll&& poll) -> decltype(poll()) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::minutes(2);
    while (std::chrono::steady_clock::now() < deadline) {
        processes.poll();
        if (auto answer = poll()) {
            return answer;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return {};
}


// One member of a tar archive written by write_tar.
struct TarEntry {
    std::string name;
    std::string content;
    // '0' a file, '5' a directory, '2' a symbolic link to `link`.
    char type = '0';
    std::string link;
};

// A ustar archive, so a test can make an archive with a hostile member on any system without a tool for it.
inline void write_tar(const std::filesystem::path& path, const std::vector<TarEntry>& entries) {
    std::string out;
    const auto field = [](char* at, std::size_t size, const std::string& text) {
        std::memcpy(at, text.data(), std::min(size, text.size()));
    };
    for (const TarEntry& entry : entries) {
        char header[512] = {};
        field(header, 100, entry.name);
        field(header + 100, 8, "0000755");
        field(header + 108, 8, "0000000");
        field(header + 116, 8, "0000000");
        char size[16];
        // The size is octal.
        std::snprintf(size, sizeof size, "%011o", static_cast<unsigned>(entry.type == '0' ? entry.content.size() : 0));
        field(header + 124, 12, size);
        field(header + 136, 12, "00000000000");
        std::memset(header + 148, ' ', 8);
        header[156] = entry.type;
        field(header + 157, 100, entry.link);
        field(header + 257, 6, std::string("ustar\0", 6));
        field(header + 263, 2, "00");
        unsigned sum = 0;
        for (const char c : header) {
            sum += static_cast<unsigned char>(c);
        }
        char check[16];
        std::snprintf(check, sizeof check, "%06o", sum);
        field(header + 148, 6, check);
        header[154] = '\0';
        header[155] = ' ';
        out.append(header, sizeof header);
        if (entry.type == '0') {
            out += entry.content;
            out.append((512 - entry.content.size() % 512) % 512, '\0');
        }
    }
    out.append(1024, '\0');
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary) << out;
}

}
