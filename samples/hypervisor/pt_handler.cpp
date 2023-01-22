#include "pt_handler.hpp"
#include <ranges>
#include "globals.hpp"
#include "x86.hpp"

namespace hh::pt
{
  // Compute address and return reference to it.
  uint8_t& pt_handler::memory_descriptor::operator[](uint64_t offset) noexcept
  {
    offset += initial_page_offset_;
    uint32_t offset_to_page = offset % common::page_size;
    uint8_t* ptr;

    if (uint32_t new_index = offset / common::page_size; prev_index_ != new_index)
    {
      prev_index_ = new_index;
      prev_it_ = std::next(memory_region_.begin(), prev_index_);
      ptr = static_cast<uint8_t*>(prev_it_->first);
    }
    else
    {
      ptr = static_cast<uint8_t*>(prev_it_->first);
    }

    return ptr[offset_to_page];
  }

  // Convenient way to copy content from guest VA
  void pt_handler::memory_descriptor::memcpy(void* destination, uint64_t offset, size_t count) noexcept
  {
    for (size_t j = 0; j < count; j++)
    {
      *(static_cast<uint8_t*>(destination) + j) = (*this)[offset + j];
    }
  }

  // Convenient way to set content in guest VA
  void pt_handler::memory_descriptor::memset(uint64_t offset, uint8_t value, size_t count) noexcept
  {
    for (size_t j = 0; j < count; j++)
    {
      (*this)[offset + j] = value;
    }
  }

  pt_handler::memory_descriptor::~memory_descriptor()
  {
    for (const auto [ptr, pte] : memory_region_)
    {
      // Mark the page as "not present" when we don't need it anymore
      *pte = pte_64{};
      __invlpg(ptr);

      // Return page to pool.
      globals::pt_handler->free_entries_.push_back(pte);
    }
  }

  pt_handler::pt_handler()
  {
    host_pt_table_ = new (std::align_val_t{ common::page_size }) host_mapping_table{};
  }

  pt_handler::~pt_handler() noexcept
  {
    delete host_pt_table_;
  }

  void pt_handler::initialize_pt()
  {
    memset(host_pt_table_, 0, sizeof(host_mapping_table));

    // identity mapping for host state

    host_pt_table_->pml4[0].present = 1;
    host_pt_table_->pml4[0].write = 1;
    host_pt_table_->pml4[0].page_frame_number = common::virtual_address_to_physical_address(&host_pt_table_->pml3_large[0]) >> common::page_shift;

    pdpte_1gb_64 template_entry_pdpte_1gb_64 = {};
    template_entry_pdpte_1gb_64.present = 1;
    template_entry_pdpte_1gb_64.write = 1;
    template_entry_pdpte_1gb_64.large_page = 1;

    __stosq(reinterpret_cast<uint64_t*>(&host_pt_table_->pml3_large[0]), template_entry_pdpte_1gb_64.as_uint, pt::pt_pml3_large_count);

    for (size_t j = 0; j < pt_pml3_large_count; j++)
    {
      host_pt_table_->pml3_large[j].page_frame_number = j * common::size_1gb >> common::page_shift_1gb;
    }

    // prepare tables for mapping of guest addresses

    host_pt_table_->pml4[1].present = 1;
    host_pt_table_->pml4[1].write = 1;
    host_pt_table_->pml4[1].page_frame_number = common::virtual_address_to_physical_address(&host_pt_table_->pml3[0]) >> common::page_shift;

    pdpte_64 template_pdpte_64 = {};
    template_pdpte_64.present = 1;
    template_pdpte_64.write = 1;

    __stosq(reinterpret_cast<uint64_t*>(&host_pt_table_->pml3[0]), template_pdpte_64.as_uint, pt_pml3_count);

    for (size_t j = 0; j < pt_pml3_count; j++)
    {
      host_pt_table_->pml3[j].page_frame_number = common::virtual_address_to_physical_address(&host_pt_table_->pml2[j][0]) >> common::page_shift;
    }

    pde_64 template_pde_64 = {};
    template_pde_64.present = 1;
    template_pde_64.write = 1;

    __stosq(reinterpret_cast<uint64_t*>(&host_pt_table_->pml2[0][0]), template_pde_64.as_uint, pt_pml3_count * pt_pml2_count);

    for (size_t j = 0; j < pt_pml3_count; j++)
    {
      for (size_t i = 0; i < pt_pml2_count; i++)
      {
        host_pt_table_->pml2[j][i].page_frame_number = common::virtual_address_to_physical_address(&host_pt_table_->pml1[j][i][0]) >> common::page_shift;
      }
    }

    for (auto& pml3_cell : host_pt_table_->pml1)
    {
      for (auto& pml2_cell : pml3_cell)
      {
        for (auto& pml1_ref : pml2_cell)
        {
          free_entries_.push_back(&pml1_ref);
        }
      }
    }
  }

  x86::cr3_t pt_handler::get_cr3() const noexcept
  {
    x86::cr3_t result = {};
    result.flags.page_frame_number = common::virtual_address_to_physical_address(&host_pt_table_->pml4[0]) >> common::page_shift;

    return result;
  }

  // Get correct host VA for mapped memory.
  void* pt_handler::get_va_for_pt(pte_64* entry) const noexcept
  {
    auto entry_address = reinterpret_cast<uint64_t>(entry);
    auto array_begin = reinterpret_cast<uint64_t>(&host_pt_table_->pml1[0][0][0]);
    uint64_t entry_index_in_single_dimension = (entry_address - array_begin) / sizeof(pte_64);

    common::virtual_address result = {};
    result.pml4_index = pml4_index_for_host_guest_mappings;
    result.pdpt_index = entry_index_in_single_dimension / (pt_pml2_count * pt_pml1_count);
    result.pd_index = entry_index_in_single_dimension / pt_pml1_count;
    result.pt_index = entry_index_in_single_dimension % pt_pml1_count;

    return reinterpret_cast<void*>(result.all);
  }

  // Map guest address and return wrapper for operations related to this memory.
  std::shared_ptr<pt_handler::memory_descriptor> pt_handler::map_guest_address(x86::cr3_t guest_cr3,
    uint8_t* virtual_address, size_t region_size)
  {
    std::shared_ptr<memory_descriptor> result = std::make_shared<memory_descriptor>();
    size_t number_of_entries = region_size / common::page_size + (region_size % common::page_size ? 1 : 0);
    result->initial_page_offset_ = reinterpret_cast<uint64_t>(virtual_address) & common::page_4kb_offset_mask;

    if (number_of_entries == 0)
    {
      throw std::exception{ __FUNCTION__": ""number_of_entries == 0." };
    }

    for (size_t j = 0; j < number_of_entries; j++)
    {
      uint64_t physical_address = common::get_physical_address_for_virtual_address_by_cr3(guest_cr3, virtual_address + j * common::page_size);
      pte_64* pt_entry = free_entries_.back(); free_entries_.pop_back();

      pt_entry->page_frame_number = physical_address >> common::page_shift;
      pt_entry->present = 1;
      pt_entry->write = 1;

      void* ptr_to_mapped_memory = get_va_for_pt(pt_entry);
      // Don't forget to invalidate TLB entry for page.
      __invlpg(ptr_to_mapped_memory);

      result->memory_region_.emplace_back(ptr_to_mapped_memory, pt_entry);
    }

    return result;
  }
}
