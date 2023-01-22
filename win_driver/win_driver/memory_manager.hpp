#pragma once
#include <ntddk.h>
#include "../../samples/hypervisor/delete_constructors.hpp"
#include "common.hpp"
#include "tlsf.h"
#include <vcruntime_new.h>

namespace hh
{
  class memory_manager abstract : non_relocatable
  {
  protected:
    volatile long spinlock_ = {};

  public:
    memory_manager() = default;
    virtual void* allocate(uint32_t allocation_size) = 0;
    virtual void* allocate_align(uint32_t allocation_size, std::align_val_t align) = 0;
    virtual void deallocate(void* ptr_to_allocation) = 0;
    virtual ~memory_manager() = default;
  };

  // tlsf allocator is a fast allocator with constant allocation time
  template<unsigned int DefaultSize = 0x1000 * 1525>
  class tlsf_allocator : public memory_manager
  {
  private:
    tlsf_t service_data_;
    size_t pool_size_;
    void* pool_ptr_;
    bool constructed_from_preallocated_buff_ = {};

  public:
    tlsf_allocator(void* ptr_to_pool, size_t size_of_pool) noexcept : service_data_{}, pool_size_{ size_of_pool }, pool_ptr_{ ptr_to_pool }
    {
      constructed_from_preallocated_buff_ = true;
      service_data_ = tlsf_create_with_pool(pool_ptr_, pool_size_);
    }

    tlsf_allocator() noexcept : service_data_{}, pool_size_{ DefaultSize }, pool_ptr_{}
    {
      pool_ptr_ = ExAllocatePoolWithTag(NonPagedPool, pool_size_, common::pool_tag);
      service_data_ = tlsf_create_with_pool(pool_ptr_, pool_size_);
    }

    tlsf_allocator(size_t pool_size) noexcept : service_data_{}, pool_size_{ pool_size }, pool_ptr_{}
    {
      pool_ptr_ = ExAllocatePoolWithTag(NonPagedPool, pool_size_, common::pool_tag);
      service_data_ = tlsf_create_with_pool(pool_ptr_, pool_size_);
    }

    void* allocate(uint32_t allocation_size) noexcept override
    {
      common::spinlock_guard _ = { &spinlock_ };
      return tlsf_malloc(service_data_, allocation_size);
    }

    void* allocate_align(uint32_t allocation_size, std::align_val_t align) noexcept override
    {
      common::spinlock_guard _ = { &spinlock_ };
      return tlsf_memalign(service_data_, static_cast<size_t>(align), allocation_size);
    }

    void deallocate(void* ptr_to_allocation) noexcept override
    {
      common::spinlock_guard _ = { &spinlock_ };
      tlsf_free(service_data_, ptr_to_allocation);
    }

    ~tlsf_allocator() noexcept override
    {
      if(constructed_from_preallocated_buff_)
      {
        return;
      }

      ExFreePoolWithTag(pool_ptr_, common::pool_tag);
    }
  };
}
