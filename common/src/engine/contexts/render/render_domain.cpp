//
// Created by niooi on 10/10/2025.
//

#include <engine/contexts/render/ctx.h>
#include <engine/contexts/render/render_domain.h>
#include <engine/engine.h>

namespace v {
    RenderDomainBase::RenderDomainBase() {}

    void RenderDomainBase::init_render(Engine& engine)
    {
        render_ctx_ = engine.get_ctx<RenderContext>();
        if (!render_ctx_)
        {
            throw std::runtime_error(
                "RenderContext must exist before creating RenderDomains");
        }

        render_ctx_->register_render_domain(this);
    }

    RenderDomainBase::~RenderDomainBase()
    {
        if (render_ctx_)
        {
            render_ctx_->unregister_render_domain(this);
        }
    }

    void RenderDomainBase::mark_graph_dirty()
    {
        if (render_ctx_)
        {
            render_ctx_->mark_graph_dirty();
        }
    }
} // namespace v
