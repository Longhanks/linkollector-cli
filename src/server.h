#pragma once

namespace wrappers::zmq {

class context;

} // namespace wrappers::zmq

namespace linkollector::server {

void loop(wrappers::zmq::context &ctx) noexcept;

} // namespace linkollector::server
