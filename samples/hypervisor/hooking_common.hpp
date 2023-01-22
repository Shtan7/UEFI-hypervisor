#pragma once
#include "common.hpp"
#include "ept.hpp"

namespace hh::hook
{
  // What access bits should be in EPT entry.
  union page_attribs
  {
    struct
    {
      uint8_t read : 1;
      uint8_t write : 1;
      uint8_t exec : 1;
    };

    uint8_t all;
  };

  struct guest_hook_request_info
  {
    // guest VA address of target page
    void* target_page_address;

    // guest VA address of page with changed content
    void* hooked_page_address;

    // from which cr3 we should compute physical address
    x86::cr3_t target_cr3;
    page_attribs required_attributes;
  };

  struct hook_info
  {
    // VA from guest cr3 perspective. Address is page aligned
    void* virtual_address;

    // The page entry in the page tables that this page is targetting.
    ept::pml1_entry* entry_address;

    // The original page entry. Will be copied back when the hook is removed from the page.
    ept::pml1_entry original_entry;

    // Hooked verison of entry
    ept::pml1_entry changed_entry;
  };
}
