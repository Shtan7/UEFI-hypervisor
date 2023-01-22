#pragma once
#include <cstdint>
#include "../../samples/hypervisor/delete_constructors.hpp"
#include "../../samples/hypervisor/x86.hpp"

#if HH_DEBUG

#define PRINT(_a_) common::print_formatted _a_

#else

#define PRINT(_a_)

#endif


namespace hh::common
{
  inline constexpr uint32_t pool_tag = 'cimf';
  inline constexpr uint32_t max_trampoline_size = 120;

  inline constexpr uint32_t page_size = 0x1000;
  inline constexpr uint64_t size_2mb = 512 * common::page_size;
  inline constexpr uint64_t size_1gb = size_2mb * 512;
  inline constexpr uint32_t page_shift = 12;
  inline constexpr uint32_t page_shift_2mb = 21;
  inline constexpr uint32_t page_shift_1gb = 30;
  inline constexpr uint64_t page_1gb_offset_mask = (~0ull) >> (64 - page_shift_1gb);
  inline constexpr uint64_t page_2mb_offset_mask = (~0ull) >> (64 - page_shift_2mb);
  inline constexpr uint64_t page_4kb_offset_mask = (~0ull) >> (64 - page_shift);

  union virtual_address
  {
    struct
    {
      uint64_t page_offset : 12;
      uint64_t pt_index : 9;
      uint64_t pd_index : 9;
      uint64_t pdpt_index : 9;
      uint64_t pml4_index : 9;
      uint64_t unused : 16;
    };

    uint64_t all;
  };

  // RAII spinlock.
  class spinlock_guard : non_relocatable
  {
  private:
    static constexpr uint32_t max_wait_ = 65536;
    volatile long* lock_;

  private:
    bool try_lock() noexcept;
    void lock_spinlock() noexcept;
    void unlock() noexcept;

  public:
    spinlock_guard(volatile long* lock) noexcept;
    ~spinlock_guard() noexcept;
  };

  // RAII cr3 guard.
  class directory_base_guard : non_relocatable
  {
  private:
    x86::cr3_t old_cr3_;

  public:
    directory_base_guard(x86::cr3_t new_cr3) noexcept;
    ~directory_base_guard() noexcept;
  };

  // I don't shure, but maybe I broke something inside this print because serial text output is a little bit messy
  void print_formatted(const char* text, ...) noexcept;

  // you can't use functions below at boot stage ( right after image is mapped )
  uint64_t virtual_address_to_physical_address(void* virtual_address) noexcept;
  uint64_t physical_address_to_virtual_address(uint64_t physical_address) noexcept;
  uint64_t get_physical_address_for_virtual_address_by_cr3(x86::cr3_t guest_cr3, void* virtual_address_guest);
}
