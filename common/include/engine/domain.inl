//
// Template implementations for DomainBase
// included at the end of engine.h to avoid circular include
//

#pragma once

namespace v {
    template <typename T>
    bool DomainBase::has() const
    {
        return engine().has_component<T>(entity());
    }

    template <typename T>
    T* DomainBase::try_get()
    {
        return engine().try_get_component<T>(entity());
    }

    template <typename T>
    const T* DomainBase::try_get() const
    {
        return engine().try_get_component<T>(entity());
    }

    template <typename T>
    T& DomainBase::get()
    {
        return engine().get_component<T>(entity());
    }

    template <typename T>
    const T& DomainBase::get() const
    {
        return engine().get_component<T>(entity());
    }

    template <typename T, typename... Args>
    T& DomainBase::attach(Args&&... args)
    {
        return engine().add_component<T>(entity(), std::forward<Args>(args)...);
    }

    template <typename T>
    usize DomainBase::remove()
    {
        return engine().remove_component<T>(entity());
    }

    template <typename T>
    T* DomainBase::get_ctx()
    {
        return engine().get_ctx<T>();
    }

    template <typename T>
    const T* DomainBase::get_ctx() const
    {
        return engine().get_ctx<T>();
    }

    template <typename... Ts>
    auto DomainBase::view()
    {
        return engine().view<Ts...>();
    }
} // namespace v
