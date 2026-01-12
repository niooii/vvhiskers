//
// Created by niooi on 7/30/2025.
//

#pragma once

#ifdef _WIN32
    #undef OPAQUE
#endif

#include <daxa/daxa.hpp>
#include <daxa/utils/task_graph.hpp>
#include <defs.h>
#include <engine/context.h>
#include <engine/contexts/window/window.h>
#include <engine/domain.h>
#include <engine/signal.h>
#include <string>
#include <tuple>
#include <vector>
#include "engine/sink.h"

namespace v {
    class RenderDomainBase;
    using RenderComponentFnRender = void(Engine*, class RenderContext*, Window*);
    using RenderComponentFnResize = void(Engine*, RenderContext*, Window*);

    struct DaxaResources;

    struct WindowRenderResources {
        WindowRenderResources(const WindowRenderResources& other) = delete;

        WindowRenderResources& operator=(const WindowRenderResources& other) = delete;

        WindowRenderResources(
            Window* window, DaxaResources* daxa_resources,
            class RenderContext* render_ctx);
        ~WindowRenderResources();

        void render();
        // called to resize the swapchain
        void resize();

        daxa::Swapchain      swapchain;
        daxa::TaskGraph      render_graph;
        daxa::TaskImage      task_swapchain_image;
        static constexpr u32 FRAMES_IN_FLIGHT = 2;

        DaxaResources* daxa_resources_;
        RenderContext* render_ctx_;

        bool resize_queued{};

        SignalConnection resize_conn_;
    };

    class RenderContext : public Context<RenderContext> {
        friend struct WindowRenderResources;
        friend class RenderDomainBase;

    public:
        explicit RenderContext(
            Engine& engine, const std::string& shader_root_path = "./resources/shaders");
        ~RenderContext();

        /// Tasks that should run before the rendering of a frame
        DependentSink pre_render;

        struct RenderEventArgs {
            Engine*        engine;
            RenderContext* render_ctx;
            Window*        window;
        };

        // Render event signals
        /// Signal fired before rendering a frame
        FORCEINLINE Signal<RenderEventArgs> prerender() const
        {
            return pre_render_event_.signal();
        }

        /// Signal fired after rendering a frame
        FORCEINLINE Signal<RenderEventArgs> postrender() const
        {
            return post_render_event_.signal();
        }

        /// Signal fired when window is resized
        FORCEINLINE Signal<RenderEventArgs> resized() const
        {
            return resize_event_.signal();
        }

        /// Get raw Daxa resources (device, pipeline manager, etc.)
        /// @note Returned reference is valid for the lifetime of RenderContext
        DaxaResources& daxa_resources() { return *daxa_resources_; }

        /// Get the swapchain task image for rendering to screen
        /// @note Used by RenderDomains to render to the swapchain
        daxa::TaskImage& swapchain_image()
        {
            return window_resources_->task_swapchain_image;
        }

        /// Get the swapchain format
        /// @note Useful for creating pipelines with matching color attachment formats
        daxa::Format get_swapchain_format() const
        {
            return window_resources_->swapchain.get_format();
        }

        /// Get the swapchain extent (dimensions)
        /// @note Useful for creating images that match swapchain size
        daxa::Extent2D get_swapchain_extent() const
        {
            return window_resources_->swapchain.get_surface_extent();
        }

        /// Mark the task graph as dirty, forcing a rebuild on the next frame.
        /// Automatically called when render domains are added/removed, or when
        /// swapchain is resized. Call manually if resources are reallocated in ways
        /// that affect graph structure (buffer resize, format change, etc.)
        void mark_graph_dirty() { graph_dirty_ = true; }

        void update();

    private:
        /// Register a render domain (called automatically from RenderDomainBase
        /// constructor)
        void register_render_domain(RenderDomainBase* domain);

        /// Unregister a render domain (called automatically from RenderDomainBase
        /// destructor)
        void unregister_render_domain(RenderDomainBase* domain);

        /// Rebuild the task graph from all registered render domains
        void rebuild_graph();

        std::string                    shader_root_path_;
        std::unique_ptr<DaxaResources> daxa_resources_;

        /// Daxa resources for the for now singleton window
        std::unique_ptr<WindowRenderResources> window_resources_;

        /// All registered render domains in registration order
        std::vector<RenderDomainBase*> render_domains_;

        /// Incremented when domains are added/removed to detect changes
        u64 domain_version_      = 0;
        u64 last_domain_version_ = 0;

        /// Set to true when graph structure must be rebuilt (domain changes, resize,
        /// etc.)
        bool graph_dirty_ = false;

        // Internal events
        Event<RenderEventArgs> pre_render_event_;
        Event<RenderEventArgs> post_render_event_;
        Event<RenderEventArgs> resize_event_;
    };
} // namespace v
