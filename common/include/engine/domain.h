//
// Created by niooi on 7/28/2025.
//
// Inspired by:
// https://voxely.net/blog/object-oriented-entity-component-system-design/
// and https://voxely.net/blog/the-perfect-voxel-engine/
//

#pragma once

#include <defs.h>
#include <engine/signal.h>
#include <engine/traits.h>
#include <entt/entt.hpp>
#include <mem/owned_ptr.h>
#include <memory>

namespace v {
    class Engine;

    /// Base class for all domains.
    ///
    /// The engine reference is set internally, the constructor should not
    /// reference the engine in any form, including the attach<>, get<> and other
    /// ECS methods.
    /// Engine-dependent initialization must be done in the init() method.
    ///
    /// Example:
    ///   class MyDomain : public Domain<MyDomain> {
    ///   public:
    ///       MyDomain(int my_param) : my_param_(my_param) {}
    ///       void init() override { attach<SomeComponent>(); }
    ///   private:
    ///       int my_param_;
    ///   };
    ///
    ///   engine.add_domain<MyDomain>(42);
    class DomainBase {
        friend class Engine;

    protected:
        DomainBase(std::string name);

        void
        init_first(Engine& engine, std::optional<entt::entity> entity = std::nullopt);

        Engine& engine()
        {
#if !defined(V_RELEASE)
            if (UNLIKELY(!engine_))
            {
                LOG_CRITICAL("Domain::engine() cannot be accessed during construction");
                throw std::logic_error(
                    "Domain::engine() cannot be accessed during construction");
            }
#endif
            return *engine_;
        }

        const Engine& engine() const
        {
#if !defined(V_RELEASE)
            if (UNLIKELY(!engine_))
                throw std::logic_error("Domain::engine() called before initialization");
#endif
            return *engine_;
        }

    public:
        virtual ~DomainBase();

        // Domains are non-copyable and non-movable
        DomainBase(const DomainBase&)            = delete;
        DomainBase& operator=(const DomainBase&) = delete;
        DomainBase(DomainBase&&)                 = delete;
        DomainBase& operator=(DomainBase&&)      = delete;

        FORCEINLINE entt::entity entity() const { return entity_; }
        FORCEINLINE std::string_view name() const { return name_; }

        /// Fired when domain is about to be destroyed
        FORCEINLINE Signal<void> removing() { return removing_.signal(); }

        /// Check if the domain's entity has component T
        template <typename T>
        bool has() const;

        /// Try to get component T from the domain's entity, returns nullptr if not found
        template <typename T>
        T* try_get();

        /// Try to get component T from the domain's entity (const version), returns
        /// nullptr if not found
        template <typename T>
        const T* try_get() const;

        /// Get component T from the domain's entity, throws if component doesn't exist
        template <typename T>
        T& get();

        /// Get component T from the domain's entity (const version), throws if component
        /// doesn't exist
        template <typename T>
        const T& get() const;

        /// Shorthand to attach a component to the domain (its entity)
        template <typename T, typename... Args>
        T& attach(Args&&... args);

        /// Remove component T from the domain's entity, returns number of components
        /// removed
        template <typename T>
        usize remove();

        /// Convenience method to get a context from the engine
        template <typename T>
        T* get_ctx();

        template <typename T>
        const T* get_ctx() const;

        /// Get a view of all entities with components Ts from the engine
        template <typename... Ts>
        auto view();

    protected:
        mutable Engine* engine_ = nullptr;
        std::string     name_;
        entt::entity    entity_;

    private:
        Event<void> removing_;
    };

    template <typename Derived>
    class Domain : public DomainBase, public QueryBy<mem::owned_ptr<Derived>> {
    protected:
        Domain(std::string name = std::string{ type_name<Derived>() }) :
            DomainBase(std::move(name))
        {}

    public:
        virtual ~Domain() = default;

        virtual void init() {}
    };

    /// A singleton domain which does not permit the creation of multiple
    /// instances of itself in the same Engine container.
    template <typename Derived>
    class SDomain : public Domain<Derived> {
    protected:
        SDomain(const std::string& name = std::string{ type_name<Derived>() }) :
            Domain<Derived>(name)
        {}

    public:
        virtual void init() override {}
    };
} // namespace v
