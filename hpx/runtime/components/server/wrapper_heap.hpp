//  Copyright (c) 1998-2017 Hartmut Kaiser
//  Copyright (c)      2011 Bryce Lelbach
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#if !defined(HPX_UTIL_WRAPPER_HEAP_JUN_12_2008_0904AM)
#define HPX_UTIL_WRAPPER_HEAP_JUN_12_2008_0904AM

#include <hpx/config.hpp>
#include <hpx/lcos/local/spinlock.hpp>
#include <hpx/runtime/naming/name.hpp>
#include <hpx/runtime_fwd.hpp>
#include <hpx/util/assert.hpp>
#include <hpx/util/generate_unique_ids.hpp>
#include <hpx/util/itt_notify.hpp>
#include <hpx/util/wrapper_heap_base.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <type_traits>

#include <hpx/config/warnings_prefix.hpp>

#define HPX_DEBUG_WRAPPER_HEAP 1

///////////////////////////////////////////////////////////////////////////////
namespace hpx { namespace components { namespace detail
{
#if HPX_DEBUG_WRAPPER_HEAP != 0
#  define HPX_WRAPPER_HEAP_INITIALIZED_MEMORY 1
#else
#  define HPX_WRAPPER_HEAP_INITIALIZED_MEMORY 0
#endif

    ///////////////////////////////////////////////////////////////////////////
    namespace one_size_heap_allocators
    {
        ///////////////////////////////////////////////////////////////////////
        // TODO: this interface should conform to the Boost.Pool allocator
        // interface, to maximize code reuse and consistency - wash.
        //
        // simple allocator which gets the memory from the default malloc,
        // but which does not reallocate the heap (it doesn't grow)
        struct fixed_mallocator
        {
            static void* alloc(std::size_t size)
            {
                return ::malloc(size);
            }
            static void free(void* p)
            {
                ::free(p);
            }
            static void* realloc(std::size_t &, void *)
            {
                // normally this should return ::realloc(p, size), but we are
                // not interested in growing the allocated heaps, so we just
                // return nullptr
                return nullptr;
            }
        };
    }

    ///////////////////////////////////////////////////////////////////////////
    class HPX_EXPORT wrapper_heap : public util::wrapper_heap_base
    {
    public:
        HPX_NON_COPYABLE(wrapper_heap);

    public:
        typedef one_size_heap_allocators::fixed_mallocator allocator_type;
        typedef hpx::lcos::local::spinlock mutex_type;
        typedef std::unique_lock<mutex_type> scoped_lock;

#if HPX_DEBUG_WRAPPER_HEAP != 0
        enum guard_value
        {
            initial_value = 0xcc,           // memory has been initialized
            freed_value = 0xdd,             // memory has been freed
        };
#endif

    public:
        explicit wrapper_heap(char const* class_name, std::size_t count,
            std::size_t heap_size, std::size_t step);

        wrapper_heap();
        ~wrapper_heap() override;

        std::size_t size() const override;
        std::size_t free_size() const override;

        bool is_empty() const;
        bool has_allocatable_slots() const;

        bool alloc(void** result, std::size_t count = 1) override;
        void free(void *p, std::size_t count = 1) override;
        bool did_alloc (void *p) const override;

        // Get the global id of the managed_component instance given by the
        // parameter p.
        //
        // The pointer given by the parameter p must have been allocated by
        // this instance of a wrapper_heap
        naming::gid_type get_gid(util::unique_id_ranges& ids, void* p,
            components::component_type type) override;

        void set_gid(naming::gid_type const& g);

    protected:
        bool test_release(scoped_lock& lk);
        bool ensure_pool(std::size_t count);

        bool init_pool();
        void tidy();

    protected:
        void* pool_;
        void* first_free_;
        std::size_t step_;
        std::size_t size_;
        std::size_t free_size_;

        // these values are used for AGAS registration of all elements of this
        // managed_component heap
        naming::gid_type base_gid_;

        mutable mutex_type mtx_;

    public:
        std::string const class_name_;
#if defined(HPX_DEBUG)
        std::size_t alloc_count_;
        std::size_t free_count_;
        std::size_t heap_count_;
#endif

        std::size_t const element_size_;  // size of one element in the heap

        std::size_t heap_count() const override { return heap_count_; }

    private:
        util::itt::heap_function heap_alloc_function_;
        util::itt::heap_function heap_free_function_;
    };

    ///////////////////////////////////////////////////////////////////////////
    // heap using malloc and friends
    template <typename T>
    class fixed_wrapper_heap : public wrapper_heap
    {
    private:
        typedef wrapper_heap base_type;

    public:
        typedef T value_type;

        explicit fixed_wrapper_heap(char const* class_name,
                std::size_t count, std::size_t step, std::size_t element_size)
          : base_type(class_name, count, step, element_size)
        {}

        naming::address get_address()
        {
            util::itt::heap_internal_access hia; HPX_UNUSED(hia);

            T* addr = static_cast<T*>(pool_->address());
            return naming::address(get_locality(),
                 components::get_component_type<typename T::type_holder>(),
                 addr);
        }
    };
}}} // namespace hpx::components::detail

#include <hpx/config/warnings_suffix.hpp>

#undef HPX_WRAPPER_HEAP_INITIALIZED_MEMORY

#endif
