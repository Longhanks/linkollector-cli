#include <array>
#include <cstdlib>
#include <iostream>
#include <thread>

#include <zmq.h>

static void server_loop(void *context) {
    void *responder = zmq_socket(context, ZMQ_REP);
    zmq_bind(responder, "tcp://*:17729");

    void *loop_connection = zmq_socket(context, ZMQ_PAIR);
    zmq_connect(loop_connection, "inproc://server_loop");
    zmq_send(loop_connection, nullptr, 0, 0);

    std::array<zmq_pollitem_t, 2> items = {
        zmq_pollitem_t{loop_connection, 0, ZMQ_POLLIN, 0},
        zmq_pollitem_t{responder, 0, ZMQ_POLLIN, 0},
    };

    while (true) {
        int rc = zmq_poll(items.data(), static_cast<int>(items.size()), -1);

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
            zmq_recv(loop_connection, nullptr, 0, 0);
            std::cout << "Interrupt received, killing server...\n";
            break;
        }

        if ((items[1].revents > 0) &&
            (static_cast<std::make_unsigned_t<decltype(items[1].revents)>>(
                 items[1].revents) &
             static_cast<std::make_unsigned_t<decltype(ZMQ_POLLIN)>>(
                 ZMQ_POLLIN)) > 0) {
            zmq_msg_t message;
            zmq_msg_init(&message);

            // Use non-blocking so we can continue to check loop_connection
            rc = zmq_msg_recv(&message, responder, ZMQ_DONTWAIT);

            if (rc < 0) {
                if (errno == EAGAIN) {
                    continue;
                }
                if (errno == EINTR) {
                    continue;
                }
                std::cerr << "Failure in zmq_msg_recv, killing server...\n";
                break;
            }

            std::string data(static_cast<char *>(zmq_msg_data(&message)),
                             zmq_msg_size(&message));
            zmq_msg_close(&message);

            zmq_send(responder, nullptr, 0, 0);

            std::cout << "Received \"" << data << "\" from client\n";
        }
    }

    zmq_close(loop_connection);
    zmq_close(responder);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Need -c or -s\n";
        return EXIT_FAILURE;
    }

    std::string arg1(*std::next(argv));

    if (arg1 == "-s") {
        void *context = zmq_ctx_new();

        void *loop_connection = zmq_socket(context, ZMQ_PAIR);
        zmq_bind(loop_connection, "inproc://server_loop");

        std::thread server(server_loop, context);

        // wait until server is ready
        zmq_recv(loop_connection, nullptr, 0, 0);

        std::cout << "Press enter to stop... " << std::endl;
        std::getchar();

        zmq_send(loop_connection, nullptr, 0, 0);

        server.join();

        zmq_close(loop_connection);

        zmq_ctx_destroy(context);
    }

    else if (arg1 == "-c") {
        if (argc < 3) {
            std::cerr << "Need a message to send\n";
            return EXIT_FAILURE;
        }

        std::string data(*std::next(std::next(argv)));

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
