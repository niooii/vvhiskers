// Signal/Event system tests

#include <atomic>
#include <engine/contexts/async/async.h>
#include <engine/domain.h>
#include <engine/engine.h>
#include <engine/signal.h>
#include <test.h>
#include <time/stopwatch.h>

using namespace v;

// Test domain for domain-bound connection tests
class TestDomain : public Domain<TestDomain> {
public:
    TestDomain(std::string name = "TestDomain") : Domain(std::move(name)) {}
    int counter = 0;
};

int main()
{
    auto [engine, tctx] = testing::init_test("signal");

    // Test basic signal connection and firing
    {
        Event<int> event;
        int        received_value = 0;

        auto conn = event.signal().connect([&](int val) { received_value = val; });

        event.fire(42);
        tctx.assert_now(received_value == 42, "Signal fired with correct value");

        event.fire(100);
        tctx.assert_now(received_value == 100, "Signal fired again with new value");
    }

    // Test void signal
    {
        Event<void> event;
        int         fire_count = 0;

        auto conn = event.signal().connect([&]() { fire_count++; });

        event.fire();
        tctx.assert_now(fire_count == 1, "Void signal fired once");

        event.fire();
        tctx.assert_now(fire_count == 2, "Void signal fired twice");
    }

    // Test multiple connections
    {
        Event<int> event;
        int        received1 = 0, received2 = 0, received3 = 0;

        auto conn1 = event.signal().connect([&](int val) { received1 = val; });
        auto conn2 = event.signal().connect([&](int val) { received2 = val; });
        auto conn3 = event.signal().connect([&](int val) { received3 = val; });

        event.fire(123);

        tctx.assert_now(received1 == 123, "First connection received correct value");
        tctx.assert_now(received2 == 123, "Second connection received correct value");
        tctx.assert_now(received3 == 123, "Third connection received correct value");
    }

    // Test SignalConnection copy semantics (Rc sharing)
    {
        Event<int> event;
        int        fire_count = 0;

        SignalConnection conn1 = event.signal().connect([&](int) { fire_count++; });
        SignalConnection conn2 = conn1; // Copy - both point to same connection

        event.fire(1);
        tctx.assert_now(fire_count == 1, "Signal fired once, not twice (shared Rc)");

        conn1.disconnect();
        event.fire(2);
        tctx.assert_now(
            fire_count == 1, "After disconnect via conn1, signal doesn't fire");

        // conn2 should also be disconnected since they share the same impl
        event.fire(3);
        tctx.assert_now(fire_count == 1, "conn2 also disconnected (shared impl)");
    }

    // Test manual disconnect
    {
        Event<int> event;
        int        fire_count = 0;

        auto conn = event.signal().connect([&](int) { fire_count++; });

        event.fire(1);
        tctx.assert_now(fire_count == 1, "Signal fired before disconnect");

        bool disconnected = conn.disconnect();
        tctx.assert_now(disconnected, "Disconnect returned true");

        event.fire(2);
        tctx.assert_now(fire_count == 1, "Signal not fired after manual disconnect");

        bool second_disconnect = conn.disconnect();
        tctx.assert_now(
            !second_disconnect,
            "Second disconnect returned false (already disconnected)");
    }

    // Test auto-disconnect when connection dropped
    {
        Event<int> event;
        int        fire_count = 0;

        {
            auto conn = event.signal().connect([&](int) { fire_count++; });
            event.fire(1);
            tctx.assert_now(fire_count == 1, "Signal fired while connection alive");
        } // conn destroyed here

        event.fire(2);
        tctx.assert_now(fire_count == 1, "Signal not fired after connection destroyed");
    }

    // Test connection removal with swap-and-pop (index fix)
    {
        Event<int> event;
        int        fire_count1 = 0, fire_count2 = 0, fire_count3 = 0;

        auto conn1 = event.signal().connect([&](int) { fire_count1++; });
        auto conn2 = event.signal().connect([&](int) { fire_count2++; });
        auto conn3 = event.signal().connect([&](int) { fire_count3++; });

        event.fire(1);
        tctx.assert_now(
            fire_count1 == 1 && fire_count2 == 1 && fire_count3 == 1,
            "All three connections fired");

        // Disconnect middle connection - this triggers swap with conn3
        conn2.disconnect();

        event.fire(2);
        tctx.assert_now(fire_count1 == 2, "First connection still works");
        tctx.assert_now(fire_count2 == 1, "Second connection disconnected");
        tctx.assert_now(
            fire_count3 == 2, "Third connection still works (index was updated)");

        // Disconnect the swapped connection
        conn3.disconnect();

        event.fire(3);
        tctx.assert_now(fire_count1 == 3, "First connection still works");
        tctx.assert_now(fire_count3 == 2, "Third connection disconnected");
    }

    // Test domain-bound connections
    {
        Event<int> event;
        int        fire_count = 0;

        auto& domain = engine->add_domain<TestDomain>("DomainBoundTest");

        // Connect with domain binding
        event.signal().connect(&domain, [&](int) { fire_count++; });

        event.fire(1);
        tctx.assert_now(fire_count == 1, "Domain-bound connection fired");

        // Destroy domain
        engine->queue_destroy_domain(domain.entity());
        engine->tick();

        event.fire(2);
        tctx.assert_now(
            fire_count == 1,
            "Domain-bound connection auto-disconnected on domain destruction");
    }

    // Test Event wrapper
    {
        Event<int> event;
        int        received = 0;

        auto conn = event.signal().connect([&](int val) { received = val; });

        event.fire(999);
        tctx.assert_now(received == 999, "Event wrapper fired correctly");
    }

    // Test Event sharing (multiple holders)
    {
        Event<int> event;
        int        fire_count = 0;

        auto signal1 = event.signal();
        auto signal2 = event.signal();

        auto conn1 = signal1.connect([&](int) { fire_count++; });
        auto conn2 = signal2.connect([&](int) { fire_count++; });

        event.fire(1);
        tctx.assert_now(fire_count == 2, "Both connections to shared signal fired");
    }

    // Test refcount correctness (raw pointer storage)
    {
        Event<int> event;
        int        fire_count = 0;

        SignalConnection conn;

        {
            auto temp_conn = event.signal().connect([&](int) { fire_count++; });
            conn           = temp_conn; // Copy

            event.fire(1);
            tctx.assert_now(fire_count == 1, "Connection works while copies exist");

            // temp_conn destroyed here, but conn still holds a reference
        }

        event.fire(2);
        tctx.assert_now(
            fire_count == 2, "Connection still works after one copy destroyed");

        conn = SignalConnection(); // Release last reference

        event.fire(3);
        tctx.assert_now(
            fire_count == 2, "Connection auto-disconnected when refcount hit 0");
    }

    // Test ThreadSafeSignal with async tasks
    {
        auto* async_ctx = engine->add_ctx<AsyncContext>(4);

        ThreadSafeEvent<int> event(*engine);
        auto                 signal = event.signal();
        std::atomic<int>     total{ 0 };
        std::atomic<int>     fire_count{ 0 };

        // Connect from multiple threads - store connections to keep them alive
        std::vector<SignalConnection> connections;
        for (int i = 0; i < 10; i++)
        {
            connections.push_back(signal.connect(
                [&total, &fire_count](int val)
                {
                    total.fetch_add(val, std::memory_order_relaxed);
                    fire_count.fetch_add(1, std::memory_order_relaxed);
                }));
        }

        // Fire from multiple threads
        for (int i = 0; i < 100; i++)
        {
            async_ctx->task<void>([&event, i]() { event.fire(i); });
        }

        // Wait for all tasks
        async_ctx->update();
        v::time::sleep_ms(100);
        engine->tick(); // Execute deferred signal fires

        tctx.assert_now(
            fire_count.load() == 1000,
            "ThreadSafeSignal fired 100 times to 10 connections");

        // Sum of 0+1+2+...+99 = 4950, times 10 connections = 49500
        tctx.assert_now(
            total.load() == 49500, "ThreadSafeSignal accumulated correct total");
    }

    // Performance benchmark: connection and firing
    {
        constexpr int NUM_CONNECTIONS = 10000;
        constexpr int NUM_FIRES       = 1000;

        Event<int>                    event;
        std::vector<SignalConnection> connections;
        int                           fire_count = 0;

        // Benchmark connection
        Stopwatch sw_connect;
        for (int i = 0; i < NUM_CONNECTIONS; i++)
        {
            connections.push_back(event.signal().connect([&](int) { fire_count++; }));
        }
        f64 connect_time = sw_connect.elapsed();

        // Benchmark firing
        Stopwatch sw_fire;
        for (int i = 0; i < NUM_FIRES; i++)
        {
            event.fire(i);
        }
        f64 fire_time = sw_fire.elapsed();

        tctx.assert_now(
            fire_count == NUM_CONNECTIONS * NUM_FIRES, "All connections fired correctly");

        LOG_TRACE(
            "[signal] {} connections created in {:.6f}s ({:.2f} conn/s)", NUM_CONNECTIONS,
            connect_time, NUM_CONNECTIONS / connect_time);
        LOG_TRACE(
            "[signal] {} fires to {} connections in {:.6f}s ({:.2f} fires/s, {:.2f}M "
            "calls/s)",
            NUM_FIRES, NUM_CONNECTIONS, fire_time, NUM_FIRES / fire_time,
            (NUM_FIRES * NUM_CONNECTIONS) / fire_time / 1e6);
    }

    // Performance benchmark: disconnection (swap-and-pop)
    {
        constexpr int NUM_CONNECTIONS = 10000;

        Event<int>                    event;
        std::vector<SignalConnection> connections;

        for (int i = 0; i < NUM_CONNECTIONS; i++)
        {
            connections.push_back(event.signal().connect([](int) {}));
        }

        // Benchmark disconnection
        Stopwatch sw_disconnect;
        for (auto& conn : connections)
        {
            conn.disconnect();
        }
        f64 disconnect_time = sw_disconnect.elapsed();

        LOG_TRACE(
            "[signal] {} disconnections in {:.6f}s ({:.2f} disc/s)", NUM_CONNECTIONS,
            disconnect_time, NUM_CONNECTIONS / disconnect_time);

        // Verify all disconnected
        int  fire_count = 0;
        auto new_conn   = event.signal().connect([&](int) { fire_count++; });
        event.fire(1);
        tctx.assert_now(
            fire_count == 1, "Only new connection fired after mass disconnect");
    }

    // Performance benchmark: auto-disconnect via destructor
    {
        constexpr int NUM_CONNECTIONS = 10000;

        Event<int> event;

        Stopwatch sw_autodisconnect;
        {
            std::vector<SignalConnection> connections;
            for (int i = 0; i < NUM_CONNECTIONS; i++)
            {
                connections.push_back(event.signal().connect([](int) {}));
            }
            // All connections destroyed here
        }
        f64 autodisconnect_time = sw_autodisconnect.elapsed();

        LOG_TRACE(
            "[signal] {} auto-disconnections (destructor) in {:.6f}s ({:.2f} "
            "disc/s)",
            NUM_CONNECTIONS, autodisconnect_time, NUM_CONNECTIONS / autodisconnect_time);

        // Verify all disconnected
        int  fire_count = 0;
        auto new_conn   = event.signal().connect([&](int) { fire_count++; });
        event.fire(1);
        tctx.assert_now(
            fire_count == 1, "Only new connection fired after auto-disconnect");
    }

    // Stress test: rapid connect/disconnect/fire cycles
    {
        Event<int>                    event;
        std::vector<SignalConnection> connections;
        int                           total_fires = 0;

        for (int cycle = 0; cycle < 100; cycle++)
        {
            // Add some connections
            for (int i = 0; i < 10; i++)
            {
                connections.push_back(
                    event.signal().connect([&](int) { total_fires++; }));
            }

            // Fire
            event.fire(1);

            // Remove some connections
            if (connections.size() > 5)
            {
                connections.erase(connections.begin(), connections.begin() + 5);
            }
        }

        tctx.assert_now(total_fires > 0, "Stress test executed fires");
        LOG_TRACE("[signal] Stress test completed: {} total fires", total_fires);
    }

    // Test connection validity checking
    {
        Event<int> event;

        SignalConnection conn1 = event.signal().connect([](int) {});
        SignalConnection conn2; // Default constructed - invalid

        tctx.assert_now(static_cast<bool>(conn1), "Valid connection is truthy");
        tctx.assert_now(!static_cast<bool>(conn2), "Invalid connection is falsy");

        conn1.disconnect();
        // Note: after disconnect, connection is still "valid" (has impl), but
        // disconnected
        tctx.assert_now(
            static_cast<bool>(conn1), "Disconnected connection still has impl");
    }

    // Test domain-bound connection with lambda capturing this
    {
        Event<void> event;

        auto& domain = engine->add_domain<TestDomain>("DomainCaptureTest");

        event.signal().connect(&domain, [&domain]() { domain.counter++; });

        event.fire();
        tctx.assert_now(
            domain.counter == 1, "Domain-bound lambda fired with this capture");

        event.fire();
        tctx.assert_now(domain.counter == 2, "Domain-bound lambda fired again");

        engine->queue_destroy_domain(domain.entity());
        engine->tick();

        event.fire();
        tctx.assert_now(
            domain.counter == 2, "Domain-bound lambda not fired after domain destroyed");
    }

    return tctx.is_failure();
}
