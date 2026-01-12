//
// Created by niooi on 10/10/2025.
//

#pragma once

#include <daxa/daxa.hpp>
#include <daxa/utils/task_graph.hpp>
#include <engine/contexts/render/render_domain.h>

namespace v {
    /// Default render domain that clears the screen to a solid color.
    /// Used as a fallback when no other render domains are active.
    class DefaultRenderDomain : public RenderDomain<DefaultRenderDomain> {
    public:
        DefaultRenderDomain()           = default;
        ~DefaultRenderDomain() override = default;

        void add_render_tasks(daxa::TaskGraph& graph) override;
    };
} // namespace v
