//
// Created by niooi on 12/15/2025.
//

#pragma once

#include <defs.h>
#include <mem/ref_counted.h>

#include <absl/synchronization/mutex.h>

#include <functional>
#include <type_traits>
#include <vector>

namespace v {
    class DomainBase;
    class Engine;

    template <typename T>
    class Signal;

    class SignalConnection;

    class SignalConnectionImpl {
        template <typename T>
        friend class SignalImpl;
        template <typename T>
        friend class ThreadSafeSignalImpl;

        template <typename T>
        friend class Signal;
        friend class SignalConnection;

        template <typename T>
        friend class mem::Rc;

    public:
        ~SignalConnectionImpl()
        {
            if (!dcd_ && dc_fn)
            {
                dc_fn(id_);
            }
        }

        /// Returns true iff the disconnect succeeded
        FORCEINLINE bool disconnect()
        {
            if (LIKELY(!dcd_))
            {
                if (dc_fn)
                {
                    bool res = dc_fn(id_);
                    if (LIKELY(res))
                    {
                        dcd_ = true;
                        return true;
                    }
                }
                dcd_ = true;
                return false;
            }
            return false;
        }

    private:
        SignalConnectionImpl(u32 id, std::function<bool(u32)> disconnect_fn) :
            id_(id), dcd_(false), dc_fn(std::move(disconnect_fn))
        {}

        u32                      id_;
        bool                     dcd_;
        std::function<bool(u32)> dc_fn;

        // for conenctions attached to a domain's lifetime
        mem::Rc<SignalConnectionImpl> domain_removing_conn_;
    };

    /// Expose the Signal as a cheaply copyable wrapper
    class SignalConnection {
        template <typename T>
        friend class SignalImpl;
        template <typename T>
        friend class Signal;
        template <typename T>
        friend class ThreadSafeSignalImpl;

    public:
        SignalConnection() = default;

        SignalConnection(const SignalConnection&)            = default;
        SignalConnection& operator=(const SignalConnection&) = default;

        SignalConnection(SignalConnection&&)            = default;
        SignalConnection& operator=(SignalConnection&&) = default;

        /// Manually disconnect the connection
        FORCEINLINE bool disconnect() { return impl_ ? impl_->disconnect() : false; }

        /// If the connection is still valid
        FORCEINLINE explicit operator bool() const { return impl_.get() != nullptr; }

    private:
        explicit SignalConnection(mem::Rc<SignalConnectionImpl> impl) :
            impl_(std::move(impl))
        {}

        mem::Rc<SignalConnectionImpl> impl_;
    };

    namespace detail {
        template <typename T, typename = void>
        struct callback_type {
            using type = std::function<void(T)>;
        };

        template <typename T>
        struct callback_type<T, std::enable_if_t<std::is_void_v<T>>> {
            using type = std::function<void()>;
        };
    } // namespace detail

    template <typename T>
    class SignalImpl {
        template <typename U>
        friend class Signal;
        template <typename U>
        friend class Event;

    public:
        using callback_type = typename detail::callback_type<T>::type;

        SignalImpl() = default;

        ~SignalImpl()
        {
            for (auto& entry : connections_)
            {
                entry.connection->dc_fn = nullptr;
                entry.connection->dcd_  = true;
            }
        }

        /// Connect a callback to run when the signal fires
        SignalConnection connect(callback_type func)
        {
            u32 id = static_cast<u32>(connections_.size());

            auto impl = mem::make_rc<SignalConnectionImpl>(
                id, [this](u32 id) { return this->disconnect_id(id); });

            connections_.push_back({ std::move(func), impl.get() });

            return SignalConnection(impl);
        }

    private:
        /// Fire the signal
        template <typename U = T>
        std::enable_if_t<!std::is_void_v<U>> fire(const U& val)
        {
            for (auto& entry : connections_)
                entry.callback(val);
        }

        /// Fire the signal (void specialization)
        template <typename U = T>
        std::enable_if_t<std::is_void_v<U>> fire()
        {
            for (auto& entry : connections_)
                entry.callback();
        }

        struct ConnectionEntry {
            callback_type         callback;
            SignalConnectionImpl* connection;
        };

        std::vector<ConnectionEntry> connections_{};

        bool disconnect_id(u32 id)
        {
            if (id >= connections_.size())
                return false;

            connections_[id] = std::move(connections_.back());

            if (id < connections_.size() - 1)
            {
                connections_[id].connection->id_ = id;
            }

            connections_.pop_back();
            return true;
        }
    };

    /// A signal fired by an event
    template <typename T>
    class Signal {
        template <typename U>
        friend class Event;

    public:
        using callback_type = typename SignalImpl<T>::callback_type;

        Signal(const Signal&)            = default;
        Signal& operator=(const Signal&) = default;

        Signal(Signal&&)            = default;
        Signal& operator=(Signal&&) = default;

        /// Connect a callback to run when the signal fires
        template <typename F>
        SignalConnection connect(F&& func)
        {
            return impl_->connect(std::forward<F>(func));
        }

        /// Connect with a lifetime bound to a Domain
        template <typename DomainType>
        SignalConnection connect(DomainType* domain, callback_type func)
            requires std::is_base_of_v<DomainBase, DomainType>
        {
            SignalConnection main_conn = connect(std::move(func));

            auto removing_conn = domain->removing().connect([main_conn]() mutable
                                                            { main_conn.disconnect(); });

            main_conn.impl_->domain_removing_conn_ = removing_conn.impl_;

            return main_conn;
        }

