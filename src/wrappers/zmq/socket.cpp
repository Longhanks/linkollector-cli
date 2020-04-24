#include "socket.h"

#include "context.h"

#include <zmq.h>

#include <cstring>

namespace wrappers::zmq {

[[nodiscard]] static constexpr decltype(ZMQ_PAIR)
to_zmq_socket_type(socket::type socket_type) {
    switch (socket_type) {
    case socket::type::pair: {
        return ZMQ_PAIR;
    }
    case socket::type::pub: {
        return ZMQ_PUB;
    }
    case socket::type::sub: {
        return ZMQ_SUB;
    }
    case socket::type::req: {
        return ZMQ_REQ;
    }
    case socket::type::rep: {
        return ZMQ_REP;
    }
    case socket::type::dealer: {
        return ZMQ_DEALER;
    }
    case socket::type::router: {
        return ZMQ_ROUTER;
    }
    case socket::type::pull: {
        return ZMQ_PULL;
    }
    case socket::type::push: {
        return ZMQ_PUSH;
    }
    case socket::type::xpub: {
        return ZMQ_XPUB;
    }
    case socket::type::xsub: {
        return ZMQ_XSUB;
    }
    case socket::type::stream: {
        return ZMQ_STREAM;
    }
    }
}

socket::socket(context &ctx, type socket_type) noexcept
    : m_socket(zmq_socket(ctx.m_context, to_zmq_socket_type(socket_type))) {
    constexpr const int no_linger = 0;
    zmq_setsockopt(
        this->m_socket, ZMQ_LINGER, &no_linger, sizeof(decltype(no_linger)));
}

socket::socket(socket &&other) noexcept {
    this->m_socket = other.m_socket;
    other.m_socket = nullptr;
}

socket &socket::operator=(socket &&other) noexcept {
    if (this != &other) {
        if (this->m_socket != nullptr) {
            zmq_close(this->m_socket);
        }

        this->m_socket = other.m_socket;
        other.m_socket = nullptr;
    }

    return *this;
}

socket::~socket() noexcept {
    if (this->m_socket != nullptr) {
        zmq_close(this->m_socket);
        this->m_socket = nullptr;
    }
}

bool socket::bind(const std::string &endpoint) noexcept {
    return zmq_bind(this->m_socket, endpoint.c_str()) == 0;
}

bool socket::connect(const std::string &endpoint) noexcept {
    return zmq_connect(this->m_socket, endpoint.c_str()) == 0;
}

bool socket::blocking_send() noexcept {
    return blocking_send({});
}

bool socket::blocking_send(gsl::span<std::byte> message) noexcept {
    if (message.empty()) {
        return zmq_send(this->m_socket, nullptr, 0, /* flags: */ 0) != -1;
    }

    zmq_msg_t msg;
    zmq_msg_init_size(&msg, message.size());
    std::memcpy(zmq_msg_data(&msg),
                static_cast<void *>(message.data()),
                message.size());

    if (zmq_msg_send(&msg, this->m_socket, /* flags: */ 0) == -1) {
        zmq_msg_close(&msg);
        return false;
    }

    return true;
}

std::optional<std::vector<std::byte>> socket::blocking_receive() noexcept {
    zmq_msg_t msg;
    zmq_msg_init(&msg);

    if (zmq_msg_recv(&msg, this->m_socket, /* flags: */ 0) == -1) {
        zmq_msg_close(&msg);
        return std::nullopt;
    }

    auto *data = static_cast<std::byte *>(zmq_msg_data(&msg));

    auto *data_end = data;
    std::advance(data_end, zmq_msg_size(&msg));

    std::vector<std::byte> buf(data, data_end);
    return {std::move(buf)};
}

bool socket::async_receive(void *data,
                           void (*callback)(void *,
                                            gsl::span<std::byte>)) noexcept {
    zmq_msg_t msg;
    zmq_msg_init(&msg);

    const auto zmq_rc = zmq_msg_recv(&msg, this->m_socket, ZMQ_DONTWAIT);

    if (zmq_rc == 0) {
        // Success, but empty message
        if (callback != nullptr) {
            callback(data, {});
        }
        zmq_msg_close(&msg);
        return true;
    }

    if (zmq_rc == -1) {
        zmq_msg_close(&msg);

        // If EAGAIN or EINTR, caller should try again
        return errno == EAGAIN || errno == EINTR;
    }

    gsl::span<std::byte> buf(static_cast<std::byte *>(zmq_msg_data(&msg)),
                             zmq_msg_size(&msg));
    if (callback != nullptr) {
        callback(data, buf);
    }
    zmq_msg_close(&msg);
    return true;
}

} // namespace wrappers::zmq
