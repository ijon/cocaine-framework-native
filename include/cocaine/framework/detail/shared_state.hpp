#pragma once

#include <queue>

#include "cocaine/framework/forwards.hpp"

#include "cocaine/framework/detail/decoder.hpp"

namespace cocaine {

namespace framework {

/// \internal
class shared_state_t {
public:
    typedef detail::decoder_t::message_type value_type;

    std::queue<value_type> queue;
    std::queue<typename task<value_type>::promise_type> pending;

    boost::optional<std::error_code> broken;

    std::mutex mutex;

public:
    void put(value_type&& message);
    void put(const std::error_code& ec);

    auto get() -> typename task<value_type>::future_type;
};

}

}