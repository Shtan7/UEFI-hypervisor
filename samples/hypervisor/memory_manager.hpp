#pragma once
#include "delete_constructors.hpp"
#include "uefi.hpp"
#include "common.hpp"
#include "tlsf.h"
#include "globals.hpp"

namespace hh
{
  // Heap manager interface.
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

  // Allocator with constant time allocation and deallocation. It fits perfectly for root mode allocations.
  template<unsigned int DefaultSize = common::page_size * 15000>
  class tlsf_allocator : public memory_manager
  {
  private:
    tlsf_t service_data_;
    size_t pool_size_;
    void* pool_ptr_;

  public:
    tlsf_allocator() : service_data_{}, pool_size_{ DefaultSize }, pool_ptr_{}
    {
      auto result = gBS->AllocatePool(EfiRuntimeServicesData, pool_size_, &pool_ptr_);

      if (EFI_ERROR(result))
      {
        throw std::exception{ __FUNCTION__": ""Failed to allocate pool for memory manager." };
      }

      service_data_ = tlsf_create_with_pool(pool_ptr_, pool_size_);
    }

    tlsf_allocator(size_t pool_size) : service_data_{}, pool_size_{ pool_size }, pool_ptr_{}
    {
      auto result = gBS->AllocatePool(EfiRuntimeServicesData, pool_size_, &pool_ptr_);

      if (EFI_ERROR(result))
      {
        throw std::exception{ __FUNCTION__": ""Failed to allocate pool for memory manager." };
      }

      service_data_ = tlsf_create_with_pool(pool_ptr_, pool_size_);
    }

    void* allocate(uint32_t allocation_size) noexcept override
    {
      common::spinlock_guard _{ &spinlock_ };
      return tlsf_malloc(service_data_, allocation_size);
    }

    void* allocate_align(uint32_t allocation_size, std::align_val_t align) noexcept override
    {
      common::spinlock_guard _{ &spinlock_ };
      return tlsf_memalign(service_data_, static_cast<size_t>(align), allocation_size);
    }

    void deallocate(void* ptr_to_allocation) noexcept override
    {
      common::spinlock_guard _{ &spinlock_ };
      tlsf_free(service_data_, ptr_to_allocation);
    }

    ~tlsf_allocator() noexcept override
    {
      if (globals::boot_state)
      {
        gBS->FreePool(pool_ptr_);
      }
    }
  };
}
