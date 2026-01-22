#include <engine/contexts/net/ctx.h>
#include <net/channels.h>
#include <prelude.h>
#include <stdexcept>
#include "engine/contexts/net/listener.h"
#include "engine/domain.h"

namespace v {
    struct ServerConfig {
        std::string host;
        u16         port;
    };

    /// A singleton server domain
    class ServerDomain : public SDomain<ServerDomain> {
    public:
        ServerDomain(ServerConfig& conf, const std::string& name = "Server Domain") :
            SDomain(name), conf_(conf)
        {}

        void init() override
        {
            SDomain::init();

            auto net_ctx = engine().get_ctx<NetworkContext>();
            if (!net_ctx)
            {
                LOG_WARN("Created default network context");
                net_ctx = engine().add_ctx<NetworkContext>(1 / 500.0);
            }

            listener_ = net_ctx->listen_on(conf_.host, conf_.port);

            listener_->connected().connect(
                this,
                [this](std::shared_ptr<NetConnection> con)
                {
                    LOG_INFO("Client connected successfully!");

                    auto& connection_channel =
                        con->create_channel<ConnectServerChannel>();

                    connection_channel.received().connect(
                        [](const ConnectServerChannel::PayloadT& req)
                        { LOG_INFO("New player {}", req.uuid); });

                    // test some quick channel stuff via chat channel
                    auto& cc = con->create_channel<ChatChannel>();

                    cc.received().connect(
                        this,
                        [this](const ChatMessage& msg)
                        {
                            LOG_INFO("Got message {} from client", msg.msg);
                            // bounce to all chat channels
                            for (auto [e, channel] : view<ChatChannel>().each())
                            {
                                ChatMessage payload = { .msg = msg.msg };
                                channel->send(payload);
                                LOG_TRACE("Echoed message to channel!");
                            }
                        });
                });

            // register a bunch of on ticks and stuff

            LOG_INFO("Listening on {}:{}", conf_.host, conf_.port);
        }

    private:
        ServerConfig                 conf_;
        std::shared_ptr<NetListener> listener_;
    };
} // namespace v
