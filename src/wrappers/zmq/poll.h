#pragma once

#include "poll_event.h"
#include "poll_response.h"
#include "poll_target.h"

#include <gsl/span>

#include <optional>
#include <vector>

namespace wrappers::zmq {

[[nodiscard]] std::optional<std::vector<poll_response>>
blocking_poll(gsl::span<poll_target> targets) noexcept;

} // namespace wrappers::zmq