        FORCEINLINE explicit operator bool() const { return impl_.get() != nullptr; }

    private:
        explicit Signal(mem::Rc<SignalImpl<T>> impl) : impl_(std::move(impl)) {}

        mem::Rc<SignalImpl<T>> impl_;
    };


    /// A fireable event type with an associated Signal.
    template <typename T>
    class Event {
    public:
        Event() : impl_(mem::make_rc<SignalImpl<T>>()) {}

        FORCEINLINE Signal<T> signal() const { return Signal<T>(impl_); }

        template <typename U = T>
        FORCEINLINE std::enable_if_t<!std::is_void_v<U>> fire(const U& val) const
        {
            impl_->fire(val);
        }

        template <typename U = T>
        FORCEINLINE std::enable_if_t<std::is_void_v<U>> fire() const
        {
            impl_->fire();
        }

    private:
        mem::Rc<SignalImpl<T>> impl_;
    };

    /// A thread safe Signal type
    template <typename T>
    class ThreadSafeSignalImpl {
        template <typename U>
        friend class ThreadSafeSignal;
        template <typename U>
        friend class ThreadSafeEvent;

        template <typename U>
        friend class mem::Rc;

    public:
        using callback_type = typename detail::callback_type<T>::type;

        ~ThreadSafeSignalImpl()
        {
            absl::MutexLock lock(&connections_mutex_);
            for (auto& entry : connections_)
            {
                entry.connection->dc_fn = nullptr;
                entry.connection->dcd_  = true;
            }
        }

        SignalConnection connect(callback_type func)
        {
            absl::MutexLock lock(&connections_mutex_);

            u32 id = static_cast<u32>(connections_.size());

            auto impl = mem::make_rc<SignalConnectionImpl>(
                id, [this](u32 id) { return this->disconnect_id(id); });

            connections_.push_back({ std::move(func), impl.get() });

            return SignalConnection(impl);
        }

    private:
        template <typename U = T>
        std::enable_if_t<!std::is_void_v<U>> fire(const U& val);

        template <typename U = T>
        std::enable_if_t<std::is_void_v<U>> fire();

        struct ConnectionEntry {
            callback_type         callback;
            SignalConnectionImpl* connection;
        };

        Engine&                                   engine_;
        absl::Mutex                               connections_mutex_;
        std::vector<ConnectionEntry> connections_ ABSL_GUARDED_BY(connections_mutex_);

        explicit ThreadSafeSignalImpl(Engine& engine);

        /// these run on the main thread. holy slop
        template <typename U = T>
        std::enable_if_t<!std::is_void_v<U>> fire_deferred(const U& val)
        {
            absl::MutexLock lock(&connections_mutex_);
            for (auto& entry : connections_)
                entry.callback(val);
        }

        template <typename U = T>
        std::enable_if_t<std::is_void_v<U>> fire_deferred()
        {
            absl::MutexLock lock(&connections_mutex_);
            for (auto& entry : connections_)
                entry.callback();
        }

        bool disconnect_id(u32 id)
        {
            absl::MutexLock lock(&connections_mutex_);

            if (id >= connections_.size())
                return false;

            connections_[id] = std::move(connections_.back());

            if (id < connections_.size() - 1)
            {
                connections_[id].connection->id_ = id;
            }

            connections_.pop_back();
            return true;
        }
    };

    // ThreadSafeSignal wrapper
    template <typename T>
    class ThreadSafeSignal {
        template <typename U>
        friend class ThreadSafeEvent;

    public:
        template <typename F>
        SignalConnection connect(F&& func)
        {
            return impl_->connect(std::forward<F>(func));
        }

    private:
        explicit ThreadSafeSignal(Engine& engine) :
            impl_(mem::make_rc<ThreadSafeSignalImpl<T>>(engine))
        {}

        /// TODO! for now, fire on THreadSafeEvents defers the callback to run on the main
        /// thread. I think we should have a fire immediately and fire deferred, one
        /// cannot use the engine but one can
        template <typename U = T>
        std::enable_if_t<!std::is_void_v<U>> fire(const U& val)
        {
            impl_->fire(val);
        }

        template <typename U = T>
        std::enable_if_t<std::is_void_v<U>> fire()
        {
            impl_->fire();
        }

        FORCEINLINE explicit operator bool() const { return impl_.get() != nullptr; }

        explicit ThreadSafeSignal(mem::Rc<ThreadSafeSignalImpl<T>> impl) :
            impl_(std::move(impl))
        {}

        mem::Rc<ThreadSafeSignalImpl<T>> impl_;
    };

    template <typename T>
    class ThreadSafeEvent {
    public:
        explicit ThreadSafeEvent(Engine& engine) :
            impl_(mem::make_rc<ThreadSafeSignalImpl<T>>(engine))
        {}

        FORCEINLINE ThreadSafeSignal<T> signal() const
        {
            return ThreadSafeSignal<T>(impl_);
        }

        template <typename U = T>
        FORCEINLINE std::enable_if_t<!std::is_void_v<U>> fire(const U& val) const
        {
            impl_->fire(val);
        }

        /// Fire the signal (void version)
        template <typename U = T>
        FORCEINLINE std::enable_if_t<std::is_void_v<U>> fire() const
        {
            impl_->fire();
        }

    private:
        explicit ThreadSafeEvent(mem::Rc<ThreadSafeSignalImpl<T>> impl) :
            impl_(std::move(impl))
        {}

        mem::Rc<ThreadSafeSignalImpl<T>> impl_;
    };

} // namespace v
