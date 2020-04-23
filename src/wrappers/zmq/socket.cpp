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
    : m_socket(zmq_socket(ctx.m_context, to_zmq_socket_type(socket_type))) {}

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
    return blocking_send(nullptr, 0);
}

bool socket::blocking_send(const void *data, std::size_t size) noexcept {
    if (data == nullptr) {
        return zmq_send(this->m_socket, nullptr, 0, /* flags: */ 0) != -1;
    }

    zmq_msg_t msg;
    zmq_msg_init_size(&msg, size);
    std::memcpy(zmq_msg_data(&msg), data, size);

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

    std::vector<std::byte> buf(data, data + zmq_msg_size(&msg));
    return {std::move(buf)};
}

} // namespace wrappers::zmq
