#pragma once

#include "poll_event.h"
#include "poll_response.h"
#include "poll_target.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include <gsl/span>

namespace wrappers::zmq {

class context;

class socket final {

public:
    enum class type {
        pair,
        pub,
        sub,
        req,
        rep,
        dealer,
        router,
        pull,
        push,
        xpub,
        xsub,
        stream,
    };

    explicit socket(context &ctx, type socket_type) noexcept;
    socket(const socket &other) = delete;
    socket &operator=(const socket &other) = delete;
    socket(socket &&other) noexcept;
    socket &operator=(socket &&other) noexcept;
    ~socket() noexcept;

    [[nodiscard]] bool bind(const std::string &endpoint) noexcept;

    [[nodiscard]] bool connect(const std::string &endpoint) noexcept;

    [[nodiscard]] bool blocking_send() noexcept;
    [[nodiscard]] bool blocking_send(gsl::span<std::byte> message) noexcept;

    [[nodiscard]] std::optional<std::vector<std::byte>>
    blocking_receive() noexcept;

    [[nodiscard]] bool
    async_receive(void *data,
                  void (*callback)(void *, gsl::span<std::byte>)) noexcept;

    friend std::optional<std::vector<poll_response>>
    blocking_poll(gsl::span<poll_target> targets) noexcept;

private:
    void *m_socket = nullptr;
};

} // namespace wrappers::zmq
