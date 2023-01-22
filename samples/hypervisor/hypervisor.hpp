#pragma once
namespace hh::hv_operations
{
  // Check MSR bits that signals about vmx support.
  void is_vmx_supported();

  // Hypervisor initialization starts here.
  void initialize_hypervisor();

  // We need our own IDT becase we can receive NMI while we are in root mode.
  void initialize_host_idt() noexcept;
}
