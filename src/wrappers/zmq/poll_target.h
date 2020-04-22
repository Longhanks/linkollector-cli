#pragma once

#include "poll_event.h"

namespace wrappers::zmq {

class socket;

struct poll_target final {
    explicit poll_target(socket &sock, poll_event event) noexcept;
    poll_target(const poll_target &other) noexcept;
    poll_target &operator=(const poll_target &other) noexcept;
    poll_target(poll_target &&other) noexcept;
    poll_target &operator=(poll_target &&other) noexcept;
    ~poll_target() = default;

    socket *target_socket;
    poll_event target_event;
};

} // namespace wrappers::zmq
