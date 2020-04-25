#include "poll.h"

#include "socket.h"
#include "../../macros.h"

#include <zmq.h>

#include <algorithm>

namespace wrappers::zmq {

[[nodiscard]] static constexpr decltype(zmq_pollitem_t::events)
to_zmq_event_type(poll_event event) {
    switch (event) {
    case poll_event::in: {
        return ZMQ_POLLIN;
    }
    case poll_event::out: {
        return ZMQ_POLLOUT;
    }
    }
    LINKOLLECTOR_UNREACHABLE;
}

std::optional<std::vector<poll_response>>
blocking_poll(gsl::span<poll_target> targets) noexcept {
    std::vector<zmq_pollitem_t> zmq_targets;

    std::transform(std::begin(targets),
                   std::end(targets),
                   std::back_inserter(zmq_targets),
                   [](const poll_target &target) -> zmq_pollitem_t {
                       return zmq_pollitem_t{
                           target.target_socket->m_socket,
                           0,
                           to_zmq_event_type(target.target_event),
                           0};
                   });

    const auto zmq_rc = zmq_poll(zmq_targets.data(),
                                 static_cast<int>(zmq_targets.size()),
                                 /* timeout: */ -1);

    std::vector<poll_response> response;

    if (zmq_rc == 0) {
        return {std::move(response)};
    }

    if (zmq_rc == -1) {
        const auto errnum = errno;
        if (errnum == EINTR) {
            return {std::move(response)};
        }
        return std::nullopt;
    }

    for (std::size_t i = 0; i != targets.size(); ++i) {
        if (zmq_targets[i].revents > 0) {
            const auto zmq_unsigned_received_events = static_cast<
                std::make_unsigned_t<decltype(zmq_pollitem_t::revents)>>(
                zmq_targets[i].revents);

            if ((zmq_unsigned_received_events &
                 static_cast<std::make_unsigned_t<decltype(ZMQ_POLLIN)>>(
                     ZMQ_POLLIN)) > 0) {
                response.emplace_back(*targets[i].target_socket,
                                      poll_event::in);
            } else if ((zmq_unsigned_received_events &
                        static_cast<
                            std::make_unsigned_t<decltype(ZMQ_POLLOUT)>>(
                            ZMQ_POLLOUT)) > 0) {
                response.emplace_back(*targets[i].target_socket,
                                      poll_event::out);
            }
        }
    }

    return {std::move(response)};
}

} // namespace wrappers::zmq
