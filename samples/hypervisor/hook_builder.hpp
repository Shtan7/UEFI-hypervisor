#pragma once
#include <map>
#include "hooking_common.hpp"

namespace hh
{
  // Class handles EPT hook related operations in root mode.
  class hook_builder : non_relocatable
  {
  private:
    std::map<uint64_t, hook::hook_info> hook_information_;

  public:
    void perform_page_hook(hook::guest_hook_request_info& guest_info);
    void unhook_page(uint64_t target_phys_address);
    void unhook_all_pages() noexcept;
    hook::hook_info* get_hooked_page_info(uint64_t physical_address);
  };
}
