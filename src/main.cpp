#include <array>
#include <cstdlib>
#include <iostream>

#include <zmq.h>

constexpr std::size_t BUF_SIZE = 10;
constexpr std::array<const char, 6> HELLO = {'H', 'E', 'L', 'L', 'O', '\0'};
constexpr std::array<const char, 6> WORLD = {'W', 'O', 'R', 'L', 'D', '\0'};
constexpr std::size_t SEND_COUNT = 10;

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Need -c or -s\n";
        return EXIT_FAILURE;
    }

    std::string arg1(*std::next(argv));

    if (arg1 == "-s") {
        void *context = zmq_ctx_new();
        void *responder = zmq_socket(context, ZMQ_REP);
        int rc = zmq_bind(responder, "tcp://*:5555");
        if (rc != 0) {
            std::cerr << "zmq_bind failed \n";
            return EXIT_FAILURE;
        }

        while (true) {
            std::array<char, BUF_SIZE> buffer{};
            zmq_recv(responder, buffer.data(), buffer.size(), 0);
            std::cout << "Received Hello\n";
            zmq_send(responder, WORLD.data(), std::strlen(WORLD.data()), 0);
        }
    }

    else if (arg1 == "-c") {
        std::cout << "Connecting to hello world server...\n";
        void *context = zmq_ctx_new();
        void *requester = zmq_socket(context, ZMQ_REQ);
        zmq_connect(requester, "tcp://localhost:5555");

        std::size_t request_nbr = 0;
        for (request_nbr = 0; request_nbr != SEND_COUNT; ++request_nbr) {
            std::array<char, BUF_SIZE> buffer{};
            std::cout << "Sending Hello " << request_nbr << "...\n";
            zmq_send(requester, HELLO.data(), std::strlen(HELLO.data()), 0);
            zmq_recv(requester, buffer.data(), buffer.size(), 0);
            std::cout << "Received World " << request_nbr << "\n";
        }
        zmq_close(requester);
        zmq_ctx_destroy(context);
    }

    else {
        std::cerr << "Unknown option " << arg1 << "\n";
        return EXIT_FAILURE;
    }
}
