#include "hypervisor.hpp"
#include "common.hpp"
#include "x86.hpp"
#include <exception>
#include "pt_handler.hpp"
#include "uefi.hpp"
extern "C"
{
#include <Pi/PiMultiPhase.h>
#include <Protocol/MpService.h>
}
#include "globals.hpp"
#include "memory_manager.hpp"
#include "ept_handler.hpp"
#include "vcpu.hpp"
#include "vmcall.hpp"
#include "vmexit_handler.hpp"
#include "hook_builder.hpp"
#include "per_cpu_data.hpp"

namespace hh::hv_operations
{
  void is_vmx_supported()
  {
    x86::msr::feature_control_msr_t feature_control_msr = x86::msr::read<x86::msr::feature_control_msr_t>();

    common::cpuid_eax_01 data = {};
    __cpuid(reinterpret_cast<int*>(data.cpu_info), 1);

    if (data.feature_information_ecx.virtual_machine_extensions == 0)
    {
      throw std::exception{ __FUNCTION__": ""VMX operation is not supported: CPUID.1:ECX.VMX[bit 5] == 0." };
    }

    // BIOS lock check. If lock is 0 then vmxon causes a general protection exception
    if (feature_control_msr.fields.lock == 0)
    {
      feature_control_msr.fields.lock = true;
      feature_control_msr.fields.enable_vmxon = true;

      x86::msr::write<x86::msr::feature_control_msr_t>(feature_control_msr);
    }
    else if (feature_control_msr.fields.enable_vmxon == false)
    {
      throw std::exception{ __FUNCTION__": ""Intel vmx feature is locked in BIOS." };
    }
  }

  void initialize_hypervisor()
  {
    is_vmx_supported();
    auto status = gBS->LocateProtocol(&gEfiMpServiceProtocolGuid, nullptr, reinterpret_cast<void**>(&globals::gEfiMpServiceProtocol));

    if (EFI_ERROR(status))
    {
      globals::gEfiMpServiceProtocol = nullptr;
    }

    globals::mem_manager = new tlsf_allocator{};
    globals::hook_handler = new hook_builder{};
    globals::ept_handler = new ept::ept_handler{};
    globals::pt_handler = new pt::pt_handler{};
    globals::number_of_cpus = common::get_active_processors_count();
    globals::cpu_related_data = reinterpret_cast<per_cpu_data*>(new uint8_t[sizeof(per_cpu_data) * globals::number_of_cpus]);
    std::shared_ptr<hv_event_handlers::vmexit_handler> vmexit_handler = std::make_shared<hv_event_handlers::kernel_hook_assistant>();
    globals::vcpus = reinterpret_cast<vcpu*>(new uint8_t[sizeof(vcpu) * globals::number_of_cpus]);

    globals::ept_handler->initialize_ept();
    globals::pt_handler->initialize_pt();

    for (size_t j = 0; j < globals::number_of_cpus; j++)
    {
      new (&globals::vcpus[j]) vcpu{ vmexit_handler };
    }

    initialize_host_idt();

    bool callback_status = true;
    std::pair<vcpu*, bool&> callback_argument = { globals::vcpus, callback_status };

    common::run_on_all_processors([](void* callback_context)
      {

        auto [vcpu_array, callback_status_inner] = *static_cast<std::pair<vcpu*, bool&>*>(callback_context);
    new (&globals::cpu_related_data[common::get_current_processor_number()]) per_cpu_data{};

    try
    {
      vcpu_array[common::get_current_processor_number()].initialize_guest();
    }
    catch (std::exception& e)
    {
      PRINT(("%a\n", e.what()));
      callback_status_inner = false;
    }

      }, &callback_argument);

    // Fence because it is possible to read wrong value from callback_status
    // because store buffer is not flushed on another logical CPU.
    _mm_lfence();

    if (!callback_status)
    {
      throw std::exception{};
    }

    if (vmx::vmcall(vmx::vmcall_number::test) != common::status::hv_success)
    {
      throw std::exception(__FUNCTION__": ""Hypervisor initialized but test vmcall failed.");
    }
  }
}
