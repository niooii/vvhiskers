//
// Created by niooi on 7/31/2025.
//

#pragma once

#include <containers/ud_set.h>
#include <defs.h>
#include <enet.h>
#include <engine/signal.h>
#include <entt/entt.hpp>

namespace v {
    class NetConnection;

    /// A 'server' class.
    class NetListener {
        friend class NetworkContext;

    public:
        /// Signal fired when a new incoming connection is created
        FORCEINLINE Signal<std::shared_ptr<NetConnection>> connected() const
        {
            return connect_event_.signal();
        }

        /// Signal fired when an incoming connection has been disconnected
        FORCEINLINE Signal<std::shared_ptr<NetConnection>> disconnected() const
        {
            return disconnect_event_.signal();
        }

    private:
        NetListener(
            class NetworkContext* ctx, const std::string& host, u16 port,
            u32 max_connections = 128);

        // called by net context when new connection is inbound
        void handle_new_connection(std::shared_ptr<NetConnection> con);

        // called by net context when connection is disconnected
        void handle_disconnection(std::shared_ptr<NetConnection> con);

        /// Function will update server components by calling them on all previous
        /// connections if requested
        void update();

        std::string addr_;
        u16         port_;

        NetworkContext* net_ctx_;
        ENetHost*       host_;

        ud_set<ENetPeer*> connected_;

        // Internal events
        Event<std::shared_ptr<NetConnection>> connect_event_;
        Event<std::shared_ptr<NetConnection>> disconnect_event_;
    };
} // namespace v
