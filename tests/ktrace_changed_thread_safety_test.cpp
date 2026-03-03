#include <ktrace.hpp>

#include <atomic>
#include <string>
#include <thread>
#include <vector>

namespace {

std::string MakeChangeKey(const int thread_index, const int iteration) {
    return std::to_string(thread_index) + ":" + std::to_string(iteration & 1);
}

void EmitChangedTrace(const int thread_index, const int iteration) {
    KTRACE_CHANGED("changed", MakeChangeKey(thread_index, iteration), "changed");
}

} // namespace

int main() {
    constexpr int kThreadCount = 8;
    constexpr int kIterationsPerThread = 20000;

    std::atomic<int> ready_threads{0};
    std::atomic<bool> start{false};

    std::vector<std::thread> workers;
    workers.reserve(kThreadCount);

    for (int thread_index = 0; thread_index < kThreadCount; ++thread_index) {
        workers.emplace_back([thread_index, &ready_threads, &start]() {
            ready_threads.fetch_add(1, std::memory_order_release);
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            for (int iteration = 0; iteration < kIterationsPerThread; ++iteration) {
                EmitChangedTrace(thread_index, iteration);
            }
        });
    }

    while (ready_threads.load(std::memory_order_acquire) < kThreadCount) {
        std::this_thread::yield();
    }
    start.store(true, std::memory_order_release);

    for (std::thread& worker : workers) {
        worker.join();
    }

    return 0;
}
