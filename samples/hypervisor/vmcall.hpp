#pragma once
#pragma once
#include <cstdint>
#include "common.hpp"

namespace hh
{
  namespace vmx
  {
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
      panic,
    };

    struct invept_context { uint64_t phys_address; };

    extern "C" common::status __vmcall(vmcall_number call_num, uint64_t optional_param_1, uint64_t optional_param_2, uint64_t optional_param_3);

    template<class T> inline common::status vmcall(T) { __int2c(); }

    template <> inline common::status vmcall(vmcall_number request) noexcept
    {
      return __vmcall(request, 0, 0, 0);
    }
  }
}
