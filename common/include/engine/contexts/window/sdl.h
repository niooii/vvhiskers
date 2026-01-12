//
// Created by niooi on 8/5/2025.
//

#pragma once

#include <engine/context.h>
#include <engine/signal.h>

#include "entt/entt.hpp"

// Forward declaration for SDL_Event
union SDL_Event;

namespace v {
    /// Context for managing SDL3 Events subsystem and global event handling
    class SDLContext : public Context<SDLContext> {
    public:
        explicit SDLContext(Engine& engine);
        ~SDLContext();

        /// This MUST be called on the main thread BTW!!
        /// Updates SDL events, pumps events, and routes window events to WindowContext
        /// Should be called before WindowContext::update() in application loop
        void update() const;

        // SDL event signals

        /// Signal fired when SDL_Quit has been fired (e.g. sigkill)
        FORCEINLINE Signal<void> quit() const { return quit_event_.signal(); }

        /// Signal fired for all SDL events that have a window ID associated with them.
        /// This includes: SDL_EVENT_WINDOW_* events, SDL_EVENT_KEY_*, SDL_EVENT_TEXT_*,
        /// SDL_EVENT_MOUSE_*, SDL_EVENT_DROP_*, and SDL_EVENT_FINGER_* events.
        FORCEINLINE Signal<const SDL_Event&> window_event() const
        {
            return window_event_.signal();
        }

        /// Signal fired for all SDL events regardless of whether they have a window ID
        FORCEINLINE Signal<const SDL_Event&> event() const { return event_.signal(); }

    private:
        Event<void>             quit_event_;
        Event<const SDL_Event&> window_event_;
        Event<const SDL_Event&> event_;
    };
} // namespace v
