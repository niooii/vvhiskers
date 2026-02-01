//
// Created by niooi on 7/28/2025.
//

#include <engine/engine.h>
#include <engine/graph.h>
#include <prelude.h>

namespace v {
    Engine::Engine() :
        ctx_entity_{ ctx_registry_.create() }, engine_entity_{ registry_.create() }
    {}

    Engine::~Engine()
    {
        on_destroy.execute();

        // run deferred post-tick tasks
        {
            std::function<void()> fn;
            while (post_tick_queue_.try_dequeue(fn))
            {
                try
                {
                    if (fn)
                        fn();
                }
                catch (const std::exception& ex)
                {
                    LOG_ERROR("post_tick callback threw: {}", ex.what());
                }
                catch (...)
                {
                    LOG_ERROR("post_tick callback threw unknown exception");
                }
            }
        }
    }

    void Engine::tick()
    {
        prev_tick_span_ = tick_time_stopwatch_.reset();

        // if this was the first frame, the deltatime value would probably be kind of
        // huge and not useful, so set dt to 0.
        if (UNLIKELY(current_tick_ == 0))
            prev_tick_span_ = 0;

        current_tick_++;

        // run tick callbacks with dependency management
        on_tick.execute();

        // run deferred post-tick tasks
        {
            std::function<void()> fn;
            while (post_tick_queue_.try_dequeue(fn))
            {
                try
                {
                    if (fn)
                        fn();
                }
                catch (const std::exception& ex)
                {
                    LOG_ERROR("post_tick callback threw: {}", ex.what());
                }
                catch (...)
                {
                    LOG_ERROR("post_tick callback threw unknown exception");
                }
            }
        }

        // LOG_TRACE("Finished tick {} ", current_tick_);

        // flush default logger
        spd::default_logger()->flush();
    }

} // namespace v
