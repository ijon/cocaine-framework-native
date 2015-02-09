#include "cocaine/framework/worker.hpp"

#include <csignal>

#include <boost/program_options.hpp>
#include <boost/thread/thread.hpp>

#include <asio/local/stream_protocol.hpp>
#include <asio/connect.hpp>

#include <cocaine/detail/service/node/messages.hpp>

#include "cocaine/framework/forwards.hpp"

namespace ph = std::placeholders;

using namespace cocaine;
using namespace cocaine::framework;

//! \note single shot.
template<class Connection>
class worker_session_t::push_t : public std::enable_shared_from_this<push_t<Connection>> {
    io::encoder_t::message_type message;
    std::shared_ptr<Connection> connection;
    promise_t<void> h;

public:
    explicit push_t(io::encoder_t::message_type&& message, std::shared_ptr<Connection> connection, promise_t<void>&& h) :
        message(std::move(message)),
        connection(connection),
        h(std::move(h))
    {}

    void operator()() {
        if (connection->channel) {
            connection->channel->writer->write(message, std::bind(&push_t::on_write, this->shared_from_this(), ph::_1));
        } else {
            h.set_exception(std::system_error(asio::error::not_connected));
        }
    }

private:
    void on_write(const std::error_code& ec) {
//        CF_DBG("write event: %s", CF_EC(ec));

        if (ec) {
            connection->on_error(ec);
            h.set_exception(std::system_error(ec));
        } else {
            h.set_value();
        }
    }
};

options_t::options_t(int argc, char** argv) {
    // TODO: Make help.
    boost::program_options::options_description options("Configuration");
    options.add_options()
        ("app",      boost::program_options::value<std::string>())
        ("uuid",     boost::program_options::value<std::string>())
        ("endpoint", boost::program_options::value<std::string>())
        ("locator",  boost::program_options::value<std::string>());


    boost::program_options::command_line_parser parser(argc, argv);
    parser.options(options);
    parser.allow_unregistered();

    boost::program_options::variables_map vm;
    boost::program_options::store(parser.run(), vm);
    boost::program_options::notify(vm);

    if (vm.count("app") == 0 || vm.count("uuid") == 0 || vm.count("endpoint") == 0 || vm.count("locator") == 0) {
        throw std::invalid_argument("invalid command line options");
    }

    name = vm["app"].as<std::string>();
    uuid = vm["uuid"].as<std::string>();
    endpoint = vm["endpoint"].as<std::string>();
    locator = vm["locator"].as<std::string>();
}

worker_t::worker_t(options_t options) :
    options(std::move(options)),
    heartbeat_timer(loop),
    disown_timer(loop)
{
    // TODO: Set default locator endpoint.
    // service_manager_t::endpoint_t locator_endpoint("127.0.0.1", 10053);

    // Block the deprecated signals.
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGPIPE);
    ::sigprocmask(SIG_BLOCK, &sigset, nullptr);
}

int worker_t::run() {
    session.reset(new worker_session_t(loop, this));
    session->connect(options.endpoint, options.uuid);
    send_heartbeat(std::error_code());

    // The main thread is guaranteed to work only with cocaine socket and timers.
    loop.run();

    return 0;
}

void worker_t::stop() {
    loop.stop();
}

// TODO: Rename to: health_manager::reset
void worker_t::send_heartbeat(const std::error_code& ec) {
    if (ec) {
        // TODO: Handle the error properly.
        COCAINE_ASSERT(false);
        return;
    }

    CF_DBG("<- ♥");
    session->push(io::encoded<io::rpc::heartbeat>(1));
    on_heartbeat_sent(std::error_code());
}

void worker_t::on_heartbeat_sent(const std::error_code& ec) {
    if (ec) {
        // TODO: Stop the worker.
        return;
    }

    heartbeat_timer.expires_from_now(boost::posix_time::seconds(10));
    heartbeat_timer.async_wait(std::bind(&worker_t::send_heartbeat, this, ph::_1));
}

void worker_t::on_disown(const std::error_code& ec) {
    if (ec) {
        if (ec == asio::error::operation_aborted) {
            // Okay. Do nothing.
            return;
        }

        // TODO: Handle other error types.
    }

    throw std::runtime_error("disowned");
}

