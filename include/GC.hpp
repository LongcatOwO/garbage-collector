#ifndef _GC_HPP_INCLUDED_
#define _GC_HPP_INCLUDED_

#include <algorithm>
#include <compare>
#include <cstddef>
#include <functional>
#include <memory>
#include <type_traits>
#include <unordered_set>
#include <unordered_map>
#include <utility>

// forward declaration
namespace GC
{
    namespace detail
    {
        template <typename T>
        struct always_false;

        struct PointerData;

        struct PointerOperationsBase;
        
        template <typename T, typename Deleter>
        struct PointerOperations;

        struct PtrBase;

        struct Operations;
    } // namespace detail

    template <typename T>
    struct Ptr;

    template <typename T>
    struct PtrTrait;
} // namespace GC
// end forward declaration




// declaration
namespace GC
{
    namespace detail
    {
        // stores the original pointer! not the offseted ones from static_casts
        // and implicit conversions
        inline std::unordered_map<void *, PointerData> nodes;
        inline std::unordered_set<PtrBase *> roots;
        // for checking if the current construction is root or not
        inline bool is_root = true;

        // returns u - t
        template <typename T, typename U>
        constexpr std::ptrdiff_t ptrDiff(T *t, U *u);

        auto registerRoot(PtrBase *) -> void;
        auto removeRoot(PtrBase *) -> void;
        
        template <typename T, typename Deleter = std::default_delete<T>>
        auto registerNode(T *ptr, Deleter d = {}) -> PointerData *;

        template <typename T>
        struct always_false : std::false_type {};

        template <typename T>
        constexpr bool always_false_v = always_false<T>::value;

        struct PointerData
        {
            void *ptr;
            bool visited;
            std::unique_ptr<PointerOperationsBase> operations;

            PointerData(void *ptr, std::unique_ptr<PointerOperationsBase> &&operations);
            auto visit() -> void;
        }; // struct PointerDataBase

        struct PointerOperationsBase
        {
            virtual auto visit(void *self) -> void = 0;
            virtual auto deletePointer(void *ptr) -> void = 0;
            virtual ~PointerOperationsBase() {}
        }; // struct PoitnerOperationsBase

        template <typename T, typename Deleter = std::default_delete<T>>
        struct PointerOperations : PointerOperationsBase
        {
            PointerOperations() {}
            PointerOperations(Deleter d);
            auto visit(void *self) -> void override;
            auto deletePointer(void *ptr) -> void override;
        private:
            Deleter _deleter;
        }; // struct PointerData

    } // namespace detail

    template <typename T, typename U>
    concept Visitor = requires (T t, U u)
    {
        t(u);
    };

    template <typename T>
    concept PtrVisitor = Visitor<T, detail::PtrBase &>;

    using PtrVisitorFunc = std::function<void (detail::PtrBase &)>;
    static_assert(PtrVisitor<PtrVisitorFunc>);

    namespace detail
    {
        struct PtrBase
        {
            friend struct Operations;

            PtrBase(PointerData *ptr_data);
            virtual ~PtrBase() {}

        protected:
            PointerData *_ptr_data;

        private:
            template <PtrVisitor T>
            auto accept(T &&visitor) -> void;
        }; // struct PtrBase

        struct Operations
        {
            static auto collect() -> void;
            static auto getPointerData(PtrBase * ptr) -> PointerData *;
        }; // struct Operations
    } // namespace detail

    inline auto collect() -> void { detail::Operations::collect(); }

    template <typename T>
    struct Ptr : detail::PtrBase
    {
        template <typename ...Args>
        static auto make(Args &&...args) -> Ptr;

        Ptr();
        Ptr(std::nullptr_t);

        template <typename U>
        explicit Ptr(U *ptr);

        Ptr(Ptr const &r);
        
        template <typename U>
        Ptr(Ptr<U> const &r);

        Ptr(Ptr &&r);

        template <typename U>
        Ptr(Ptr<U> &&r);

        template <typename U, typename Deleter>
        Ptr(std::unique_ptr<U, Deleter> &&r);

        ~Ptr();

        auto operator=(std::nullptr_t) -> Ptr&;

        auto operator=(Ptr const &r) -> Ptr&;

        template <typename U>
        auto operator=(Ptr<U> const &r) -> Ptr&;

        auto operator=(Ptr &&r) -> Ptr&;

        template <typename U>
        auto operator=(Ptr<U> &&r) -> Ptr&;

        template <typename U, typename Deleter>
        auto operator=(std::unique_ptr<U, Deleter> &&r) -> Ptr&;

        auto reset() -> void;

        template <typename U>
        auto reset(U *ptr) -> void;

        template <typename U, typename Deleter>
        auto reset(U *ptr, Deleter d) -> void;

        template <typename U, typename Deleter, typename Alloc>
        auto reset(U *ptr, Deleter d, Alloc alloc) -> void;

