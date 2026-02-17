#include "server/net/transport_event_source.hpp"
#include "server/net/transport_event_source/internal.hpp"

namespace bz3::server::net {

std::unique_ptr<ServerEventSource> CreateServerTransportEventSource(uint16_t port,
                                                                    std::string_view app_name) {
    auto transport_source = std::make_unique<detail::TransportServerEventSource>(
        port,
        app_name.empty() ? std::string("server") : std::string(app_name));
    if (!transport_source->initialized()) {
        return nullptr;
    }
    return transport_source;
}

} // namespace bz3::server::net
