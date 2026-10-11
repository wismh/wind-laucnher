#include "downloader.h"
#include "test_support.h"

#include <gtest/gtest.h>

#include <algorithm>

using launcher_test::TempDir;

namespace {

// An IProcessLauncher that does not start anything: it records the program and answers with a prepared result. `on_run`
// writes the files a real curl would.
class FakeLauncher final : public engine::IProcessLauncher {
public:
    void dispose() override {}
    [[nodiscard]] engine::ProcessCall run(engine::ProcessDesc desc) override {
        runs.push_back(desc);
        if (on_run) {
            on_run(desc);
        }
        return engine::ProcessCall::resolved(result, output);
    }
    [[nodiscard]] std::expected<void, engine::ProcessError> launch(const engine::ProcessDesc&) override {
        return {};
    }
    [[nodiscard]] bool is_supported() const override {
        return true;
    }

    std::vector<engine::ProcessDesc> runs;
    engine::ProcessResult result = engine::ProcessExit{.code = 0};
    std::vector<std::string> output;
    std::function<void(const engine::ProcessDesc&)> on_run;
};

bool has_argument(const engine::ProcessDesc& desc, const std::string& argument) {
    return std::ranges::find(desc.arguments, argument) != desc.arguments.end();
}

}

TEST(HttpHeaders, TheStatusOfTheLastResponse) {
    EXPECT_EQ(launcher::parse_http_status("HTTP/2 200 \r\netag: \"a\"\r\n\r\n"), 200);
    EXPECT_EQ(launcher::parse_http_status("HTTP/1.1 304 Not Modified\r\n\r\n"), 304);
    EXPECT_EQ(launcher::parse_http_status("HTTP/1.1 403 Forbidden\nx-ratelimit-remaining: 0\n\n"), 403);
    EXPECT_EQ(launcher::parse_http_status(
                      "HTTP/1.1 302 Found\r\nlocation: https://x\r\n\r\nHTTP/2 200 \r\ncontent-length: 5\r\n\r\n"),
            200);
    EXPECT_EQ(launcher::parse_http_status(""), 0);
    EXPECT_EQ(launcher::parse_http_status("garbage"), 0);
}

TEST(HttpHeaders, AHeaderOfTheLastResponseIgnoringCase) {
    const std::string dump =
            "HTTP/1.1 302 Found\r\nETag: \"redirect\"\r\n\r\n"
            "HTTP/2 200 \r\nETag: W/\"abc\"\r\nLink:   <https://api.github.com/x?page=2>; rel=\"next\"\r\n\r\n";
    EXPECT_EQ(launcher::find_header(dump, "etag"), "W/\"abc\"");
    EXPECT_EQ(launcher::find_header(dump, "LINK"), "<https://api.github.com/x?page=2>; rel=\"next\"");
    EXPECT_EQ(launcher::find_header(dump, "x-missing"), std::nullopt);
    EXPECT_EQ(launcher::find_header("", "etag"), std::nullopt);
}

TEST(CurlArguments, AnApiCallDoesNotFollowRedirectsOrFail) {
    launcher::DownloadRequest request{.url = "https://api.github.com/x", .output = "body.json", .header_output = "h.txt",
            .download = false};
    const std::vector<std::string> args = launcher::curl_arguments(request, {});
    const auto has = [&](const char* text) { return std::ranges::find(args, text) != args.end(); };
    EXPECT_TRUE(has("--silent"));
    EXPECT_TRUE(has("--show-error"));
    EXPECT_TRUE(has("--retry"));
    EXPECT_FALSE(has("--location"));
    EXPECT_FALSE(has("--fail"));
    EXPECT_TRUE(has("--dump-header"));
    EXPECT_EQ(args.back(), "https://api.github.com/x");
}

TEST(CurlArguments, ADownloadFollowsOnlyToHttpsAndFailsOnErrors) {
    launcher::DownloadRequest request{.url = "https://github.com/a/b", .output = "out.bin"};
    const std::vector<std::string> args = launcher::curl_arguments(request, {});
    const auto has = [&](const char* text) { return std::ranges::find(args, text) != args.end(); };
    EXPECT_TRUE(has("--location"));
    EXPECT_TRUE(has("--fail"));
    EXPECT_TRUE(has("--proto-redir"));
    EXPECT_TRUE(has("--proto"));
    EXPECT_TRUE(has("=https"));
    EXPECT_FALSE(has("--dump-header")) << "no header file asked for";
}

