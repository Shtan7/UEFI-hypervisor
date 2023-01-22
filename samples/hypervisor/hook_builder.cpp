#include "hook_builder.hpp"
#include <ranges>
#include "ept_handler.hpp"
#include "globals.hpp"
#include "invept.hpp"

namespace hh
{
  void hook_builder::perform_page_hook(hook::guest_hook_request_info& guest_info)
  {
    // page aligning is performed in guest mode before vmcall
    uint64_t target_phys_address = common::get_physical_address_for_virtual_address_by_cr3(guest_info.target_cr3, guest_info.target_page_address);
    uint64_t hooked_page_phys_address = common::get_physical_address_for_virtual_address_by_cr3(guest_info.target_cr3, guest_info.hooked_page_address);

    hook_information_[target_phys_address] = {};
    hook::hook_info& hook_info = hook_information_.find(target_phys_address)->second;

    // Split large page and get pml1 entry for more accuracy in hooking process.
    // We don't want to cause vmexit all times when someone want to access memory
    // in whole 2 mb region. We reduce this region to 4kb.
    globals::ept_handler->split_large_page(target_phys_address);
    ept::pml1_entry* target_pml1_entry = globals::ept_handler->get_pml1_entry(target_phys_address);
    ept::pml1_entry changed_entry = *target_pml1_entry;

    changed_entry.read_access = guest_info.required_attributes.read;
    changed_entry.write_access = guest_info.required_attributes.write;

    // We treat execute hook in special way.
    if (guest_info.required_attributes.exec)
    {
      changed_entry.read_access = 0;
      changed_entry.write_access = 0;
      changed_entry.execute_access = 1;
    }

    changed_entry.page_frame_number = hooked_page_phys_address >> common::page_shift;

    hook_info.changed_entry = changed_entry;
    hook_info.original_entry = *target_pml1_entry;
    hook_info.virtual_address = guest_info.target_page_address;
    hook_info.entry_address = target_pml1_entry;

    globals::ept_handler->set_pml1_and_invalidate_tlb(target_pml1_entry, changed_entry,
      vmx::invvpid_type::invvpid_individual_address);
  }

  void hook_builder::unhook_page(uint64_t target_phys_address)
  {
    hook::hook_info& info_entry = hook_information_.at(target_phys_address);

    globals::ept_handler->set_pml1_and_invalidate_tlb(info_entry.entry_address, info_entry.original_entry,
      vmx::invvpid_type::invvpid_individual_address);

    hook_information_.erase(target_phys_address);
  }

  void hook_builder::unhook_all_pages() noexcept
  {
    for (const auto& hook_info : hook_information_ | std::views::values)
    {
      globals::ept_handler->set_pml1_and_invalidate_tlb(hook_info.entry_address, hook_info.original_entry,
        vmx::invvpid_type::invvpid_individual_address);
    }

    hook_information_.clear();
  }

  hook::hook_info* hook_builder::get_hooked_page_info(uint64_t physical_address)
  {
    physical_address = reinterpret_cast<uint64_t>(PAGE_ALIGN(physical_address));
    return &hook_information_.at(physical_address);
  }
}