        auto swap(Ptr &r) -> void;

        auto get() const -> T *;

        auto operator*() const -> T &;
        auto operator->() const -> T *;

        explicit operator bool() const;

    private:
        using base = detail::PtrBase;

        T *_ptr;
    }; // struct Ptr

    template <typename T, typename D>
    Ptr(std::unique_ptr<T, D>) -> Ptr<T>;


    template <typename T>
    concept UntraversablePrimitive = std::is_fundamental_v<T> || std::is_pointer_v<T>;

    template <typename T>
    struct PtrTrait
    {
        static_assert(detail::always_false_v<T>, "PtrTrait<T> not implemented for type T");
        template <PtrVisitor V>
        static auto visit(T *self, V &&visitor) -> void;
    };
    
    template <UntraversablePrimitive T>
    struct PtrTrait<T>
    {
        template <PtrVisitor V>
        static auto visit(T *, V &&) -> void {}
    };


} // namespace GC
// end delcaration




// implementation
namespace GC
{
    namespace detail
    {
        inline auto registerRoot(PtrBase *root) -> void
        {
            if (is_root)
                roots.insert(root);
        }

        inline auto removeRoot(PtrBase *root) -> void
        {
            roots.erase(root);
        }

        template <typename T, typename Deleter>
        auto registerNode(T *ptr, Deleter d) -> PointerData *
        {
            PtrTrait<T>::visit
                (ptr, 
                 [](PtrBase &ptr) { removeRoot(&ptr); });
            return &nodes.insert({
                ptr, 
                PointerData
                (ptr, std::make_unique<PointerOperations<T, Deleter>>(d))
            }).first->second;
        }

        inline PointerData::PointerData
            (void *ptr, std::unique_ptr<PointerOperationsBase> &&ops)
            : ptr(ptr), operations(std::move(ops)) {}

        inline auto PointerData::visit() -> void
        {
            if (visited) return;
            visited = true;
            operations->visit(ptr);
        }

        template <typename T, typename Deleter>
        PointerOperations<T, Deleter>::PointerOperations(Deleter d) : _deleter(d) {}

        template <typename T, typename Deleter>
        auto PointerOperations<T, Deleter>::visit(void *self) -> void
        {
            PtrTrait<T>::visit
                (reinterpret_cast<T *>(self),
                 [](PtrBase &ptr) 
                 { 
                    PointerData *ptr_data = Operations::getPointerData(&ptr);
                    if (ptr_data) ptr_data->visit();
                 });
        }

        template <typename T, typename Deleter>
        auto PointerOperations<T, Deleter>::deletePointer(void *ptr) -> void
        {
            _deleter(reinterpret_cast<T *>(ptr));
        }

        inline PtrBase::PtrBase(PointerData *ptr_data) : _ptr_data(ptr_data) {}

        template <PtrVisitor T>
        auto PtrBase::accept(T &&visitor) -> void
        {
            visitor(*this);
        }

        inline auto Operations::collect() -> void
        {
            bool collect = true;
            while (collect)
            {
                collect = false;

                // reset marks
                std::for_each
                    (nodes.begin(), nodes.end(), 
                     [](decltype(nodes)::value_type &pair) 
                     { 
                        pair.second.visited = false;
                     });

                // mark
                std::for_each
                    (roots.begin(), roots.end(),
                    [](PtrBase *root) 
                    { 
                        if (root->_ptr_data) root->_ptr_data->visit();
                    });

                for (auto it = nodes.begin(); it != nodes.end();)
                {
                    if (!it->second.visited)
                    {
                        // not visited, can be collected
                        it->second.operations->deletePointer(it->first);
                        it = nodes.erase(it);
                        collect = true;
                    }
                    else
                    {
                        ++it;
                    }
                }
            }
        }

        inline auto Operations::getPointerData(PtrBase *ptr) -> PointerData *
        {
            return ptr->_ptr_data;
        }
    }

    template <typename T> template <typename ...Args>
    auto Ptr<T>::make(Args &&...args) -> Ptr
    {
        bool old_is_root = detail::is_root;
        // set to false, so all Ptr() calls inside T() will be treated as non root
        detail::is_root = false;
        T *t = new T(std::forward<Args>(args)...);
        // set back to original value
        detail::is_root = old_is_root;

        return Ptr(t);
    }

    template <typename T>
    Ptr<T>::Ptr() : base(nullptr), _ptr(nullptr)
    {
        detail::registerRoot(this);
    }

    template <typename T>
    Ptr<T>::Ptr(std::nullptr_t) : base(nullptr), _ptr(nullptr)
    {
        detail::registerRoot(this);
    }

