//
// Created by niooi on 7/28/2025.
//

#include <engine/domain.h>
#include <engine/engine.h>

namespace v {
    DomainBase::DomainBase(std::string name) : name_(std::move(name)) {}

    void DomainBase::init_first(Engine& engine, std::optional<entt::entity> entity)
    {
        engine_ = &engine;
        entity_ = entity.value_or(engine.registry().create());
    }

    DomainBase::~DomainBase()
    {
        removing_.fire();
        // entity lifetime is managed by engine
    }

} // namespace v
