#pragma once
#include <cstdint>
#include <map>
#include <string_view>
#include "enum_to_str.hpp"

namespace hh
{
  enum class bug_check_codes
  {
    kmode_exception_not_handled,
    invalid_cruntime_parameter,
    install_more_memory,
    overran_stack_buffer,
    allocator_out_of_memory,
    unexpected_exception_or_interrupt,
    vmx_error,
    boot_services_unavailable,
    corrupted_machine_state,
    corrupted_pe_header,
    no_matching_exception_handler,
    unwinding_non_cxx_frame,
    corrupted_eh_unwind_data,
    corrupted_exception_handler,
  };

  template<> inline std::map<bug_check_codes, std::string_view> enum_map<bug_check_codes> =
  {
    { bug_check_codes::allocator_out_of_memory, "allocator_out_of_memory" },
    { bug_check_codes::install_more_memory, "install_more_memory" },
    { bug_check_codes::invalid_cruntime_parameter, "invalid_cruntime_parameter" },
    { bug_check_codes::kmode_exception_not_handled, "kmode_exception_not_handled" },
    { bug_check_codes::overran_stack_buffer, "overran_stack_buffer" },
    { bug_check_codes::unexpected_exception_or_interrupt, "unexpected_exception_or_interrupt" },
    { bug_check_codes::vmx_error, "vmx_error" },
    { bug_check_codes::boot_services_unavailable, "BOOT_SERVICES_UNAVAILABLE" },
    { bug_check_codes::corrupted_machine_state, "corrupted_machine_state" },
    { bug_check_codes::corrupted_pe_header, "corrupted_pe_header" },
    { bug_check_codes::corrupted_pe_header, "corrupted_pe_header" },
    { bug_check_codes::no_matching_exception_handler, "no_matching_exception_handler" },
    { bug_check_codes::unwinding_non_cxx_frame, "unwinding_non_cxx_frame" },
    { bug_check_codes::corrupted_eh_unwind_data, "corrupted_eh_unwind_data" },
    { bug_check_codes::corrupted_exception_handler, "corrupted_exception_handler" },
  };

  void bug_check(const bug_check_codes code, const uint64_t arg0 = 0, const uint64_t arg1 = 0,
    const uint64_t arg2 = 0, const uint64_t arg3 = 0) noexcept;
}
