#pragma once

namespace linkollector::signal_helper {

struct sigint_guard final {
    explicit sigint_guard(void *data, void (*callback)(void *));
    sigint_guard(const sigint_guard &other) = delete;
    sigint_guard &operator=(const sigint_guard &other) = delete;
    sigint_guard(sigint_guard &&other) noexcept = default;
    sigint_guard &operator=(sigint_guard &&other) noexcept = default;
    ~sigint_guard() noexcept;
};

} // namespace linkollector::signal_helper
