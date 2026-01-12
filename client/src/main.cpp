//
// Created by niooi on 7/24/25.
//

#include <client.h>
#include <prelude.h>
#include <time/stopwatch.h>
#include <time/time.h>
#include <tracy/Tracy.hpp>

constexpr i32 TEMP_MAX_FPS = 40;

using namespace v;

int main(int argc, char** argv)
{
    init(argv[0]);

    auto          stopwatch = Stopwatch();
    constexpr f64 TEMP_SPF  = 1.0 / TEMP_MAX_FPS;

    Engine engine{};
    auto&  client = engine.add_domain<Client>();

    while (client.is_running())
    {
        client.update();

        if (const auto sleep_time = stopwatch.until(TEMP_SPF); sleep_time > 0)
            time::sleep_ms(sleep_time * 1000);

        stopwatch.reset();
    }

    return 0;
}
