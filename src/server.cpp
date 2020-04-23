#include "server.h"

#include "wrappers/zmq/poll.h"
#include "wrappers/zmq/socket.h"

#include <array>
#include <iostream>
#include <tuple>

namespace linkollector::server {

using on_message_payload =
    std::tuple<wrappers::zmq::socket &, wrappers::zmq::socket &, bool &>;

static void on_message(void *data, gsl::span<std::byte> msg) noexcept;

void loop(wrappers::zmq::context &ctx) noexcept {
    wrappers::zmq::socket tcp_responder_socket(
        ctx, wrappers::zmq::socket::type::rep);
    if (!tcp_responder_socket.bind("tcp://*:17729")) {
        std::cerr << "Failed to bind the TCP responder socket\n";
        return;
    }

    wrappers::zmq::socket worker_response_socket(
        ctx, wrappers::zmq::socket::type::pair);
    if (!worker_response_socket.bind("inproc://worker_response")) {
        std::cerr << "Failed to bind the worker response socket\n";
        return;
    }

    wrappers::zmq::socket worker_shutdown_socket(
        ctx, wrappers::zmq::socket::type::pair);
    if (!worker_shutdown_socket.connect("inproc://worker_shutdown")) {
        std::cerr << "Failed to connect the worker shutdown socket\n";
        return;
    }

    if (!worker_shutdown_socket.blocking_send()) {
        std::cerr << "Failed to send data to the worker shutdown socket\n";
        return;
    }

    std::array<wrappers::zmq::poll_target, 2> items = {
        wrappers::zmq::poll_target(worker_shutdown_socket,
                                   wrappers::zmq::poll_event::in),
        wrappers::zmq::poll_target(tcp_responder_socket,
                                   wrappers::zmq::poll_event::in),
    };

    bool break_loop = false;

    while (!break_loop) {
        auto maybe_responses = wrappers::zmq::blocking_poll(items);

        if (!maybe_responses.has_value()) {
            std::cerr << "Failure in zmq_poll, killing server...\n";
            return;
        }

        const auto responses = std::move(*maybe_responses);

        if (responses.empty()) {
            continue;
        }

        for (const auto &response : responses) {
            if (response.response_socket == &worker_shutdown_socket &&
                response.response_event == wrappers::zmq::poll_event::in) {

                const auto maybe_data =
                    worker_shutdown_socket.blocking_receive();
                if (!maybe_data.has_value()) {
                    std::cerr << "Failed to receive answer from the worker "
                                 "shutdown socket\n";
                    return;
                }
                break_loop = true;
                break;
            }

            if (response.response_socket == &tcp_responder_socket &&
                response.response_event == wrappers::zmq::poll_event::in) {

                bool did_error = false;
                on_message_payload payload(
                    tcp_responder_socket, worker_response_socket, did_error);

                if (!tcp_responder_socket.async_receive(&payload,
                                                        on_message)) {
                    std::cerr
                        << "Failure in zmq_msg_recv, killing server...\n";
                    return;
                }

                if (did_error) {
                    return;
                }
            }
        }
    }

    std::cout << "Worker thread clean shutdown\n";
}

static void on_message(void *data, gsl::span<std::byte> msg) noexcept {
    auto &[tcp_responder_socket, worker_response_socket, did_error] =
        *static_cast<on_message_payload *>(data);

    if (!tcp_responder_socket.blocking_send()) {
        std::cerr << "Failed to send data to the TCP responder socket\n";
        did_error = true;
        return;
    }

    const bool send_result = worker_response_socket.blocking_send(msg);
    if (!send_result) {
        std::cerr << "Failed to send data to the worker response socket\n";
        did_error = true;
        return;
    }
}

} // namespace linkollector::server
