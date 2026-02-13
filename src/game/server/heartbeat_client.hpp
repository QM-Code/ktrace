#pragma once

#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

namespace bz3::server {

class IHeartbeatClient {
 public:
    virtual ~IHeartbeatClient() = default;

    virtual void requestHeartbeat(const std::string& community_url,
                                  const std::string& server_address,
                                  int players,
                                  int max_players) = 0;
};

class HeartbeatClient final : public IHeartbeatClient {
 public:
    HeartbeatClient();
    ~HeartbeatClient();

    void requestHeartbeat(const std::string& community_url,
                          const std::string& server_address,
                          int players,
                          int max_players) override;

 private:
    struct Request {
        std::string community_url{};
        std::string server_address{};
        int players = 0;
        int max_players = 0;
    };

    void startWorker();
    void stopWorker();
    void workerProc();

    std::deque<Request> requests_{};
    std::mutex mutex_{};
    std::condition_variable cv_{};
    std::thread worker_{};
    bool stop_requested_ = false;
};

} // namespace bz3::server
