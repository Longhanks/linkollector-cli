#pragma once

#include "poll_event.h"

namespace wrappers::zmq {

class socket;

struct poll_response final {
    explicit poll_response(socket &sock, poll_event event) noexcept;
    poll_response(const poll_response &other) noexcept;
    poll_response &operator=(const poll_response &other) noexcept;
    poll_response(poll_response &&other) noexcept;
    poll_response &operator=(poll_response &&other) noexcept;
    ~poll_response() = default;

    socket *response_socket;
    poll_event response_event;
};

} // namespace wrappers::zmq
