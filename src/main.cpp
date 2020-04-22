#include <array>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <thread>

#include "signal_helper.h"
#include "wrappers/zmq/context.h"
#include "wrappers/zmq/poll.h"
#include "wrappers/zmq/socket.h"

#include <zmq.h>

static void server_loop(wrappers::zmq::context &ctx) {
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
                zmq_msg_t message;
                zmq_msg_init(&message);

                // Use non-blocking so we can continue to check loop_connection
                int rc = zmq_msg_recv(
                    &message, tcp_responder_socket.get(), ZMQ_DONTWAIT);

                if (rc < 0) {
                    if (errno == EAGAIN) {
                        continue;
                    }
                    if (errno == EINTR) {
                        continue;
                    }
                    std::cerr
                        << "Failure in zmq_msg_recv, killing server...\n";
                    return;
                }

                if (!tcp_responder_socket.blocking_send()) {
                    std::cerr
                        << "Failed to send data to the TCP responder socket\n";
                    return;
                }

                const bool send_result = worker_response_socket.blocking_send(
                    zmq_msg_data(&message), zmq_msg_size(&message));

                zmq_msg_close(&message);

                if (!send_result) {
                    std::cerr << "Failed to send data to the worker response "
                                 "socket\n";
                    return;
                }
            }
        }
    }

    std::cout << "Worker thread clean shutdown\n";
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Need -r or -p\n";
        return EXIT_FAILURE;
    }

    std::string arg1(*std::next(argv));

    if (arg1 == "-r") {
        wrappers::zmq::context ctx;

        linkollector::signal_helper::static_data_guard
            signal_static_data_guard(static_cast<void *>(&ctx));

        const auto signal_handler = []([[maybe_unused]] int signal_value) {
            wrappers::zmq::context &ctx_ =
                *static_cast<wrappers::zmq::context *>(
                    linkollector::signal_helper::static_data());

            wrappers::zmq::socket signal_socket(
                ctx_, wrappers::zmq::socket::type::pair);

            if (signal_socket.connect("inproc://signal")) {
                [[maybe_unused]] const bool result =
                    signal_socket.blocking_send();
            }
        };

        wrappers::zmq::socket signal_socket(ctx,
                                            wrappers::zmq::socket::type::pair);
        if (!signal_socket.bind("inproc://signal")) {
            std::cerr << "Failed to bind the SIGINT/SIGTERM socket\n";
            return EXIT_FAILURE;
        }

#ifdef _WIN32
        std::signal(SIGINT, signal_handler);
#else
        struct sigaction action;
        action.sa_handler = signal_handler;
        action.sa_flags = 0;
        sigemptyset(&action.sa_mask);
        sigaction(SIGINT, &action, nullptr);
        sigaction(SIGTERM, &action, nullptr);
#endif

        wrappers::zmq::socket worker_shutdown_socket(
            ctx, wrappers::zmq::socket::type::pair);
        if (!worker_shutdown_socket.bind("inproc://worker_shutdown")) {
            std::cerr << "Failed to bind the worker shutdown socket\n";
            return EXIT_FAILURE;
        }

        wrappers::zmq::socket worker_response_socket(
            ctx, wrappers::zmq::socket::type::pair);
        if (!worker_response_socket.connect("inproc://worker_response")) {
            std::cerr << "Failed to connect the worker response socket\n";
            return EXIT_FAILURE;
        }

        std::thread server(server_loop, std::ref(ctx));

        // wait until server is ready
        const auto maybe_data = worker_shutdown_socket.blocking_receive();
        if (!maybe_data.has_value()) {
            std::cerr << "Failed to receive answer from the worker shutdown "
                         "socket\n";
            return EXIT_FAILURE;
        }

        std::cout << "Press CTRL+C to cancel..." << std::endl;

        std::array<zmq_pollitem_t, 2> items = {
            zmq_pollitem_t{signal_socket.get(), 0, ZMQ_POLLIN, 0},
            zmq_pollitem_t{worker_response_socket.get(), 0, ZMQ_POLLIN, 0},
        };

        while (true) {
            int rc =
                zmq_poll(items.data(), static_cast<int>(items.size()), -1);

            if (rc == 0) {
                continue;
            }

            if (rc < 0) {
                if (errno == EINTR) {
                    continue;
                }
                std::cerr << "Failure in zmq_poll, killing server...\n";
                break;
            }

            if ((items[0].revents > 0) &&
                (static_cast<std::make_unsigned_t<decltype(items[0].revents)>>(
                     items[0].revents) &
                 static_cast<std::make_unsigned_t<decltype(ZMQ_POLLIN)>>(
                     ZMQ_POLLIN)) > 0) {
                if (!signal_socket.blocking_receive()) {
                    std::cerr
                        << "Failed to receive answer from signal socket\n";
                }
                break;
            }

            if ((items[1].revents > 0) &&
                (static_cast<std::make_unsigned_t<decltype(items[1].revents)>>(
                     items[1].revents) &
                 static_cast<std::make_unsigned_t<decltype(ZMQ_POLLIN)>>(
                     ZMQ_POLLIN)) > 0) {
                zmq_msg_t message;
                zmq_msg_init(&message);

                // Use non-blocking so we can continue to check
                // signal_socket
                rc = zmq_msg_recv(
                    &message, worker_response_socket.get(), ZMQ_DONTWAIT);

                if (rc < 0) {
                    if (errno == EAGAIN) {
                        continue;
                    }
                    if (errno == EINTR) {
                        continue;
                    }
                    std::cerr
                        << "Failure in zmq_msg_recv, killing server...\n";
                    break;
                }

                std::string data(static_cast<char *>(zmq_msg_data(&message)),
                                 zmq_msg_size(&message));
                zmq_msg_close(&message);
                std::cout << "Received \"" << data << "\" from client\n";
            }
        }

        if (!worker_shutdown_socket.blocking_send()) {
            std::cerr << "Failed to send data to the worker shutdown socket\n";
        }
        server.join();

        std::cout << "Main thread clean shutdown\n";
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

        std::cout << "Sending \"" << data << "\" to hello world server...\n";

        void *context = zmq_ctx_new();
        void *requester = zmq_socket(context, ZMQ_REQ);
        zmq_connect(requester, "tcp://localhost:17729");

        zmq_msg_t message;
        zmq_msg_init_data(&message,
                          static_cast<void *>(data.data()),
                          data.size(),
                          nullptr,
                          nullptr);
        zmq_msg_send(&message, requester, 0);
        zmq_recv(requester, nullptr, 0, 0);

        zmq_close(requester);
        zmq_ctx_destroy(context);
    }

    else {
        std::cerr << "Unknown option " << arg1 << "\n";
        return EXIT_FAILURE;
    }
}
