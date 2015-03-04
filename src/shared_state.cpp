#include "cocaine/framework/detail/shared_state.hpp"

using namespace cocaine::framework;

void shared_state_t::put(value_type&& message) {
    std::lock_guard<std::mutex> lock(mutex);

    COCAINE_ASSERT(!broken);

    if (pending.empty()) {
        queue.push(std::move(message));
    } else {
        pending.front().set_value(std::move(message));
        pending.pop();
    }
}

void shared_state_t::put(const std::error_code& ec) {
    std::lock_guard<std::mutex> lock(mutex);

    COCAINE_ASSERT(!broken);

    broken = ec;
    while (!pending.empty()) {
        pending.front().set_exception(std::system_error(ec));
        pending.pop();
    }
}

auto shared_state_t::get() -> typename task<value_type>::future_type {
    std::lock_guard<std::mutex> lock(mutex);

    if (broken) {
        COCAINE_ASSERT(queue.empty());

        return make_ready_future<value_type>::error(std::system_error(broken.get()));
    }

    if (queue.empty()) {
        typename task<value_type>::promise_type promise;
        auto future = promise.get_future();
        pending.push(std::move(promise));
        return future;
    }

    auto future = make_ready_future<value_type>::value(std::move(queue.front()));
    queue.pop();
    return future;
}