    template <typename T> template <typename U>
    Ptr<T>::Ptr(U *ptr) : base(nullptr), _ptr(ptr)
    {
        detail::registerRoot(this);
        if (ptr)
        {
            _ptr_data = detail::registerNode(ptr);
        }
    }

    template <typename T>
    Ptr<T>::Ptr(Ptr const &r) : base(r._ptr_data), _ptr(r._ptr)
    {
        detail::registerRoot(this);
    }

    template <typename T> template <typename U>
    Ptr<T>::Ptr(Ptr<U> const &r) : base(r._ptr_data), _ptr(r._ptr)
    {
        detail::registerRoot(this);
    }

    template <typename T>
    Ptr<T>::Ptr(Ptr &&r) : base(r._ptr_data), _ptr(r._ptr)
    {
        detail::registerRoot(this);
        r._ptr = nullptr;
        r._ptr_data = nullptr;
    }

    template <typename T> template <typename U>
    Ptr<T>::Ptr(Ptr<U> &&r) : base(r._ptr_data), _ptr(r._ptr)
    {
        detail::registerRoot(this);
        r._ptr = nullptr;
        r._ptr_data = nullptr;
    }

    template <typename T> template <typename U, typename Deleter>
    Ptr<T>::Ptr(std::unique_ptr<U, Deleter> &&r) : base(nullptr)
    {
        detail::registerRoot(this);
        U *ptr = r.release();
        _ptr = ptr;
        if (ptr)
        {
            _ptr_data = detail::registerNode(ptr, r.get_deleter());
        }
    }

    template <typename T>
    Ptr<T>::~Ptr()
    {
        detail::removeRoot(this);
    }

    template <typename T>
    auto Ptr<T>::operator=(std::nullptr_t) -> Ptr&
    {
        _ptr = nullptr;
        _ptr_data = nullptr;
        return *this;
    }

    template <typename T>
    auto Ptr<T>::operator=(Ptr const &r) -> Ptr&
    {
        _ptr = r._ptr;
        _ptr_data = r._ptr_data;
        return *this;
    }

    template <typename T> template <typename U>
    auto Ptr<T>::operator=(Ptr<U> const &r) -> Ptr&
    {
        _ptr = r._ptr;
        _ptr_data = r._ptr_data;
        return *this;
    }

    template <typename T>
    auto Ptr<T>::operator=(Ptr &&r) -> Ptr&
    {
        _ptr = r._ptr;
        _ptr_data = r._ptr_data;
        r._ptr = nullptr;
        r._ptr_data = nullptr;
        return *this;
    }

    template <typename T> template <typename U>
    auto Ptr<T>::operator=(Ptr<U> &&r) -> Ptr&
    {
        _ptr = r._ptr;
        _ptr_data = r._ptr_data;
        r._ptr = nullptr;
        r._ptr_data = nullptr;
        return *this;
    }

    template <typename T> template <typename U, typename Deleter>
    auto Ptr<T>::operator=(std::unique_ptr<U, Deleter> &&r) -> Ptr&
    {
        U *ptr = r.release();
        _ptr = ptr;
        if (ptr)
        {
            _ptr_data = detail::registerNode(ptr, r.get_deleter());
        }
        else
        {
            _ptr_data = nullptr;
        }
        return *this;
    }

    template <typename T>
    auto Ptr<T>::reset() -> void
    {
        _ptr = nullptr;
        _ptr_data = nullptr;
    }

    template <typename T> template <typename U>
    auto Ptr<T>::reset(U *ptr) -> void
    {
        _ptr = ptr;
        if (ptr)
        {
            _ptr_data = detail::registerNode(ptr);
        }
        else
        {
            _ptr_data = nullptr;
        }
    }

    template <typename T> template <typename U, typename Deleter>
    auto Ptr<T>::reset(U *ptr, Deleter d) -> void
    {
        _ptr = ptr;
        if (ptr)
        {
            _ptr_data = detail::registerNode(ptr, d);
        }
        else
        {
            _ptr_data = nullptr;
        }
    }

    template <typename T> template <typename U, typename Deleter, typename Alloc>
    auto Ptr<T>::reset(U *ptr, Deleter d, Alloc alloc) -> void
    {
        static_assert(detail::always_false_v<T>, "Not implemented");
    }

    template <typename T>
    auto Ptr<T>::swap(Ptr<T> &r) -> void
    {
        std::swap(_ptr, r._ptr);
        std::swap(_ptr_data, r._ptr_data);
    }

    template <typename T>
    auto Ptr<T>::get() const -> T *
    {
        return _ptr;
    }

    template <typename T>
    auto Ptr<T>::operator*() const -> T &
    {
        return *_ptr;
    }

    template <typename T>
    auto Ptr<T>::operator->() const -> T *
    {
        return _ptr;
    }

    template <typename T>
    Ptr<T>::operator bool() const
    {
        return _ptr;
    }
} // namespace GC
// end implementation

#endif
