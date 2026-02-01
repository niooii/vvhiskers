//
// Created by niooi on 8/1/2025.
//

#include <cstring>
#include <defs.h>
#include <enet.h>
#include <engine/contexts/net/channel.h>
#include <engine/contexts/net/connection.h>
#include <engine/contexts/net/ctx.h>
#include <memory>
#include <stdexcept>
#include "moodycamel/concurrentqueue.h"

namespace v {
    // create an outgoing connection
    NetConnection::NetConnection(
        NetworkContext* ctx, const std::string& host, u16 port, f64 connection_timeout) :
        net_ctx_(ctx), conn_type_(ConnectionType::Outgoing),
        connection_timeout_(connection_timeout)
    {
        ENetAddress address = {};
        if (enet_address_set_host(&address, host.c_str()) != 0)
        {
            enet_deinitialize();
            throw std::runtime_error("Failed to set host address");
        }
        address.port = port;

        peer_ = enet_host_connect(
            *ctx->outgoing_host_.write(), &address, 4 /* 4 channels */, 0);

        if (!peer_)
        {
            LOG_ERROR("Failed to connect to peer at {}:{}", host, port);
            throw std::runtime_error("bleh");
        }

        peer_->data = (void*)this;

        LOG_TRACE("Outgoing connection initialized");
    }

    // just an incoming connection, its whatever
    NetConnection::NetConnection(NetworkContext* ctx, ENetPeer* peer) :
        net_ctx_(ctx), peer_(peer), conn_type_(ConnectionType::Incoming)
    {
        peer_->data = (void*)this;

        LOG_TRACE("Incoming connection initialized");
    }

    // at this point there should be no more references internally
    NetConnection::~NetConnection()
    {
        // internally queues disconnect
        // if remote_disconnected_, then the peer may no longer be a valid pointer
        if (!remote_disconnected_)
        {
            // enqueue onto the IO thread no race condition
            net_ctx_->enqueue_io([this] { enet_peer_disconnect(peer_, 0); });
        }

        // clear the peer data pointer - safe to do here since destructor runs on main
        // thread after all lifecycle events have been processed
        if (peer_->data == (void*)this)
            peer_->data = NULL;

        ENetPacket* packet;
        while (pending_packets_.try_dequeue(packet))
            enet_packet_destroy(packet);

        // cleanup outgoing packets if they weren't sent
        if (outgoing_packets_)
        {
            while (outgoing_packets_->try_dequeue(packet))
                enet_packet_destroy(packet);
            delete outgoing_packets_;
        }

        LOG_TRACE("Connection destroyed");
    }


    void NetConnection::request_close()
    {
        NetworkEvent event{ NetworkEventType::DestroyConnection,
                            net_ctx_->get_connection(peer_), nullptr };

        // delete the internal shared ptr no leak yes
        net_ctx_->event_queue_.enqueue(std::move(event));
    }

    void NetConnection::activate_connection()
    {
        pending_activation_ = false;

        // Process any packets that arrived while pending
        ENetPacket* packet;
        while (pending_packets_.try_dequeue(packet))
        {
            handle_raw_packet(packet);
        }

        // send any packets that were queued to go out
        if (outgoing_packets_)
        {
            ENetPacket* packet;
            while (outgoing_packets_->try_dequeue(packet))
            {
                enet_peer_send(peer_, 0, packet);
            }

            delete outgoing_packets_;
            outgoing_packets_ = nullptr;
        }

        LOG_TRACE("Connection activated");
    }

    NetConnectionResult NetConnection::update()
    {
        if (pending_activation_)
        {
            if (since_open_.elapsed() > connection_timeout_)
            {
                LOG_ERROR("Connection timed out in {} seconds.", connection_timeout_);
                remote_disconnected_ = true;
                // just nuke the connection TODO! i think this is safe??
                net_ctx_->enqueue_io([this] { enet_peer_disconnect_now(peer_, 0); });

                return NetConnectionResult::TimedOut;
            }

            return NetConnectionResult::ConnWaiting;
        }

        {
            // update all channels (runs the callbacks for recieved/parsed data)
            for (auto& [name, chnl] : c_insts_)
            {
                chnl->update();
            }
        }

        // destroys the consumed packets
        ENetPacket* packet;
        while (packet_destroy_queue_.try_dequeue(packet))
        {
            enet_packet_destroy(packet);
        }

        return NetConnectionResult::Success;
    }


