#include <cstdlib>
#include <iostream>

#include <zmq.h>

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

        zmq_msg_t message;
        zmq_msg_init(&message);

        std::cout << "Waiting for client...\n";
        zmq_msg_recv(&message, responder, 0);

        std::string data(static_cast<char *>(zmq_msg_data(&message)),
                         zmq_msg_size(&message));
        zmq_msg_close(&message);

        std::cout << "Received \"" << data << "\" from client\n";

        zmq_close(responder);
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
        zmq_connect(requester, "tcp://localhost:5555");

        zmq_msg_t message;
        zmq_msg_init_data(&message,
                          static_cast<void *>(data.data()),
                          data.size(),
                          nullptr,
                          nullptr);
        zmq_msg_send(&message, requester, 0);

        zmq_close(requester);
        zmq_ctx_destroy(context);
    }

    else {
        std::cerr << "Unknown option " << arg1 << "\n";
        return EXIT_FAILURE;
    }
}
