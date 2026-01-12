// Async context integration test

#include <chrono>
#include <engine/contexts/async/async.h>
#include <test.h>
#include <thread>
#include <time/stopwatch.h>
#include "input/names.h"

using namespace v;

int main()
{
    auto [engine, tctx] = testing::init_test("async");

    auto* async_ctx = engine->add_ctx<AsyncContext>(4);

    // Register coroutine scheduler update
    engine->on_tick.connect({}, {}, "async_coro", [async_ctx]() { async_ctx->update(); });

    // task creation and execution
    {
        bool task_executed = false;
        auto future        = async_ctx->task(
            [&task_executed]()
            {
                task_executed = true;
                return 42;
            });

        // wait for task to complete
        future.wait();
        int result = future.get();

        tctx.assert_now(task_executed, "Task function executed");
        tctx.assert_now(result == 42, "Task returned correct value");
    }

    // multiple concurrent tasks
    {
        constexpr int          num_tasks = 10;
        std::vector<Task<int>> futures;
        std::atomic<int>       tasks_executed{ 0 };

        for (int i = 0; i < num_tasks; ++i)
        {
            futures.emplace_back(async_ctx->task(
                [&tasks_executed, i]()
                {
                    // Simulate some work
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    tasks_executed.fetch_add(1);
                    return i * 2;
                }));
        }

        // Wait for all tasks
        for (auto& future : futures)
        {
            future.wait();
        }

        // Verify all tasks executed
        tctx.assert_now(
            tasks_executed.load() == num_tasks, "All concurrent tasks executed");

        // Verify results
        for (int i = 0; i < num_tasks; ++i)
        {
            int result = futures[i].get();
            tctx.assert_now(
                result == i * 2,
                fmt::format("Task {} returned correct value", i).c_str());
        }
    }

    // wait_for()
    {
        auto future = async_ctx->task(
            []()
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                return 123;
            });

        // Should timeout (50ms < 100ms sleep)
        Stopwatch sw;
        future.wait_for(std::chrono::milliseconds(50));
        f64 elapsed = sw.elapsed();

        tctx.assert_now(elapsed >= 0.04, "wait_for() respected timeout");

        // Now wait for completion
        future.wait();
        int result = future.get();
        tctx.assert_now(result == 123, "Task completed after timeout");
    }

    // different return types
    {
        // String task
        auto string_future = async_ctx->task([]() { return std::string("hello world"); });
        string_future.wait();
        auto str_result = string_future.get();
        tctx.assert_now(
            str_result == "hello world", "String task returned correct value");

        // Void task
        bool actual_void_executed = false;
        auto void_future =
            async_ctx->task([&actual_void_executed]() { actual_void_executed = true; });
        void_future.wait();
        tctx.assert_now(actual_void_executed, "Void task executed");

        // Test void task with .then() callback
        bool void_callback_executed = false;
        auto void_future_with_callback =
            async_ctx->task([&actual_void_executed]() { actual_void_executed = true; });
        void_future_with_callback.then([&void_callback_executed]()
                                       { void_callback_executed = true; });
        void_future_with_callback.wait();
        for (int i = 0; i < 10; ++i)
        {
            engine->tick();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            if (void_callback_executed)
                break;
        }
        tctx.assert_now(void_callback_executed, "Void task .then() callback executed");
    }

    // long-running computation
    {
        auto future = async_ctx->task(
            []()
            {
                int sum = 0;
                for (int i = 0; i < 1000000; ++i)
                {
                    sum += i % 1000;
                }
                return sum;
            });

        future.wait();
        int result = future.get();
        tctx.assert_now(result > 0, "Long computation completed");
    }

    // Exception handling tests
    {
        auto future = async_ctx->task(
            []()
            {
                throw std::runtime_error("Test exception");
                return 42;
            });

        // This should handle the exception gracefully
        try
        {
            future.wait();
            future.get(); // This should throw
            tctx.assert_now(false, "Expected exception but none was thrown");
        }
        catch (const std::runtime_error& e)
        {
            tctx.assert_now(
                std::string(e.what()) == "Test exception",
                "Exception propagated correctly");
        }
    }

    // .then() should not trigger on exception
    {
        bool callback_executed = false;
        auto future            = async_ctx->task(
            []()
            {
                throw std::runtime_error("Test exception");
                return 42;
            });

        future.then([&callback_executed](int result) { callback_executed = true; });

        try
        {
            future.wait();
        }
        catch (...)
        {
            // Expected exception
        }

        for (int i = 0; i < 10; ++i)
        {
            engine->tick();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        tctx.assert_now(!callback_executed, ".then() callback not executed on exception");
    }

    // .or_else() should not trigger on success
    {
        bool error_callback_executed = false;
        auto future                  = async_ctx->task([]() { return 42; });

        future.or_else([&error_callback_executed](std::exception_ptr e)
                       { error_callback_executed = true; });

        future.wait();
        for (int i = 0; i < 10; ++i)
        {
            engine->tick();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        tctx.assert_now(
            !error_callback_executed, ".or_else() callback not executed on success");
    }

    // .then() callback registration
    {
        bool callback_executed = false;
        int  callback_result   = 0;
        auto future            = async_ctx->task(
            []()
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                return 555;
            });

        future.then(
            [&callback_executed, &callback_result](int result)
            {
                callback_executed = true;
                callback_result   = result;
                // This should execute on the engine's main thread
            });

        // Wait and tick the engine to process callbacks
        future.wait();
        for (int i = 0; i < 10; ++i)
        {
            engine->tick();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            if (callback_executed)
                break;
        }

        tctx.assert_now(callback_executed, ".then() callback executed");
        tctx.assert_now(
            callback_result == 555, ".then() callback received correct value");
    }

    // .then() called after completion
    {
        bool callback_executed = false;
        auto future            = async_ctx->task([]() { return 777; });

        // Wait for completion first
        future.wait();

        // Then register callback - should execute immediately
        future.then([&callback_executed](int result) { callback_executed = true; });

        // Tick the engine to process immediate callback
        for (int i = 0; i < 5; ++i)
        {
            engine->tick();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            if (callback_executed)
                break;
        }

        tctx.assert_now(
            callback_executed,
            ".then() executed immediately when future already completed");
    }

    // .or_else() tests
    {
        bool error_callback_executed = false;
        auto future                  = async_ctx->task(
            []()
            {
                throw std::runtime_error("Test error");
                return 42;
            });

        future.or_else(
            [&error_callback_executed](std::exception_ptr e)
            {
                error_callback_executed = true;
                try
                {
                    std::rethrow_exception(e);
                }
                catch (const std::runtime_error& ex)
                {
                    // Expected exception type
                }
            });

        future.then([](int a) { LOG_INFO("yay"); });

        try
        {
            future.wait();
        }
        catch (...)
        {
            // Expected exception
        }

        for (int i = 0; i < 10; ++i)
        {
            engine->tick();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            if (error_callback_executed)
                break;
        }

        tctx.assert_now(error_callback_executed, ".or_else() callback executed");
    }

    // .or_else() called after completion
    {
        bool error_callback_executed = false;
        auto future                  = async_ctx->task(
            []()
            {
                throw std::runtime_error("Test error");
                return 42;
            });

        // Wait for completion first
        try
        {
            future.wait();
        }
        catch (...)
        {
            // Expected exception
        }

        // Then register callback - should execute immediately
        future.or_else([&error_callback_executed](std::exception_ptr e)
                       { error_callback_executed = true; });

        // Tick the engine to process immediate callback
        for (int i = 0; i < 5; ++i)
        {
            engine->tick();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            if (error_callback_executed)
                break;
        }

        tctx.assert_now(
            error_callback_executed,
            ".or_else() executed immediately when future already completed with "
            "exception");
    }

    // Coroutine basic spawn and sleep
    {
        bool coroutine_executed = false;
        auto coro               = async_ctx->spawn(
            [&coroutine_executed](CoroutineInterface& ci) -> Coroutine<void>
            {
                coroutine_executed = true;
                co_return;
            });

        // Tick until coroutine completes
        while (!coro.done())
        {
            engine->tick();
        }

        tctx.assert_now(coroutine_executed, "Basic coroutine executed");
    }

    bool completed{};
    // Coroutine sleep test
    {
        Stopwatch sw{};
        auto      coro = async_ctx->spawn(
            [&](CoroutineInterface& ci) -> Coroutine<void>
            {
                co_await ci.sleep(100); // 100ms
                LOG_DEBUG("finised sleeping");
                LOG_DEBUG("{}", sw.elapsed());
                tctx.assert_now(
                    sw.elapsed() > 0.1 && sw.elapsed() < 0.2,
                    "Coroutine slept for ~100ms");
                // TODO! setting this is a segfault, thats kinda bad
                completed = true;
                // LOG_INFO("we done");
                co_return;
            });

        // Tick until coroutine completes
        while (!coro.done())
        {
            engine->tick();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        tctx.assert_now(completed, "Coroutine that slept for 100ms completed");
    }

    // Coroutine with return value
    {
        auto coro = async_ctx->spawn(
            [](CoroutineInterface& ci) -> Coroutine<int>
            {
                co_await ci.sleep(50);
                co_return 42;
            });

        bool callback_executed = false;
        int  callback_result   = 0;
        coro.then(
            [&callback_executed, &callback_result](int result)
            {
                LOG_INFO("YAY");
                callback_executed = true;
                callback_result   = result;
            });

        // Tick until coroutine completes
        while (!coro.done())
        {
            engine->tick();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        engine->tick();

        tctx.assert_now(callback_executed, "Coroutine .then() callback executed");
        tctx.assert_now(callback_result == 42, "Coroutine returned correct value");
    }

    // Coroutine exception handling
    {
        auto coro = async_ctx->spawn(
            [](CoroutineInterface& ci) -> Coroutine<int>
            {
                co_await ci.sleep(10);
                throw std::runtime_error("Coroutine exception");
                co_return 0;
            });

        bool error_callback_executed = false;
        coro.or_else([&error_callback_executed](std::exception_ptr e)
                     { error_callback_executed = true; });

        // Tick until coroutine completes
        while (!coro.done())
        {
            engine->tick();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        tctx.assert_now(
            error_callback_executed, "Coroutine .or_else() callback executed");
    }

    // Multiple co_awaits in single coroutine
    {
        int  step = 0;
        auto coro = async_ctx->spawn(
            [&step](CoroutineInterface& ci) -> Coroutine<void>
            {
                step = 1;
                co_await ci.sleep(50);
                step = 2;
                co_await ci.sleep(50);
                step = 3;
                co_await ci.sleep(50);
                step = 4;
                co_return;
            });

        // Tick until coroutine completes
        while (!coro.done())
        {
            engine->tick();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        tctx.assert_now(step == 4, "Multiple co_awaits completed successfully");
    }

    // While loop with co_await
    {
        int  tick_count = 0;
        auto coro       = async_ctx->spawn(
            [&tick_count](CoroutineInterface& ci) -> Coroutine<void>
            {
                while (co_await ci.sleep(50))
                {
                    tick_count++;
                    if (tick_count >= 3)
                        break;
                }
                co_return;
            });

        // Tick until coroutine completes
        while (!coro.done())
        {
            engine->tick();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        tctx.assert_now(tick_count == 3, "While loop with co_await executed 3 times");
    }

    // Multiple concurrent coroutines
    // {
    //     std::atomic<int> counter{ 0 };
    //     constexpr int    num_coros = 5;

    //     for (int i = 0; i < num_coros; ++i)
    //     {
    //         async_ctx->spawn(
    //             [&counter, i](CoroutineInterface& ci) -> Coroutine<void>
    //             {
    //                 co_await ci.sleep(i * 20); // Stagger wake times
    //                 counter.fetch_add(1);
    //                 co_return;
    //             });
    //     }

    //     // Tick until all complete (max wait 500ms)
    //     for (int i = 0; i < 50; ++i)
    //     {
    //         engine->tick();
    //         std::this_thread::sleep_for(std::chrono::milliseconds(10));
    //         if (counter.load() == num_coros)
    //             break;
    //     }

    //     tctx.assert_now(
    //         counter.load() == num_coros, "All concurrent coroutines completed");
    // }

    return tctx.is_failure();
}
