//
// Created by niooi on 12/15/2025.
//

#pragma once

#include <defs.h>
#include <memory>

namespace v::mem {
    struct in_place_t {
        explicit in_place_t() = default;
    };
    inline constexpr in_place_t in_place{};

    /// A reference counted smart pointer, not atomic to avoid unneeded overhead
    template <typename T>
    class Rc {
    public:
        constexpr Rc() noexcept : ptr_(nullptr), blk_(nullptr) {}

        constexpr Rc(std::nullptr_t) noexcept : ptr_(nullptr), blk_(nullptr) {}

        // Value constructor - creates new T with given args
        template <typename... Args>
        explicit Rc(in_place_t, Args&&... args)
        {
            ptr_ = new T(std::forward<Args>(args)...);
            blk_ = new ControlBlock{ 1 };
        }

        // Non-const lvalue copy
        Rc(Rc& other) noexcept : Rc(static_cast<const Rc&>(other)) {}

        // Copy constructor
        Rc(const Rc& other) noexcept : ptr_(other.ptr_), blk_(other.blk_)
        {
            if (blk_)
            {
                blk_->refc++;
            }
        }

        // Copy assignment
        Rc& operator=(const Rc& other) noexcept
        {
            if (this != &other)
            {
                release();
                ptr_ = other.ptr_;
                blk_ = other.blk_;
                if (blk_)
                {
                    blk_->refc++;
                }
            }
            return *this;
        }

        // Move constructor
        Rc(Rc&& other) noexcept : ptr_(other.ptr_), blk_(other.blk_)
        {
            other.ptr_ = nullptr;
            other.blk_ = nullptr;
        }

        // Move assignment
        Rc& operator=(Rc&& other) noexcept
        {
            if (this != &other)
            {
                release();
                ptr_       = other.ptr_;
                blk_       = other.blk_;
                other.ptr_ = nullptr;
                other.blk_ = nullptr;
            }
            return *this;
        }

        // Destructor
        ~Rc() { release(); }

        // Access operators
        T& operator*() const noexcept { return *ptr_; }

        T* operator->() const noexcept { return ptr_; }

        // Get raw pointer
        T* get() const noexcept { return ptr_; }

        // Check if non-null
        explicit operator bool() const noexcept { return ptr_ != nullptr; }

        // Reset to null
        void reset() noexcept
        {
            release();
            ptr_ = nullptr;
            blk_ = nullptr;
        }

        // Reset with new value
        template <typename... Args>
        void reset(Args&&... args)
        {
            release();
            ptr_ = new T(std::forward<Args>(args)...);
            blk_ = new ControlBlock{ 1 };
        }

        // Get reference count
        u64 use_count() const noexcept { return blk_ ? blk_->refc : 0; }

    private:
        struct ControlBlock {
            u64 refc;
        };

        void release()
        {
            if (blk_)
            {
                blk_->refc--;
                if (blk_->refc == 0)
                {
                    delete ptr_;
                    delete blk_;
                }
            }
        }

        T*            ptr_;
        ControlBlock* blk_;
    };

    // Helper function to create Rc with type deduction
    template <typename T, typename... Args>
    Rc<T> make_rc(Args&&... args)
    {
        return Rc<T>(in_place, std::forward<Args>(args)...);
    }

    /// haha actually its just std::shared ptr!
    template <typename T>
    class Arc {
    public:
        constexpr Arc() noexcept : ptr_(nullptr) {}

        // nullptr constructor
        constexpr Arc(std::nullptr_t) noexcept : ptr_(nullptr) {}

        template <typename... Args>
        Arc(Args&&... args)
        {
            ptr_ = std::make_shared<T>(std::forward<Args>(args)...);
        }

        // Copy constructor
        Arc(const Arc& other) noexcept = default;

        // Copy assignment
        Arc& operator=(const Arc& other) noexcept = default;

        // Move constructor
        Arc(Arc&& other) noexcept = default;

        // Move assignment
        Arc& operator=(Arc&& other) noexcept = default;

        // Destructor
        ~Arc() = default;

        // Access operators
        T& operator*() const noexcept { return *ptr_; }

        T* operator->() const noexcept { return ptr_.get(); }

        // Get raw pointer
        T* get() const noexcept { return ptr_.get(); }

        // Check if non-null
        explicit operator bool() const noexcept { return ptr_ != nullptr; }

        // Reset to null
        void reset() noexcept { ptr_.reset(); }

        // Reset with new value
        template <typename... Args>
        void reset(Args&&... args)
        {
            ptr_ = std::make_shared<T>(std::forward<Args>(args)...);
        }

        // Get reference count
        u64 use_count() const noexcept { return ptr_.use_count(); }

    private:
        std::shared_ptr<T> ptr_;
    };
} // namespace v::mem

// aliases for now
namespace v {
    template <typename T>
    using Rc = v::mem::Rc<T>;

    template <typename T>
    using Arc = v::mem::Arc<T>;
} // namespace v
