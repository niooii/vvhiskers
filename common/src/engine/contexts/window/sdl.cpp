//
// Created by niooi on 8/5/2025.
//

#include <SDL3/SDL.h>
#include <engine/contexts/window/sdl.h>
#include <engine/contexts/window/window.h>

#include "engine/engine.h"

namespace v {
    static bool has_window_id(Uint32 event_type);

    SDLContext::SDLContext(Engine& engine) : Context(engine)
    {
        SDL_InitSubSystem(SDL_INIT_EVENTS);
    }

    SDLContext::~SDLContext()
    {
        SDL_QuitSubSystem(SDL_INIT_EVENTS);
        LOG_INFO("Shutdown SDLContext.");
    }

    void SDLContext::update() const
    {
        SDL_Event event;

        // Poll and route events
        while (SDL_PollEvent(&event))
        {
            // Fire event for all events
            event_.fire(event);

            if (has_window_id(event.type))
            {
                // Fire window event for window-related events
                window_event_.fire(event);
            }
            else
            {
                // Handle global non-window related events
                switch (event.type)
                {
                case SDL_EVENT_QUIT:
                    quit_event_.fire();
                    break;
                default:;
                }
            }
        }
    }


    // NOTE: If this function is ever changed, update the doc comments for
    // SDLComponent::on_win_event to reflect the new list of events
    static bool has_window_id(Uint32 event_type)
    {
        switch (event_type)
        {
            /* All events that have a windowID */
        case SDL_EVENT_WINDOW_SHOWN:
        case SDL_EVENT_WINDOW_HIDDEN:
        case SDL_EVENT_WINDOW_EXPOSED:
        case SDL_EVENT_WINDOW_MOVED:
        case SDL_EVENT_WINDOW_RESIZED:
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        case SDL_EVENT_WINDOW_MINIMIZED:
        case SDL_EVENT_WINDOW_MAXIMIZED:
        case SDL_EVENT_WINDOW_RESTORED:
        case SDL_EVENT_WINDOW_MOUSE_ENTER:
        case SDL_EVENT_WINDOW_MOUSE_LEAVE:
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
        case SDL_EVENT_WINDOW_FOCUS_LOST:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        case SDL_EVENT_WINDOW_HIT_TEST:
        case SDL_EVENT_WINDOW_ICCPROF_CHANGED:
        case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
        case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
        case SDL_EVENT_WINDOW_OCCLUDED:
        case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
        case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
        case SDL_EVENT_WINDOW_DESTROYED:
        case SDL_EVENT_WINDOW_SAFE_AREA_CHANGED:
        case SDL_EVENT_WINDOW_HDR_STATE_CHANGED:
        case SDL_EVENT_WINDOW_METAL_VIEW_RESIZED:

        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
        case SDL_EVENT_TEXT_EDITING:
        case SDL_EVENT_TEXT_INPUT:

        case SDL_EVENT_MOUSE_MOTION:
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
        case SDL_EVENT_MOUSE_WHEEL:

        case SDL_EVENT_DROP_FILE:
        case SDL_EVENT_DROP_TEXT:
        case SDL_EVENT_DROP_BEGIN:
        case SDL_EVENT_DROP_COMPLETE:

        case SDL_EVENT_FINGER_DOWN:
        case SDL_EVENT_FINGER_UP:
        case SDL_EVENT_FINGER_MOTION:
            return true;
        default:
            return false;
        }
    }
} // namespace v
