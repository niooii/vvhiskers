//
// Created by niooi on 7/31/2025.
//

#pragma once

#include <algorithm>
#include <containers/ud_map.h>
#include <defs.h>
#include <engine/contexts/net/connection.h>
#include <engine/serial/serde.h>
#include <engine/signal.h>
#include <engine/traits.h>
#include <exception>
#include <memory>
#include "engine/contexts/net/listener.h"
#include "moodycamel/concurrentqueue.h"

namespace v {
    inline std::atomic<u32>& _global_type_counter()
    {
        static std::atomic<u32> c{ 1 };
        return c;
    }

    template <typename T>
    inline uint32_t runtime_type_id()
    {
        static const u32 id = _global_type_counter()++;
        return id;
    }

    /// Default payload of a net channel.
    /// prefer the send_raw method. There is no serialize implementation.
    struct Bytes : public std::span<const std::byte> {
        Bytes(const u8* bytes, u64 len) :
            std::span<const std::byte>(std::span<const std::byte>(
                reinterpret_cast<const std::byte*>(bytes), len)) {};

        /// A function to construct a default Bytes payload. The bytes* ptr will stay
        /// alive until after the payload is consumed by listener(s). Any
        /// implementation of this function should be thread safe or isolated, as it
        /// runs on the networking io thread.
        static Bytes parse(const u8* bytes, u64 len) { return Bytes{ bytes, len }; }
    };

    class NetChannelBase {
        friend NetConnection;

    public:
        virtual ~NetChannelBase()                 = default;
        virtual void send_raw(char* buf, u64 len) = 0;

    protected:
        /// Takes ownership of a packet until consumed.
        virtual void take_packet(ENetPacket* packet) = 0;
        /// Update internal stuff function to be ran on the main thread
        virtual void update() = 0;
    };

    /// An abstract channel class, created and managed by a NetConnection connection
    /// object, meaning a channel is unique to a specific NetConnection only. To create a
    /// channel, inherit from this class.
    /// E.g. class ChatChannel : public NetChannel<ChatChannel> {};
    template <typename Derived, typename Payload = Bytes>
    class NetChannel : NetChannelBase, public QueryBy<Derived*> {
        friend NetConnection;

    public:
        using PayloadT = Payload;

        ~NetChannel() override
        {
            // give packets back to owning connection for destruction

            std::unique_ptr<std::tuple<Payload, ENetPacket*>> elem;

            while (incoming_.try_dequeue(elem))
            {
                conn_->packet_destroy_queue_.enqueue(std::get<ENetPacket*>(*elem));
            }

            conn_.reset();
        }

        /// Get the signal for receiving payloads on this channel
        FORCEINLINE Signal<Payload> received() { return recv_event_.signal(); }

        /// Get the owning NetConnection.
        FORCEINLINE const class std::shared_ptr<NetConnection> connection_info()
        {
            return conn_;
        };

        /// Assigns a unique name to the channel, to be used for communication
        /// over a network.
        /// Assigns the name of the struct and it's namespaces
        /// (e.g. "v::Chat::ChatChannel").
        static constexpr const std::string_view unique_name()
        {
            return v::type_name<Derived>();
        }

        // Templated so that if a user doesn't call send(), then the
        // std::vector<std::byte> serialize function does not have to
        // be implemented.

        /// Send a payload to the other end of the connection, Payload type
        /// must implement std::vector<std::byte> serialize(const P& p) fn.
        template <typename = u8>
        void send(const Payload& payload)
        {
            std::vector<std::byte> bytes = payload.serialize();

            send_raw(reinterpret_cast<char*>(bytes.data()), bytes.size());
        }

        void send_raw(char* buf, u64 len) override
        {
            u32 channel_id = runtime_type_id<Derived>();

            // create packet with u32 prefix + payload data
            const u64   total_len = sizeof(u32) + len;
            ENetPacket* packet =
                enet_packet_create(nullptr, total_len, ENET_PACKET_FLAG_RELIABLE);

            if (!packet)
            {
                LOG_ERROR(
                    "Failed to create packet for channel {}", Derived::unique_name());
                return;
            }

            // write channel ID as first 4 bytes, converting to network byte order
            u32 network_channel_id = htonl(channel_id);
            std::memcpy(packet->data, &network_channel_id, sizeof(u32));

            // write payload data after the channel ID
            std::memcpy(packet->data + sizeof(u32), buf, len);

            // send the packet
            if (conn_->pending_activation_)
            {
                LOG_WARN("Connection is not yet open, queueing packet send");

                // allocate outgoing queue on demand
                if (!conn_->outgoing_packets_)
                {
                    conn_->outgoing_packets_ =
                        new moodycamel::ConcurrentQueue<ENetPacket*>();
                }

                conn_->outgoing_packets_->enqueue(packet);
                return; // don't destroy packet, it will be sent later
            }

            if (enet_peer_send(conn_->peer_, 0, packet) != 0)
            {
                LOG_ERROR("Failed to send packet on channel {}", Derived::unique_name());
                enet_packet_destroy(packet);
            }

            LOG_TRACE("Packet should be queued to send.");
        }

    protected:
        NetChannel() = default;

    private:
        // explicit init beacuse i dont want derived class to have to explicitly
        // define constructors
        void init(std::shared_ptr<NetConnection> c) { conn_ = c; }
        std::shared_ptr<NetConnection> connection() const { return conn_; }

        /// Calls the listeners and hands packets back to the NetConnection.
        void update() override
        {
            std::unique_ptr<std::tuple<Payload, ENetPacket*>> elem;

            while (incoming_.try_dequeue(elem))
            {
                // Fire the event with the received payload
                recv_event_.fire(std::move(std::get<Payload>(*elem)));

                // hand packet back to owning connection for destruction
                conn_->packet_destroy_queue_.enqueue(std::get<ENetPacket*>(*elem));
            }
        }

        void take_packet(ENetPacket* packet) override
        {
            try
            {
                // exclude the channel id header
                Payload p = Payload::parse(
                    packet->data + sizeof(u32), packet->dataLength - sizeof(u32));

                std::unique_ptr<std::tuple<Payload, ENetPacket*>> elem =
                    std::make_unique<std::tuple<Payload, ENetPacket*>>(
                        std::move(p), packet);

                incoming_.enqueue(std::move(elem));
            }
            catch (std::exception const& ex)
            {
                LOG_ERROR(
                    "[{}] Failed to parse, caught exception {}", unique_name(),
                    ex.what());
                // destroy..
                conn_->packet_destroy_queue_.enqueue(packet);
            }
            catch (...)
            {
                LOG_ERROR(
                    "[{}] Failed to parse, caught unknown exception.", unique_name());
                conn_->packet_destroy_queue_.enqueue(packet);
            }
        }

        moodycamel::ConcurrentQueue<std::unique_ptr<std::tuple<Payload, ENetPacket*>>>
            incoming_{};

        // pointer to the owning netconnection, which is guarenteed to be valid
        // shared ptr so the owning connection can only destroy itself when all
        // channels are gone (will be destroyed when requesting a connection close)
        std::shared_ptr<NetConnection> conn_;

        // Event for receiving payloads
        Event<Payload> recv_event_;
    };

    /// A channel to make sure the program builds correctly
    class TestChannel : NetChannel<TestChannel> {};
} // namespace v
