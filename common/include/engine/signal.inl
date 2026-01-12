//
// Template implementations for ThreadSafeSignal
// included at the end of engine.h to avoid circular include
//

#pragma once

#include <engine/engine.h>

namespace v {
    template <typename T>
    ThreadSafeSignalImpl<T>::ThreadSafeSignalImpl(Engine& engine) : engine_(engine) {}

    template <typename T>
    template <typename U>
    std::enable_if_t<!std::is_void_v<U>> ThreadSafeSignalImpl<T>::fire(const U& val)
    {
        auto impl_ptr = this;
        engine_.post_tick([impl_ptr, val]() {
            impl_ptr->fire_deferred(val);
        });
    }

    template <typename T>
    template <typename U>
    std::enable_if_t<std::is_void_v<U>> ThreadSafeSignalImpl<T>::fire()
    {
        auto impl_ptr = this;
        engine_.post_tick([impl_ptr]() {
            impl_ptr->fire_deferred();
        });
    }
} // namespace v
