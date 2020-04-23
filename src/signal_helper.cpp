#include "signal_helper.h"

#include <csignal>

namespace linkollector::signal_helper {

static void *s_data = nullptr;
static void (*s_callback)(void *) = nullptr;

static void signal_handler([[maybe_unused]] int signal_value) noexcept {
    s_callback(s_data);
}

sigint_guard::sigint_guard(void *data, void (*callback)(void *)) {
    s_data = data;
    s_callback = callback;

#ifdef _WIN32
    std::signal(SIGINT, signal_handler);
#else
    struct sigaction action;
    action.sa_handler = signal_handler;
    action.sa_flags = 0;
    sigemptyset(&action.sa_mask);
    sigaction(SIGINT, &action, nullptr);
    sigaction(SIGTERM, &action, nullptr);
#endif
}

sigint_guard::~sigint_guard() noexcept {
    s_data = nullptr;
    s_callback = nullptr;
}

} // namespace linkollector::signal_helper
