#include "context.h"

#include <zmq.h>

namespace wrappers::zmq {

context::context() noexcept : m_context(zmq_ctx_new()) {}

context::context(context &&other) noexcept : m_context(other.m_context) {
    other.m_context = nullptr;
}

context &context::operator=(context &&other) noexcept {
    if (this != &other) {
        if (this->m_context != nullptr) {
            zmq_ctx_term(this->m_context);
        }

        this->m_context = other.m_context;
        other.m_context = nullptr;
    }

    return *this;
}

context::~context() noexcept {
    if (this->m_context != nullptr) {
        zmq_ctx_term(this->m_context);
        this->m_context = nullptr;
    }
}

} // namespace wrappers::zmq
