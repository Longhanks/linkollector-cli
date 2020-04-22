#pragma once

namespace wrappers::zmq {

class context final {

public:
    explicit context() noexcept;
    context(const context &other) = delete;
    context &operator=(const context &other) = delete;
    context(context &&other) noexcept;
    context &operator=(context &&other) noexcept;
    ~context() noexcept;

    friend class socket;

private:
    void *m_context = nullptr;
};

} // namespace wrappers::zmq
