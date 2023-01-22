#pragma once
#include "exception.hpp"
#include "vmx.hpp"

namespace hh
{
  // Struct represents interrupt info in root mode.
  struct interrupt
  {
    vmx::interrupt_info interrupt_info;
    exception_error_code exception_code;
    int rip_increment;

    constexpr interrupt() noexcept : interrupt_info{}, exception_code{}, rip_increment{}
    {}

    constexpr interrupt(vmx::interrupt_info info, exception_error_code error_code, int rip_adjust) noexcept
      : interrupt_info{ info }, exception_code{ error_code }, rip_increment{ rip_adjust }
    {}

    constexpr interrupt(const interrupt& other) noexcept = default;
    constexpr interrupt(interrupt&& other) noexcept = default;
    constexpr interrupt& operator=(const interrupt& other)  noexcept = default;
    constexpr interrupt& operator=(interrupt&& other) noexcept = default;
    constexpr ~interrupt() noexcept = default;

    constexpr exception_vector vector() const noexcept { return static_cast<exception_vector>(interrupt_info.vector); }
    constexpr vmx::interrupt_type type() const noexcept { return static_cast<vmx::interrupt_type>(interrupt_info.type); }
    constexpr uint32_t error_code_valid() const noexcept { return interrupt_info.error_code_valid; }
    constexpr uint32_t nmi_unblocking() const noexcept { return interrupt_info.nmi_unblocking; }
    constexpr uint32_t valid() const noexcept { return interrupt_info.valid; }
    constexpr exception_error_code error_code() const noexcept { return exception_code; }
    constexpr int rip_adjust() const noexcept { return rip_increment; }
    constexpr vmx::interrupt_info vmx_interrupt_info() const noexcept { return interrupt_info; }
  };

  // Some common interrupt templates that we often use.
  namespace interrupt_templates
  {
    consteval interrupt nmi() noexcept
    {
      interrupt nmi_interrupt = {};

      nmi_interrupt.interrupt_info.type = static_cast<uint32_t>(vmx::interrupt_type::nmi);
      nmi_interrupt.interrupt_info.vector = static_cast<uint32_t>(exception_vector::nmi_interrupt);
      nmi_interrupt.interrupt_info.valid = 1;

      return nmi_interrupt;
    }

    consteval interrupt divide_error() noexcept
    {
      interrupt divide_error = {};

      divide_error.interrupt_info.type = static_cast<uint32_t>(vmx::interrupt_type::hardware_exception);
      divide_error.interrupt_info.vector = static_cast<uint32_t>(exception_vector::divide_error);
      divide_error.interrupt_info.valid = 1;

      return divide_error;
    }

    consteval interrupt general_protection() noexcept
    {
      interrupt general_protection = {};

      general_protection.interrupt_info.type = static_cast<uint32_t>(vmx::interrupt_type::hardware_exception);
      general_protection.interrupt_info.vector = static_cast<uint32_t>(exception_vector::general_protection);
      general_protection.interrupt_info.error_code_valid = 1;
      general_protection.exception_code.flags = 0;
      general_protection.interrupt_info.valid = 1;

      return general_protection;
    }
  }
}