    void NetConnection::handle_raw_packet(ENetPacket* packet)
    {
        // If connection is pending activation, queue the packet
        if (pending_activation_)
        {
            pending_packets_.enqueue(packet);
            return;
        }

        // TODO! auto channel creation, routing, etc
        LOG_TRACE("Got packet");
        // check if its a channel creation request
        constexpr std::string_view prefix = "CHANNEL|";
        if (UNLIKELY(
                packet->dataLength > prefix.size() &&
                memcmp(packet->data, prefix.data(), prefix.size()) == 0))
        {
            LOG_TRACE(
                "Packet is channel creation request: {}",
                std::string(
                    reinterpret_cast<const char*>(packet->data), packet->dataLength));

            const char* data =
                reinterpret_cast<const char*>(packet->data) + prefix.size();


            size_t remaining_len = packet->dataLength - prefix.size();

            std::string message(data, remaining_len);

            size_t separator = message.find('|');
            if (separator != std::string::npos)
            {
                std::string channel_name = message.substr(0, separator);
                std::string channel_id   = message.substr(separator + 1);

                u32 c_id;
                auto [ptr, ec] = std::from_chars(
                    channel_id.data(), channel_id.data() + channel_id.size(), c_id);

                if (ec != std::errc())
                {
                    LOG_ERROR("Invalid channel ID: {}", channel_id);
                    return;
                }

                // populate info maps
                {
                    auto lock = map_lock_.write();

                    recv_c_ids_[channel_name] = c_id;
                    auto [it, inserted]       = recv_c_info_.try_emplace(c_id);
                    auto& info                = it->second;
                    info.name                 = channel_name;
                }

                NetworkEvent event{
                    .type                       = NetworkEventType::ChannelLink,
                    .connection                 = shared_con_,
                    .created_channel.name       = channel_name,
                    .created_channel.remote_uid = c_id,
                };
                net_ctx_->event_queue_.enqueue(std::move(event));

                LOG_TRACE("Channel {} linked to remote uid {}", channel_name, channel_id);
            }
            else
            {
                LOG_WARN(
                    "Bad channel creation packet",
                    std::string(
                        reinterpret_cast<const char*>(packet->data), packet->dataLength));
            }

            // Channel creation packets are not forwarded to channels, so destroy them
            // here
            enet_packet_destroy(packet);
            return;
        }

        // else its a regular packet traveling to a channel
        if (packet->dataLength < sizeof(u32))
        {
            LOG_WARN("Packet too small to contain channel ID, dropping");
            enet_packet_destroy(packet);
            return;
        }

        // extract channel ID from first 4 bytes, converting from network byte order
        u32 channel_id;
        std::memcpy(&channel_id, packet->data, sizeof(u32));
        channel_id = ntohl(channel_id);

        {
            // we mutate info here, so take write lock
            auto lock = map_lock_.write();
            auto it   = recv_c_info_.find(channel_id);
            if (it == recv_c_info_.end())
            {
                LOG_WARN("Invalid packet, no such channel id exists: {}", channel_id);
                enet_packet_destroy(packet);
                return;
            }

            auto& info = it->second;
            if (!info.channel)
            {
                if (!info.before_creation_packets)
                {
                    info.before_creation_packets =
                        std::make_unique<moodycamel::ConcurrentQueue<ENetPacket*>>();
                }
                info.before_creation_packets->enqueue(packet);
            }
            else
            {
                LOG_DEBUG("Channel exists already");
                info.channel->take_packet(packet);
            }
        }
    }

    void NetConnection::NetChannelInfo::drain_queue(NetChannelBase* channel)
    {
        if (!before_creation_packets)
            return;
        ENetPacket* packet;
        while (before_creation_packets->try_dequeue(packet))
        {
            LOG_TRACE("Processed a message sent before local channel creation.");
            channel->take_packet(packet);
        }

        // queue is no longer needed after draining
        before_creation_packets.reset();
    }
} // namespace v