void worker_t::on_heartbeat() {
    CF_DBG("-> ♥");

    disown_timer.expires_from_now(boost::posix_time::seconds(60));
    disown_timer.async_wait(std::bind(&worker_t::on_disown, this, ph::_1));
}

auto worker_session_t::push(std::uint64_t span, io::encoder_t::message_type&& message) -> future_t<void> {
    auto channels = this->channels.synchronize();
    auto it = channels->find(span);
    if (it == channels->end()) {
        // TODO: Throw a typed exception.
        throw std::runtime_error("trying to send message through non-registered channel");
    }

    return push(std::move(message));
}

auto worker_session_t::push(io::encoder_t::message_type&& message) -> future_t<void> {
    promise_t<void> p;
    auto f = p.get_future();

    loop.post(std::bind(&push_t<worker_session_t>::operator(),
                        std::make_shared<push_t<worker_session_t>>(std::move(message), shared_from_this(), std::move(p))));
    return f;
}

void worker_session_t::on_read(const std::error_code& ec) {
    CF_DBG("read event: %s", CF_EC(ec));
    // TODO: Stop the worker on any network error.
    // TODO: If message.type() == 3, 4, 5, 6 => push. Otherwise unpack into the control message.
    switch (message.type()) {
    case (io::event_traits<io::rpc::handshake>::id):
        // TODO: Should be never sent.
        COCAINE_ASSERT(false);
        break;
    case (io::event_traits<io::rpc::heartbeat>::id):
        worker->on_heartbeat();
        break;
    case (io::event_traits<io::rpc::terminate>::id):
        worker->stop();
        break;
    case (io::event_traits<io::rpc::invoke>::id):
    case (io::event_traits<io::rpc::chunk>::id):
    case (io::event_traits<io::rpc::error>::id):
    case (io::event_traits<io::rpc::choke>::id):
        CF_DBG("event %d, span %d", message.type(), message.span());
        dispatch(message);
        break;
    default:
        break;
    }

    channel->reader->read(message, std::bind(&worker_session_t::on_read, this, ph::_1));
}

void worker_session_t::on_write(const std::error_code& ec) {
    // TODO: Stop the worker on any network error.
}

void worker_session_t::on_error(const std::error_code& ec) {

}

void worker_session_t::connect(std::string endpoint, std::string uuid) {
    std::unique_ptr<socket_type> socket(new socket_type(loop));
    protocol_type::endpoint endpoint_(endpoint);
    socket->connect(endpoint_);

    channel.reset(new channel_type(std::move(socket)));

    channel->reader->read(message, std::bind(&worker_session_t::on_read, shared_from_this(), ph::_1));
    push(io::encoded<io::rpc::handshake>(1, uuid));
}

void worker_session_t::revoke(std::uint64_t span) {
}

void worker_session_t::dispatch(const io::decoder_t::message_type& message) {
    // TODO: Make pretty.
    // visit(message);
    switch (message.type()) {
    case (io::event_traits<io::rpc::invoke>::id): {
        // Find handler.
        // If not found - log and drop.
        // Create tx and rx.
        // Save shared state for rx. Pass this for tx.
        // Invoke handler.
        std::string event;
        io::type_traits<
            typename io::event_traits<io::rpc::invoke>::argument_type
        >::unpack(message.args(), event);
        auto it = worker->handlers.find(event);
        if (it == worker->handlers.end()) {
            // TODO: Log
            return;
        }
        auto id = message.span();
        auto tx = std::make_shared<basic_sender_t<worker_session_t>>(id, shared_from_this());
//        auto ss = std::make_shared<detail::shared_state_t>();
//        auto rx = std::make_shared<basic_receiver_t>(id, shared_from_this(), ss);

//        auto channels = this->channels.synchronize();
//        channels->

        it->second(tx);
        break;
    }
    case (io::event_traits<io::rpc::chunk>::id):
    case (io::event_traits<io::rpc::error>::id):
    case (io::event_traits<io::rpc::choke>::id):
        // Find rx shared state by span().
        // If not found - log and drop.
        // Push to the shared state.
        break;
    default:
        COCAINE_ASSERT(false);
    }
}
