#pragma once

#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "karma/graphics/texture_handle.hpp"

namespace ui {

struct ThumbnailTexture {
    graphics::TextureHandle texture{};
    bool failed = false;
    bool loading = false;
};

class ThumbnailCache {
public:
    ThumbnailCache() = default;
    ~ThumbnailCache();

    ThumbnailTexture *getOrLoad(const std::string &url);
    void processUploads();
    void shutdown();

private:
    struct ThumbnailPayload {
        std::string url;
        int width = 0;
        int height = 0;
        bool failed = true;
        std::vector<unsigned char> pixels;
    };

    void clearTextures();
    void startWorker();
    void stopWorker();
    void queueRequest(const std::string &url);
    void workerProc();

    std::unordered_map<std::string, ThumbnailTexture> cache;
    std::deque<std::string> requests;
    std::deque<ThumbnailPayload> results;
    std::unordered_set<std::string> inFlight;
    std::mutex mutex;
    std::condition_variable cv;
    std::thread worker;
    bool workerStop = false;
};

} // namespace ui
