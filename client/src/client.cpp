//
// Created by niooi on 9/26/2025.
//

#include <client.h>
#include <engine/camera.h>
#include <engine/contexts/net/connection.h>
#include <engine/contexts/net/ctx.h>
#include <engine/contexts/render/ctx.h>
#include <engine/contexts/window/sdl.h>
#include <engine/contexts/window/window.h>
#include <net/channels.h>
#include <render/mandelbulb_renderer.h>
#include <render/triangle_domain.h>
#include <world/world.h>
#include "engine/contexts/async/async.h"
#include "engine/contexts/async/coro_interface.h"
#include "engine/contexts/render/default_render_domain.h"

namespace v {
    Client::Client() : running_(true) {}

    void Client::init()
    {
        // all the contexts the client needs to function
        sdl_ctx_    = engine().add_ctx<SDLContext>();
        window_ctx_ = engine().add_ctx<WindowContext>();
        window_     = window_ctx_->create_window("hjey man!", { 600, 600 }, { 600, 600 });

        window_->key_pressed().connect(
            this,
            [this](Key k)
            {
                switch (k)
                {
                case Key::R:
                    window_->capture_mouse(!window_->capturing_mouse());
                default:
                }
            });

        render_ctx_ = engine().add_ctx<RenderContext>("./resources/shaders");
        net_ctx_    = engine().add_ctx<NetworkContext>(1.0 / 500);

        constexpr u16 threads   = 16;
        auto*         async_ctx = engine().add_ctx<AsyncContext>(threads);

        // for some reason this crashes when theres no content
        // async_ctx->spawn(
        //     [](CoroutineInterface& ci) -> Coroutine<void>
        //     {
        //         // while (co_await ci.sleep(500))
        //             // LOG_DEBUG("500ms hi");
        //     });

        // test rendering via domains
        // TODO! this should be order independent, but how? like if i add
        // triangle domain before a clearing domain, the clear should still come first?
        // manual graph ordering maybe?
        engine().add_domain<TriangleRenderer>();
        // engine().add_domain<DefaultRenderDomain>();
        auto& mandelbulb = engine().add_domain<MandelbulbRenderer>();

        // Setup network connection
        LOG_INFO("Connecting to server...");
        connection_ = net_ctx_->create_connection("127.0.0.1", 25566);
        LOG_INFO("Connection created, attempting to connect...");

        // test channel kinda
        auto& channel = connection_->create_channel<ChatChannel>();

        // Connect to receive messages
        channel.received().connect(
            this, [](const ChatMessage& msg)
            { LOG_INFO("Received chat message: {}", msg.msg); });

        ChatMessage msg;
        msg.msg = "hi server man";
        channel.send(msg);

        // windows update task does not depend on anything
        engine().on_tick.connect(
            {}, {}, "windows",
            [this]()
            {
                window_ctx_->update();
                sdl_ctx_->update();
            });

        // render depends on the window input update task to be finished
        engine().on_tick.connect(
            { "windows" }, {}, "render", [this]() { render_ctx_->update(); });

        // network update task does not depend on anything
        engine().on_tick.connect({}, {}, "network", [this]() { net_ctx_->update(); });

        // async coroutine scheduler update
        engine().on_tick.connect({}, {}, "async", [async_ctx]() { async_ctx->update(); });

        // handle the sdl quit event (includes keyboard interrupt)
        sdl_ctx_->quit().connect(this, [this]() { running_ = false; });

        // TODO! temporarily connect to the server with dummy info
        auto name = std::format("Player-{}", rand::irange(0, 1'000'000));
        LOG_INFO("Generated new random name {}", name);
        auto& connect_chnl = connection_->create_channel<ConnectServerChannel>();
        connect_chnl.send({ .uuid = name });
    }

    void Client::update() { engine().tick(); }
} // namespace v
