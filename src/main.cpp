#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>

#include "macros.h"
#include "signal_helper.h"
#include "wrappers/zmq/context.h"
#include "wrappers/zmq/poll.h"
#include "wrappers/zmq/socket.h"

#include <gsl/span>

enum class activity { url, text };

[[nodiscard]] static std::string
activity_to_string(activity activity_) noexcept {
    switch (activity_) {
    case activity::url: {
        return "URL";
    }
    case activity::text: {
        return "TEXT";
    }
    }
    LINKOLLECTOR_UNREACHABLE;
}

[[nodiscard]] static std::optional<activity>
activity_from_string(const std::string_view activity_) noexcept {
    std::string lowercase_activity;
    std::transform(
        std::begin(activity_),
        std::end(activity_),
        std::back_inserter(lowercase_activity),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lowercase_activity == "url") {
        return activity::url;
    }
    if (lowercase_activity == "text") {
        return activity::text;
    }
    return std::nullopt;
}

constexpr std::string_view activity_delimiter = "\uedfd";
constexpr std::array<std::byte, activity_delimiter.size()>
    activity_delimiter_bin = []() {
        std::array<std::byte, activity_delimiter.size()> delim = {};
        for (std::size_t i = 0; i < activity_delimiter.size(); ++i) {
            delim.at(i) = static_cast<std::byte>(activity_delimiter[i]);
        }
        return delim;
    }();

static std::optional<std::pair<activity, std::string>>
deserialize(gsl::span<std::byte> msg) noexcept {
    const auto delimiter_begin =
        std::search(std::begin(msg),
                    std::end(msg),
                    std::begin(activity_delimiter_bin),
                    std::end(activity_delimiter_bin));

    if (delimiter_begin == std::begin(msg) ||
        delimiter_begin == std::end(msg)) {
        return std::nullopt;
    }

    const auto delimiter_end = [&delimiter_begin]() {
        auto begin_ = delimiter_begin;
        std::advance(begin_, activity_delimiter.size());
        return begin_;
    }();

    if (std::distance(delimiter_end, std::end(msg)) == 0) {
        return std::nullopt;
    }

    const auto activity_string =
        std::string(static_cast<char *>(static_cast<void *>(msg.data())),
                    static_cast<std::size_t>(
                        std::distance(std::begin(msg), delimiter_begin)));

    auto maybe_activity = activity_from_string(activity_string);

    if (!maybe_activity.has_value()) {
        return std::nullopt;
    }

    auto activity_ = *maybe_activity;
    auto string_ = std::string(
        static_cast<char *>(static_cast<void *>(&(*delimiter_end))),
        static_cast<std::size_t>(std::distance(delimiter_end, std::end(msg))));

    return {std::make_pair(activity_, std::move(string_))};
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Need -r or -s\n";
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
                    using payload_t = std::tuple<
                        wrappers::zmq::socket &,
                        std::optional<std::pair<activity, std::string>> &,
                        bool &>;

                    std::optional<std::pair<activity, std::string>> maybe_data;
                    bool did_error = false;

                    payload_t payload(
                        tcp_responder_socket, maybe_data, did_error);

                    const auto on_message = [](void *payload_,
                                               gsl::span<std::byte> msg) {
                        auto &[tcp_responder_socket_,
                               maybe_data_,
                               did_error_] =
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

                        maybe_data_ = deserialize(msg);
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

                    if (!maybe_data.has_value()) {
                        std::cout << "Could not parse message from client\n";
                        continue;
                    }

                    auto data = std::move(*maybe_data);
                    const auto activity_ = data.first;
                    const auto string_ = std::move(data.second);

                    std::cout << "Received " << activity_to_string(activity_)
                              << "\n:" << string_ << "\n";
                }
            }
        }
    }

    else if (arg1 == "-s") {
        if (argc < 5) {
            std::cerr << "Need a server name, a message type (url or text) "
                         "and a message to send\n";
            return EXIT_FAILURE;
        }

        std::string server(*std::next(argv, 2));
        auto maybe_activity = activity_from_string(*std::next(argv, 3));
        std::string message(*std::next(argv, 4));

        if (server.empty()) {
            std::cerr << "Server cannot be empty\n";
            return EXIT_FAILURE;
        }

        if (!maybe_activity.has_value()) {
            std::cerr << "Message type must be url or text\n";
            return EXIT_FAILURE;
        }

        if (message.empty()) {
            std::cerr << "Message cannot be empty\n";
            return EXIT_FAILURE;
        }

        wrappers::zmq::socket tcp_requester_socket(
            ctx, wrappers::zmq::socket::type::req);
        if (!tcp_requester_socket.connect("tcp://" + server + ":17729")) {
            std::cerr << "Failed to connect the TCP requester socket\n";
            return EXIT_FAILURE;
        }

        std::string data;
        data += activity_to_string(*maybe_activity);
        data += activity_delimiter;
        data += message;

        if (!tcp_requester_socket.blocking_send(
                {static_cast<std::byte *>(static_cast<void *>(data.data())),
                 data.size()})) {
            std::cerr << "Failed to send data to the TCP requester socket\n";
            return EXIT_FAILURE;
        }

        std::cout << "Sending " << activity_to_string(*maybe_activity) << " \""
                  << message << "\" to hello world server...\n";

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
