//
// Created by niooi on 7/24/25.
//

#include <engine/contexts/net/connection.h>
#include <engine/contexts/net/ctx.h>
#include <iostream>
#include <net/channels.h>
#include <prelude.h>
#include <server.h>
#include <time/stopwatch.h>
#include <time/time.h>
#include <world/world.h>
#include "engine/contexts/async/async.h"
#include "engine/contexts/async/coro_interface.h"
#include "engine/contexts/net/listener.h"

using namespace v;

constexpr f64 SERVER_UPDATE_RATE = 1.0 / 144.0; // 60 FPS

int main(int argc, char** argv)
{
    init(argv[0]);

    LOG_INFO("Starting v server on 127.0.0.1:25566");

    Engine engine{};

    // TODO! world iis useless rn its just here for fun
    auto& world = engine.add_domain<WorldDomain>();

    // attempts to update every 1ms
    auto net_ctx = engine.add_ctx<NetworkContext>(1.0 / 1000.0);

    auto async = engine.add_ctx<AsyncContext>(8);

    async->spawn(
        [](CoroutineInterface& ci) -> Coroutine<void>
        {
            int count = 0;
            while (co_await ci.sleep(100))
            {
                // LOG_INFO("Hi {}", ++count);
            }
        });

    ServerConfig config{ "127.0.0.1", 25566 };
    engine.add_domain<ServerDomain>(config);

    Stopwatch        stopwatch{};
    std::atomic_bool running{ true };

    LOG_INFO("Server ready, waiting for connections...");

    while (running)
    {
        stopwatch.reset();

        net_ctx->update();

        async->update();

        engine.tick();

        if (const auto sleep_time = stopwatch.until(SERVER_UPDATE_RATE); sleep_time > 0)
            time::sleep_ms(sleep_time * 1000);
    }

    LOG_INFO("Server shutting down");
    return 0;
}