TEST(CurlDownloader, TheTokenGoesInAFileNotOnTheCommandLine) {
    const TempDir dir;
    FakeLauncher launcher;
    std::string header_file_text;
    launcher.on_run = [&](const engine::ProcessDesc& desc) {
        // The request-headers file exists while curl runs and is named after `-H @`.
        for (std::size_t i = 0; i + 1 < desc.arguments.size(); ++i) {
            if (desc.arguments[i] == "-H") {
                header_file_text = launcher_test::read_file(desc.arguments[i + 1].substr(1));
            }
        }
    };
    launcher::CurlDownloader downloader(launcher);
    downloader.start(launcher::DownloadRequest{.url = "https://api.github.com/x", .output = dir.path() / "b.json",
            .headers = {"Accept: application/vnd.github+json"}, .bearer_token = "ghp_secret", .download = false});

    ASSERT_EQ(launcher.runs.size(), 1u);
    EXPECT_EQ(launcher.runs[0].program, std::filesystem::path("curl"));
    for (const std::string& argument : launcher.runs[0].arguments) {
        EXPECT_EQ(argument.find("ghp_secret"), std::string::npos) << "the token is on the command line: " << argument;
    }
    EXPECT_NE(header_file_text.find("Authorization: Bearer ghp_secret"), std::string::npos);
    EXPECT_NE(header_file_text.find("Accept: application/vnd.github+json"), std::string::npos);

    const auto answer = downloader.poll();
    ASSERT_TRUE(answer.has_value());
    EXPECT_TRUE(answer->ok());
    EXPECT_FALSE(std::filesystem::exists(dir.path() / "b.json.request-headers")) << "the token file is removed";
    EXPECT_FALSE(downloader.busy());
    EXPECT_EQ(downloader.poll(), std::nullopt) << "answers once";
}

TEST(CurlDownloader, AnswersWithTheStatusOfTheHeaderDump) {
    const TempDir dir;
    FakeLauncher launcher;
    launcher.on_run = [&](const engine::ProcessDesc&) {
        dir.write("h.txt", "HTTP/2 304 \r\n\r\n");
    };
    launcher::CurlDownloader downloader(launcher);
    downloader.start(launcher::DownloadRequest{.url = "https://api.github.com/x", .output = dir.path() / "b.json",
            .header_output = dir.path() / "h.txt", .download = false});
    const auto answer = downloader.poll();
    ASSERT_TRUE(answer.has_value());
    EXPECT_TRUE(answer->ok());
    EXPECT_EQ(answer->status, 304);
}

TEST(CurlDownloader, CurlFailingReportsItsOwnMessage) {
    const TempDir dir;
    FakeLauncher launcher;
    launcher.result = engine::ProcessExit{.code = 22};
    launcher.output = {"curl: (22) The requested URL returned error: 404"};
    launcher::CurlDownloader downloader(launcher);
    downloader.start(launcher::DownloadRequest{.url = "https://github.com/x", .output = dir.path() / "o.bin"});
    const auto answer = downloader.poll();
    ASSERT_TRUE(answer.has_value());
    EXPECT_FALSE(answer->ok());
    EXPECT_EQ(answer->error, "(22) The requested URL returned error: 404");
}

TEST(CurlDownloader, CurlWithoutAMessageStillSaysSomething) {
    const TempDir dir;
    FakeLauncher launcher;
    launcher.result = engine::ProcessExit{.code = 7};
    launcher::CurlDownloader downloader(launcher);
    downloader.start(launcher::DownloadRequest{.url = "https://github.com/x", .output = dir.path() / "o.bin"});
    const auto answer = downloader.poll();
    ASSERT_TRUE(answer.has_value());
    EXPECT_NE(answer->error.find("exit code 7"), std::string::npos);
}

TEST(CurlDownloader, ACurlThatIsNotThereSaysWhatToDo) {
    const TempDir dir;
    FakeLauncher launcher;
    launcher.result = std::unexpected(engine::ProcessError::NotFound);
    launcher::CurlDownloader downloader(launcher);
    downloader.start(launcher::DownloadRequest{.url = "https://github.com/x", .output = dir.path() / "o.bin"});
    const auto answer = downloader.poll();
    ASSERT_TRUE(answer.has_value());
    EXPECT_NE(answer->error.find("curl is not installed"), std::string::npos) << answer->error;
}

TEST(CurlDownloader, AnOldOutputFileIsRemovedBeforeTheTransfer) {
    const TempDir dir;
    dir.write("o.bin", "old");
    FakeLauncher launcher;
    bool existed = true;
    launcher.on_run = [&](const engine::ProcessDesc&) { existed = std::filesystem::exists(dir.path() / "o.bin"); };
    launcher::CurlDownloader downloader(launcher);
    downloader.start(launcher::DownloadRequest{.url = "https://github.com/x", .output = dir.path() / "o.bin"});
    EXPECT_FALSE(existed);
}

TEST(CurlDownloader, NothingStartedAnswersNothing) {
    FakeLauncher launcher;
    launcher::CurlDownloader downloader(launcher);
    EXPECT_FALSE(downloader.busy());
    EXPECT_EQ(downloader.poll(), std::nullopt);
    downloader.cancel();
}
