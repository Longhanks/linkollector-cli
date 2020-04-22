#include "poll_target.h"

namespace wrappers::zmq {

poll_target::poll_target(socket &sock, poll_event event) noexcept
    : target_socket(&sock), target_event(event) {}

poll_target::poll_target(const poll_target &other) noexcept
    : target_socket(other.target_socket), target_event(other.target_event) {}

poll_target &poll_target::operator=(const poll_target &other) noexcept {
    if (this != &other) {
        this->target_socket = other.target_socket;
        this->target_event = other.target_event;
    }

    return *this;
}

poll_target::poll_target(poll_target &&other) noexcept
    : target_socket(other.target_socket), target_event(other.target_event) {
    other.target_socket = nullptr;
    other.target_event = {};
}

poll_target &poll_target::operator=(poll_target &&other) noexcept {
    if (this != &other) {
        this->target_socket = other.target_socket;
        other.target_socket = nullptr;
        this->target_event = other.target_event;
        other.target_event = {};
    }

    return *this;
}

} // namespace wrappers::zmq
