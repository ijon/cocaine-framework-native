#include "cocaine/framework/receiver.hpp"

#include "cocaine/framework/session.hpp"

#include "cocaine/framework/detail/basic_session.hpp"
#include "cocaine/framework/detail/shared_state.hpp"
#include "cocaine/framework/detail/log.hpp"

using namespace cocaine::framework;

template<class Session>
basic_receiver_t<Session>::basic_receiver_t(std::uint64_t id, std::shared_ptr<Session> session, std::shared_ptr<shared_state_t> state) :
    id(id),
    session(session),
    state(state)
{}

template<class Session>
basic_receiver_t<Session>::~basic_receiver_t() {
    // TODO: Really need?
    CF_DBG("received the last message in the protocol graph - revoking ...");
    session->revoke(id);
}

template<class Session>
typename task<decoded_message>::future_type
basic_receiver_t<Session>::recv() {
    return state->get();
}

template<class Session>
void basic_receiver_t<Session>::revoke() {
    // CF_DBG("received the last message in the protocol graph - revoking ...");
    // session->revoke(id);
}
