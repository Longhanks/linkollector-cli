#include <array>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <tuple>

#include "signal_helper.h"
#include "wrappers/zmq/context.h"
#include "wrappers/zmq/poll.h"
#include "wrappers/zmq/socket.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Need -r or -p\n";
        return EXIT_FAILURE;
    }

    std::string arg1(*std::next(argv));

    wrappers::zmq::context ctx;

    linkollector::signal_helper::sigint_guard guard(
        static_cast<void *>(&ctx), [](void *data) {
            wrappers::zmq::context &ctx_ =
                *static_cast<wrappers::zmq::context *>(data);
            wrappers::zmq::socket signal_socket(
                ctx_, wrappers::zmq::socket::type::pair);

            if (signal_socket.connect("inproc://signal")) {
                [[maybe_unused]] const bool result =
                    signal_socket.blocking_send();
            }
        });

    wrappers::zmq::socket signal_socket(ctx,
                                        wrappers::zmq::socket::type::pair);
    if (!signal_socket.bind("inproc://signal")) {
        std::cerr << "Failed to bind the SIGINT/SIGTERM socket\n";
        return EXIT_FAILURE;
    }

    if (arg1 == "-r") {
        wrappers::zmq::socket tcp_responder_socket(
            ctx, wrappers::zmq::socket::type::rep);
        if (!tcp_responder_socket.bind("tcp://*:17729")) {
            std::cerr << "Failed to bind the TCP responder socket\n";
            return EXIT_FAILURE;
        }

        std::cout << "Press CTRL+C to cancel..." << std::endl;

        std::array<wrappers::zmq::poll_target, 2> items = {
            wrappers::zmq::poll_target(signal_socket,
                                       wrappers::zmq::poll_event::in),
            wrappers::zmq::poll_target(tcp_responder_socket,
                                       wrappers::zmq::poll_event::in),
        };

        bool break_loop = false;

        while (!break_loop) {
            auto maybe_responses = wrappers::zmq::blocking_poll(items);

            if (!maybe_responses.has_value()) {
                std::cerr << "Failure in zmq_poll, killing server...\n";
                break;
            }

            const auto responses = std::move(*maybe_responses);

            if (responses.empty()) {
                continue;
            }

            for (const auto &response : responses) {
                if (response.response_socket == &signal_socket &&
                    response.response_event == wrappers::zmq::poll_event::in) {
                    if (!signal_socket.blocking_receive()) {
                        std::cerr
                            << "Failed to receive answer from signal socket\n";
                    }
                    break_loop = true;
                    break;
                }

                if (response.response_socket == &tcp_responder_socket &&
                    response.response_event == wrappers::zmq::poll_event::in) {
                    using payload_t = std::
                        tuple<wrappers::zmq::socket &, std::string &, bool &>;

                    std::string data;
                    bool did_error;

                    payload_t payload(tcp_responder_socket, data, did_error);

                    const auto on_message = [](void *payload_,
                                               gsl::span<std::byte> msg) {
                        auto &[tcp_responder_socket_, data_, did_error_] =
                            *static_cast<payload_t *>(payload_);

                        if (!tcp_responder_socket_.blocking_send()) {
                            std::cerr << "Failed to send data to the TCP "
                                         "responder socket\n";
                            did_error_ = true;
                            return;
                        }

                        if (msg.empty()) {
                            return;
                        }

                        auto *chars = static_cast<char *>(
                            static_cast<void *>(msg.data()));
                        data_ = std::string(chars, msg.size());
                    };

                    if (did_error) {
                        break_loop = true;
                        break;
                    }

                    if (!tcp_responder_socket.async_receive(
                            static_cast<void *>(&payload), on_message)) {
                        std::cerr
                            << "Failure in zmq_msg_recv, killing server...\n";
                        break_loop = true;
                        break;
                    }

                    if (data.empty()) {
                        std::cout << "Received empty message from client\n";
                    } else {
                        std::cout << "Received \"" << data
                                  << "\" from client\n";
                    }
                }
            }
        }
    }

    else if (arg1 == "-p") {
        if (argc < 3) {
            std::cerr << "Need a message to send\n";
            return EXIT_FAILURE;
        }

        std::string data(*std::next(argv, 2));

        if (data.empty()) {
            std::cerr << "Message cannot be empty\n";
            return EXIT_FAILURE;
        }

        wrappers::zmq::socket tcp_requester_socket(
            ctx, wrappers::zmq::socket::type::req);
        if (!tcp_requester_socket.connect("tcp://localhost:17729")) {
            std::cerr << "Failed to connect the TCP requester socket\n";
            return EXIT_FAILURE;
        }

        if (!tcp_requester_socket.blocking_send(
                {static_cast<std::byte *>(static_cast<void *>(data.data())),
                 data.size()})) {
            std::cerr << "Failed to send data to the TCP requester socket\n";
            return EXIT_FAILURE;
        }

        std::cout << "Sending \"" << data << "\" to hello world server...\n";

        std::array<wrappers::zmq::poll_target, 2> items = {
            wrappers::zmq::poll_target(signal_socket,
                                       wrappers::zmq::poll_event::in),
            wrappers::zmq::poll_target(tcp_requester_socket,
                                       wrappers::zmq::poll_event::in),
        };

        bool break_loop = false;

        while (!break_loop) {
            auto maybe_responses = wrappers::zmq::blocking_poll(items);

            if (!maybe_responses.has_value()) {
                std::cerr << "Failure in zmq_poll, killing client...\n";
                break;
            }

            const auto responses = std::move(*maybe_responses);

            if (responses.empty()) {
                continue;
            }

            for (const auto &response : responses) {
                if (response.response_socket == &signal_socket &&
                    response.response_event == wrappers::zmq::poll_event::in) {

                    if (!signal_socket.blocking_receive()) {
                        std::cerr
                            << "Failed to receive answer from signal socket\n";
                    }
                    break_loop = true;
                    break;
                }

                if (response.response_socket == &tcp_requester_socket &&
                    response.response_event == wrappers::zmq::poll_event::in) {

                    if (!tcp_requester_socket.async_receive(nullptr,
                                                            nullptr)) {
                        std::cerr
                            << "Failure in zmq_msg_recv, killing client...\n";
                    }

                    break_loop = true;
                    break;
                }
            }
        }
    }

    else {
        std::cerr << "Unknown option " << arg1 << "\n";
        return EXIT_FAILURE;
    }
}
