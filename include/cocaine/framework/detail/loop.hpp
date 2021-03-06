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

#include "cocaine/framework/detail/forwards.hpp"

namespace cocaine {

namespace framework {

/// \internal
struct event_loop_t {
    typedef detail::loop_t loop_type;

    loop_type& loop;
    loop_type& userloop;

    explicit event_loop_t(loop_type& loop) noexcept :
        loop(loop),
        userloop(loop)
    {}

    event_loop_t(loop_type& ioloop, loop_type& userloop) noexcept :
        loop(ioloop),
        userloop(userloop)
    {}
};

}

}
