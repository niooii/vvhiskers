//
// Created by niooi on 7/30/2025.
//

#include <algorithm>
#include <engine/contexts/render/ctx.h>
#include <engine/contexts/render/render_domain.h>
#include <stdexcept>
#include "engine/contexts/render/init_vk.h"
#include "engine/engine.h"

namespace v {
    RenderContext::RenderContext(Engine& engine, const std::string& shader_root_path) :
        Context(engine), shader_root_path_(shader_root_path)
    {
        // Create Daxa resources
        daxa_resources_ = std::make_unique<DaxaResources>(engine_, shader_root_path_);

        // Init rendering stuff for the window
        auto window_ctx = engine_.get_ctx<WindowContext>();
        if (!window_ctx)
        {
            throw std::runtime_error(
                "Create WindowContext before creating RenderContext");
        }

        Window* win = window_ctx->get_window();
        window_resources_ =
            std::make_unique<WindowRenderResources>(win, daxa_resources_.get(), this);
    }

    RenderContext::~RenderContext() = default;


    void RenderContext::register_render_domain(RenderDomainBase* domain)
    {
        render_domains_.push_back(domain);
        domain_version_++;
        graph_dirty_ = true;
    }

    void RenderContext::unregister_render_domain(RenderDomainBase* domain)
    {
        std::erase(render_domains_, domain);
        domain_version_++;
        graph_dirty_ = true;
    }

    void RenderContext::rebuild_graph()
    {
        LOG_TRACE("Rebuilding render graph (version {})", domain_version_);

        window_resources_->render_graph = daxa::TaskGraph(
            {
                .device                   = daxa_resources_->device,
                .swapchain                = window_resources_->swapchain,
                .record_debug_information = true,
                .name                     = "main loop graph",
            });

        // Re-register persistent resources
        window_resources_->render_graph.use_persistent_image(
            window_resources_->task_swapchain_image);

        // Clear swapchain first (bc windows requires defined initial content)
        window_resources_->render_graph.add_task(
            daxa::Task::Raster("clear_swapchain")
                .color_attachment
                .reads_writes(
                    daxa::ImageViewType::REGULAR_2D,
                    window_resources_->task_swapchain_image)
                .executes(
                    [this](daxa::TaskInterface ti)
                    {
                        auto image_id =
                            ti.get(window_resources_->task_swapchain_image).ids[0];
                        auto image_view =
                            ti.get(window_resources_->task_swapchain_image).view_ids[0];
                        auto image_info = ti.device.image_info(image_id).value();

                        auto render_recorder = std::move(ti.recorder).begin_renderpass({
                            .color_attachments = {{
                                .image_view = image_view,
                                .load_op = daxa::AttachmentLoadOp::CLEAR,
                                .clear_value = std::array<f32, 4>{ 0.1f, 0.1f, 0.1f, 1.0f },
                            }},
                            .render_area = {
                                .x = 0,
                                .y = 0,
                                .width = image_info.size.x,
                                .height = image_info.size.y,
                            },
                        });
                        ti.recorder = std::move(render_recorder).end_renderpass();
                    }));

        // Add tasks from all render domains
        for (auto* domain : render_domains_)
        {
            domain->add_render_tasks(window_resources_->render_graph);
        }

        // Finalize graph
        window_resources_->render_graph.submit({});
        window_resources_->render_graph.present({});
        window_resources_->render_graph.complete({});

        LOG_TRACE(
            "Task graph rebuilt successfully with {} domains", render_domains_.size());
    }

    void RenderContext::update()
    {
        // Rebuild graph if domains changed or manually marked dirty
        if (graph_dirty_ || domain_version_ != last_domain_version_)
        {
            rebuild_graph();
            graph_dirty_         = false;
            last_domain_version_ = domain_version_;
        }

        pre_render.execute();

        // execute the taskgraphs
        window_resources_->render();

        daxa_resources_->device.collect_garbage();
    }
} // namespace v
