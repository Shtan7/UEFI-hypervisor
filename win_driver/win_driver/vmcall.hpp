#pragma once
#include <cstdint>


namespace hh
{
  enum class status
  {
    hv_success,
    hv_unsuccessful,
  };

  enum class vmcall_number : uint64_t
  {
    test = 0x1,
    change_page_attrib,
    invept_all_contexts,
    invept_single_context,
    unhook_all_pages,
    unhook_single_page,
    get_win_driver_pool_address,
    get_ntoskrnl_base_address,
    get_win_driver_pool_size,
    get_physical_address_for_virtual,
    notify_all_to_invalidate_ept,
  };

  extern "C" status __vmcall(vmcall_number vmcall_number, uint64_t arg1 = 0, uint64_t arg2 = 0, uint64_t arg3 = 0);
  extern "C" uint64_t __vmcall_with_returned_value(vmcall_number vmcall_number, uint64_t arg1 = 0, uint64_t arg2 = 0, uint64_t arg3 = 0);
}
