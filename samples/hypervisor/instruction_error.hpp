#pragma once
#include <cstdint>

namespace hh
{
  namespace vmx
  {
    enum class instruction_error : uint32_t
    {
      no_error = 0,
      vmcall_in_vmx_root_operation = 1,
      vmclear_invalid_physical_address = 2,
      vmclear_invalid_vmxon_pointer = 3,
      vmlauch_non_clear_vmcs = 4,
      vmresume_non_launched_vmcs = 5,
      vmresume_after_vmxoff = 6,
      vmentry_invalid_control_fields = 7,
      vmentry_invalid_host_state = 8,
      vmptrld_invalid_physical_address = 9,
      vmptrld_vmxon_pointer = 10,
      vmptrld_incorrect_vmcs_revision_id = 11,
      vmread_vmwrite_invalid_component = 12,
      vmwrite_readonly_component = 13,
      reserved_1 = 14,
      vmxon_in_vmx_root_op = 15,
      vmentry_invalid_vmcs_executive_pointer = 16,
      vmentry_non_launched_executive_vmcs = 17,
      vmentry_executive_vmcs_ptr = 18,
      vmcall_non_clear_vmcs = 19,
      vmcall_invalid_vmexit_fields = 20,
      reserved_2 = 21,
      vmcall_invalid_mseg_revision_id = 22,
      vmxoff_dual_monitor = 23,
      vmcall_invalid_smm_monitor = 24,
      vmentry_invalid_vm_execution_control = 25,
      vmentry_mov_ss = 26,
      reserved_3 = 27,
      invept_invvpid_invalid_operand = 28,
    };
  }

  template<> inline std::map<vmx::instruction_error, std::string_view> enum_map<vmx::instruction_error> =
  {
      { vmx::instruction_error::no_error, "no_error" },
      { vmx::instruction_error::vmcall_in_vmx_root_operation, "vmcall_in_vmx_root_operation" },
      { vmx::instruction_error::vmclear_invalid_physical_address, "vmclear_invalid_physical_address" },
      { vmx::instruction_error::vmclear_invalid_vmxon_pointer, "vmclear_invalid_vmxon_pointer" },
      { vmx::instruction_error::vmlauch_non_clear_vmcs, "vmlauch_non_clear_vmcs" },
      { vmx::instruction_error::vmresume_non_launched_vmcs, "vmresume_non_launched_vmcs" },
      { vmx::instruction_error::vmresume_after_vmxoff, "vmresume_after_vmxoff" },
      { vmx::instruction_error::vmentry_invalid_control_fields, "vmentry_invalid_control_fields" },
      { vmx::instruction_error::vmentry_invalid_host_state, "vmentry_invalid_host_state" },
      { vmx::instruction_error::vmptrld_invalid_physical_address, "vmptrld_invalid_physical_address" },
      { vmx::instruction_error::vmptrld_vmxon_pointer, "vmptrld_vmxon_pointer" },
      { vmx::instruction_error::vmptrld_incorrect_vmcs_revision_id, "vmptrld_incorrect_vmcs_revision_id" },
      { vmx::instruction_error::vmread_vmwrite_invalid_component, "vmread_vmwrite_invalid_component" },
      { vmx::instruction_error::vmwrite_readonly_component, "vmwrite_readonly_component" },
      { vmx::instruction_error::reserved_1, "reserved_1" },
      { vmx::instruction_error::vmxon_in_vmx_root_op, "vmxon_in_vmx_root_op" },
      { vmx::instruction_error::vmentry_invalid_vmcs_executive_pointer, "vmentry_invalid_vmcs_executive_pointer" },
      { vmx::instruction_error::vmentry_non_launched_executive_vmcs, "vmentry_non_launched_executive_vmcs" },
      { vmx::instruction_error::vmentry_executive_vmcs_ptr, "vmentry_executive_vmcs_ptr" },
      { vmx::instruction_error::vmcall_non_clear_vmcs, "vmcall_non_clear_vmcs" },
      { vmx::instruction_error::vmcall_invalid_vmexit_fields, "vmcall_invalid_vmexit_fields" },
      { vmx::instruction_error::reserved_2, "reserved_2" },
      { vmx::instruction_error::vmcall_invalid_mseg_revision_id, "vmcall_invalid_mseg_revision_id" },
      { vmx::instruction_error::vmxoff_dual_monitor, "vmxoff_dual_monitor" },
      { vmx::instruction_error::vmcall_invalid_smm_monitor, "vmcall_invalid_smm_monitor" },
      { vmx::instruction_error::vmentry_invalid_vm_execution_control, "vmentry_invalid_vm_execution_control" },
      { vmx::instruction_error::vmentry_mov_ss, "vmentry_mov_ss" },
      { vmx::instruction_error::reserved_3, "reserved_3" },
      { vmx::instruction_error::invept_invvpid_invalid_operand, "invept_invvpid_invalid_operand" }
  };
}
