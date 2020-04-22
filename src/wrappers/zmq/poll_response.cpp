#include "poll_response.h"

namespace wrappers::zmq {

poll_response::poll_response(socket &sock, poll_event event) noexcept
    : response_socket(&sock), response_event(event) {}

poll_response::poll_response(const poll_response &other) noexcept
    : response_socket(other.response_socket),
      response_event(other.response_event) {}

poll_response &poll_response::operator=(const poll_response &other) noexcept {
    if (this != &other) {
        this->response_socket = other.response_socket;
        this->response_event = other.response_event;
    }

    return *this;
}

poll_response::poll_response(poll_response &&other) noexcept
    : response_socket(other.response_socket),
      response_event(other.response_event) {
    other.response_socket = nullptr;
    other.response_event = {};
}

poll_response &poll_response::operator=(poll_response &&other) noexcept {
    if (this != &other) {
        this->response_socket = other.response_socket;
        other.response_socket = nullptr;
        this->response_event = other.response_event;
        other.response_event = {};
    }

    return *this;
}

} // namespace wrappers::zmq
