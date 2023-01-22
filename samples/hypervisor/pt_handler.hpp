#pragma once
#include <list>
#include "pt.hpp"
#include <memory>
#include <vector>
#include "x86.hpp"

namespace hh::pt
{
  // Class handles root mode PT table related operations.
  class pt_handler : non_relocatable
  {
  public:

    // Auxiliary class for reading of guest VA.
    class memory_descriptor : non_copyable
    {
      friend class pt_handler;

    private:
      std::list<std::pair<void*, pt::pte_64*>> memory_region_;
      decltype(memory_region_.begin()) prev_it_;
      uint32_t prev_index_ = -1;
      uint32_t initial_page_offset_ = {};

    public:
      memory_descriptor() = default;
      memory_descriptor(memory_descriptor&&) = default;
      memory_descriptor& operator=(memory_descriptor&&) = default;

      uint8_t& operator[](uint64_t offset) noexcept;
      void memcpy(void* destination, uint64_t offset, size_t count) noexcept;
      void memset(uint64_t offset, uint8_t value, size_t count) noexcept;
      ~memory_descriptor();
    };

    friend class memory_descriptor;

  private:
    static constexpr uint64_t pml4_index_for_host_guest_mappings = 1;

  private:
    host_mapping_table* host_pt_table_;
    std::list<pte_64*> free_entries_;

  private:
    void* get_va_for_pt(pte_64* entry) const noexcept;

  public:
    pt_handler();
    ~pt_handler() noexcept;

    void initialize_pt();
    x86::cr3_t get_cr3() const noexcept;
    std::shared_ptr<memory_descriptor> map_guest_address(x86::cr3_t guest_cr3, uint8_t* virtual_address, size_t region_size);
  };
}
