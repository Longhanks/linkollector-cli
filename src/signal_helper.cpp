#include "signal_helper.h"

namespace linkollector::signal_helper {

static void *s_data = nullptr;

void *static_data() noexcept {
    return s_data;
}

static_data_guard::static_data_guard(void *data) {
    s_data = data;
}

static_data_guard::~static_data_guard() noexcept {
    s_data = nullptr;
}

} // namespace linkollector::signal_helper
