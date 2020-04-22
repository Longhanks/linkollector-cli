#pragma once

namespace linkollector::signal_helper {

[[nodiscard]] void *static_data() noexcept;

struct static_data_guard final {
    explicit static_data_guard(void *data);
    static_data_guard(const static_data_guard &other) = delete;
    static_data_guard &operator=(const static_data_guard &other) = delete;
    static_data_guard(static_data_guard &&other) noexcept = default;
    static_data_guard &operator=(static_data_guard &&other) noexcept = default;
    ~static_data_guard() noexcept;
};

} // namespace linkollector::signal_helper
