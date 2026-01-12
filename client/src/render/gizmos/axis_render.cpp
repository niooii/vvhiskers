//
// Created by niooi on 10/17/2025.
//

#include <render/gizmos/axis_render.h>
#include "daxa/utils/task_graph.hpp"
#include "engine/contexts/render/render_domain.h"

namespace v {
    void WorldAxesRenderer::init()
    {
        RenderDomain::init();
        TODO()
    }

    WorldAxesRenderer::~WorldAxesRenderer() {}

    void WorldAxesRenderer::add_render_tasks(daxa::TaskGraph& graph) {}
} // namespace v
