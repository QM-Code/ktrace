#include "karma/network/http/curl_global.hpp"

#include <cstdlib>
#include <curl/curl.h>
#include <mutex>

namespace karma::network::http {

bool EnsureCurlGlobalInit() {
    static std::once_flag flag;
    static bool initialized = false;
    std::call_once(flag, []() {
        initialized = (curl_global_init(CURL_GLOBAL_DEFAULT) == 0);
        if (initialized) {
            std::atexit([]() { curl_global_cleanup(); });
        }
    });
    return initialized;
}

} // namespace karma::network::http
