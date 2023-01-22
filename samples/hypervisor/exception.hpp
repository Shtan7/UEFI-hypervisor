#pragma once
#include <cstdint>
#include "enum_to_str.hpp"

namespace hh
{
  enum class exception_vector : uint32_t
  {
    divide_error = 0,
    debug = 1,
    nmi_interrupt = 2,
    breakpoint = 3,
    overflow = 4,
    bound = 5,
    invalid_opcode = 6,
    device_not_available = 7,
    double_fault = 8,
    coprocessor_segment_overrun = 9,
    invalid_tss = 10,
    segment_not_present = 11,
    stack_segment_fault = 12,
    general_protection = 13,
    page_fault = 14,
    x87_floating_point_error = 16,
    alignment_check = 17,
    machine_check = 18,
    simd_floating_point_error = 19,
    virtualization_exception = 20,

    //
    // NT (Windows) specific exception vectors.
    //

    nt_apc_interrupt = 31,
    nt_dpc_interrupt = 47,
    nt_clock_interrupt = 209,
    nt_pmi_interrupt = 254,
  };

  struct pagefault_error_code
  {
    union
    {
      uint32_t flags;

      struct
      {
        uint32_t present : 1;
        uint32_t write : 1;
        uint32_t user_mode_access : 1;
        uint32_t reserved_bit_violation : 1;
        uint32_t execute : 1;
        uint32_t protection_key_violation : 1;
        uint32_t reserved_1 : 9;
        uint32_t sgx_access_violation : 1;
        uint32_t reserved_2 : 16;
      };
    };
  };

  struct exception_error_code
  {
    union
    {
      uint32_t flags;

      struct
      {
        uint32_t external_event : 1;
        uint32_t descriptor_location : 1;
        uint32_t table : 1;
        uint32_t index : 13;
        uint32_t reserved : 16;
      };

      pagefault_error_code pagefault;
    };
  };

  template<> inline std::map<exception_vector, std::string_view> enum_map<exception_vector> =
  {
    { exception_vector::divide_error, "divide_error" },
    { exception_vector::debug, "debug" },
    { exception_vector::nmi_interrupt, "nmi_interrupt" },
    { exception_vector::breakpoint, "breakpoint", },
    { exception_vector::overflow, "overflow" },
    { exception_vector::bound, "bound" },
    { exception_vector::invalid_opcode, "invalid_opcode" },
    { exception_vector::device_not_available, "device_not_available" },
    { exception_vector::double_fault, "double_fault" },
    { exception_vector::coprocessor_segment_overrun, "coprocessor_segment_overrun" },
    { exception_vector::invalid_tss, "invalid_tss" },
    { exception_vector::segment_not_present, "segment_not_present" },
    { exception_vector::stack_segment_fault, "stack_segment_fault" },
    { exception_vector::general_protection, "general_protection" },
    { exception_vector::page_fault, "page_fault" },
    { exception_vector::x87_floating_point_error, "x87_floating_point_error" },
    { exception_vector::alignment_check, "alignment_check" },
    { exception_vector::machine_check, "machine_check" },
    { exception_vector::simd_floating_point_error, "simd_floating_point_error" },
    { exception_vector::virtualization_exception, "virtualization_exception" },
    { exception_vector::nt_apc_interrupt, "nt_apc_interrupt" },
    { exception_vector::nt_dpc_interrupt, "nt_dpc_interrupt" },
    { exception_vector::nt_clock_interrupt, "nt_clock_interrupt" },
    { exception_vector::nt_pmi_interrupt, "nt_pmi_interrupt" }
  };
}
