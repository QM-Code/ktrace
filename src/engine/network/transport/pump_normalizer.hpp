#pragma once

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

namespace karma::network::transport::detail {

template <typename Event, typename EventType, typename KeyFn>
size_t NormalizePumpEventsPerKey(std::vector<Event>* out_events,
                                 std::vector<Event>* staged_events,
                                 EventType connected_type,
                                 EventType received_type,
                                 EventType disconnected_type,
                                 KeyFn key_fn) {
    if (!out_events || !staged_events || staged_events->empty()) {
        return 0;
    }

    using Key = decltype(key_fn((*staged_events)[0]));
    std::vector<Key> key_order{};
    key_order.reserve(staged_events->size());
    for (const auto& event : *staged_events) {
        const auto key = key_fn(event);
        if (std::find(key_order.begin(), key_order.end(), key) == key_order.end()) {
            key_order.push_back(key);
        }
    }

    std::vector<bool> emitted_mask(staged_events->size(), false);
    size_t emitted = 0;
    auto append_by_key_and_type = [&](const Key& key, EventType type) {
        for (size_t idx = 0; idx < staged_events->size(); ++idx) {
            if (emitted_mask[idx]) {
                continue;
            }
            auto& event = (*staged_events)[idx];
            if (key_fn(event) == key && event.type == type) {
                out_events->push_back(std::move(event));
                emitted_mask[idx] = true;
                ++emitted;
            }
        }
    };

    for (const auto& key : key_order) {
        append_by_key_and_type(key, connected_type);
        append_by_key_and_type(key, received_type);
        append_by_key_and_type(key, disconnected_type);
    }

    staged_events->clear();
    return emitted;
}

template <typename Event, typename EventType>
size_t NormalizePumpEvents(std::vector<Event>* out_events,
                           std::vector<Event>* staged_events,
                           EventType connected_type,
                           EventType received_type,
                           EventType disconnected_type) {
    return NormalizePumpEventsPerKey(out_events,
                                     staged_events,
                                     connected_type,
                                     received_type,
                                     disconnected_type,
                                     [](const Event&) { return 0u; });
}

} // namespace karma::network::transport::detail
