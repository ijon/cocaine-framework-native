/*
    Copyright (c) 2015 Evgeny Safronov <division494@gmail.com>
    Copyright (c) 2011-2015 Other contributors as noted in the AUTHORS file.
    This file is part of Cocaine.
    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.
    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <cstdint>

#include <boost/asio/ip/tcp.hpp>

#include "cocaine/framework/config.hpp"
#include "cocaine/framework/channel.hpp"
#include "cocaine/framework/encoder.hpp"
#include "cocaine/framework/forwards.hpp"
#include "cocaine/framework/receiver.hpp"
#include "cocaine/framework/scheduler.hpp"
#include "cocaine/framework/sender.hpp"
#include "cocaine/framework/trace.hpp"

#include <cocaine/idl/logging.hpp>

namespace cocaine {

namespace framework {

/*!
 * RAII class that manages with connection queue and returns a typed sender/receiver.
 */
template<class BasicSession>
class session {
public:
    typedef BasicSession basic_session_type;
    typedef boost::asio::ip::tcp::endpoint endpoint_type;
#if BOOST_VERSION > 104800
    typedef boost::asio::ip::tcp::socket::native_handle_type native_handle_type;
#else
    typedef boost::asio::ip::tcp::socket::native_type native_handle_type;
#endif

    typedef std::tuple<
        std::shared_ptr<basic_sender_t<basic_session_t>>,
        std::shared_ptr<basic_receiver_t<basic_session_t>>
    > basic_invoke_result;

private:
    class impl;
    std::shared_ptr<impl> d;
    scheduler_t& scheduler;

public:
    explicit session(scheduler_t& scheduler);
    ~session();

    bool connected() const;

    auto connect(const endpoint_type& endpoint) -> task<void>::future_type;
    auto connect(const std::vector<endpoint_type>& endpoints) -> task<void>::future_type;

    auto endpoint() const -> boost::optional<endpoint_type>;

    native_handle_type
    native_handle() const;

    template<class Event, class... Args>
    typename task<channel<Event>>::future_type
    invoke(Args&&... args) {
        auto encode_cb = std::bind(
                    &encode<Event, Args...>,
                    std::placeholders::_1,
                    std::forward<Args>(args)...
        );
        return invoke(std::move(encode_cb)).then(scheduler, trace_t::bind(&session::on_invoke<Event>, std::placeholders::_1));
    }

private:
    task<basic_invoke_result>::future_type
    invoke(encode_callback_t encode_callback);

    template<class Event>
    static
    channel<Event>
    on_invoke(task<basic_invoke_result>::future_move_type future) {
        return channel<Event>(future.get());
    }
};

typedef session<basic_session_t> session_t;

} // namespace framework

} // namespace cocaine
