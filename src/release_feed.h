#pragma once

#include "downloader.h"
#include "sdk_release.h"

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace launcher {

// What the launcher remembers of the last good answer of GitHub, so the Available list shows at once and offline.
struct ReleaseCache {
    // The ETag of the first page, sent back as `If-None-Match`.
    std::string etag;
    // The day (UTC, `2026-10-11`) of the last answer.
    std::string fetched;
    std::vector<SdkRelease> releases;
};

// A text file: a header line, then `etag`, `fetched`, `platform`, and one `release` line per SDK, tab separated. A
// cache written for another platform, an unknown header, or a damaged line is an empty cache.
[[nodiscard]] std::string format_release_cache(const ReleaseCache& cache, const Platform& platform);
[[nodiscard]] ReleaseCache parse_release_cache(std::string_view text, const Platform& platform);
[[nodiscard]] ReleaseCache load_release_cache(const std::filesystem::path& file, const Platform& platform);
[[nodiscard]] bool save_release_cache(
        const std::filesystem::path& file, const ReleaseCache& cache, const Platform& platform);

// Today's date in UTC, `2026-10-11`.
[[nodiscard]] std::string today_utc();

struct ReleaseFeedOptions {
    Platform platform = current_platform();
    // Where the cache is kept; empty: nothing is remembered.
    std::filesystem::path cache_file;
    // A directory for the downloaded pages. Created when needed.
    std::filesystem::path work_directory;
    // `GITHUB_TOKEN` or `GH_TOKEN`, sent as a bearer token to lift the rate limit; empty: none.
    std::string token;
    std::string repository = "wismh/wind-engine";
    // Stops following pages once this many releases were seen.
    std::size_t max_releases = 500;
    std::function<std::string()> today = today_utc;
};

// The SDK releases of the engine's GitHub repository for this platform: the cached list at once, and refresh() asks
// GitHub (with the cache's ETag) without blocking the frame. When GitHub cannot be asked (no network, the rate limit,
// a bad answer) the cached list stays and note() says why. Main thread only.
class ReleaseFeed {
public:
    ReleaseFeed(IDownloader& downloader, ReleaseFeedOptions options);

    // Reads the cache file. Call once before the first refresh().
    void load_cache();
    // Starts asking GitHub; nothing happens while a refresh runs.
    void refresh();
    // Advances a refresh. Call every frame.
    void poll();

    [[nodiscard]] bool loading() const {
        return loading_;
    }
    // The newest-first list: the answer of GitHub, or the cache.
    [[nodiscard]] const std::vector<SdkRelease>& releases() const {
        return cache_.releases;
    }
    // A line for the page: why the list is the cached one ("offline: showing the list from 2026-10-10"), or empty.
    [[nodiscard]] const std::string& note() const {
        return note_;
    }
    // Grows whenever releases(), note(), or loading() changed.
    [[nodiscard]] unsigned revision() const {
        return revision_;
    }

private:
    void request_page(const std::string& url);
    void page_answered(const DownloadResult& answer);
    void finish_with_cache(const std::string& reason);
    void finish(std::vector<SdkRelease> releases);
    [[nodiscard]] std::string cached_note(const std::string& reason) const;

    IDownloader* downloader_;
    ReleaseFeedOptions options_;
    ReleaseCache cache_;
    bool loading_ = false;
    std::string note_;
    unsigned revision_ = 0;

    // A refresh in progress.
    std::size_t page_ = 0;
    std::size_t seen_ = 0;
    std::string etag_;
    std::vector<SdkRelease> collected_;
    std::filesystem::path body_file_;
    std::filesystem::path header_file_;
};

}
