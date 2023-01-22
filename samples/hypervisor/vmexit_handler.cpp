#include "vmexit_handler.hpp"
#include <intrin.h>
#include "globals.hpp"
#include "x86.hpp"
#include "vpid.hpp"
#include "vmcall.hpp"
#include "vcpu.hpp"
#include "invept.hpp"
#include "efi_stub.hpp"
#include "ept_handler.hpp"
#include "pt_handler.hpp"
#include "win_driver.hpp"
#include "hook_builder.hpp"
#include "per_cpu_data.hpp"
#include "pe.hpp"

namespace hh::hv_event_handlers
{
  vmexit_handler::fx_state_saver::fx_state_saver(common::fxsave_area* ptr) noexcept : fxsave_area{ ptr }
  {
    *fxsave_area = common::fxsave_area{};
    _fxsave64(fxsave_area);
  }

  vmexit_handler::fx_state_saver::~fx_state_saver() noexcept
  {
    _fxrstor64(fxsave_area);
  }

  bool vmexit_handler::handlers_dispatcher(common::guest_regs* regs) noexcept
  {
    if (globals::panic_status)
    {
      __halt();
    }

    vcpu* current_vcpu = &globals::vcpus[per_cpu_data::get_cpu_id()];

    // We use AVX in some places so we need to save SIMD registers.
    fx_state_saver saver{ current_vcpu->fxsave_area() };
    regs->fx_area = current_vcpu->fxsave_area();
    vmexit_handler* this_ptr = current_vcpu->vmexit_handler().get();
    const uint64_t vmexit_reason = static_cast<uint16_t>(current_vcpu->vmexit_reason());

    current_vcpu->skip_instruction(true);

    if (per_cpu_data::callback_ready_status())
    {
      if (const auto [callback, context] = per_cpu_data::pop_root_mode_callback_from_queue();
        callback != nullptr)
      {
        callback(context.get());
        PRINT(("root mode callback called on core #%d\n", per_cpu_data::get_cpu_id()));
      }
    }

    (this_ptr->*(this_ptr->handlers_[vmexit_reason]))(regs, current_vcpu);

    if (current_vcpu->skip_instruction())
    {
      current_vcpu->resume_to_next_instruction();
    }

    return current_vcpu->vmxoff_executed();
  }

  uint64_t vmexit_handler::get_instruction_pointer_for_vmxoff() noexcept
  {
    return globals::vcpus[per_cpu_data::get_cpu_id()].vmxoff_state_guest_rip();
  }

  uint64_t vmexit_handler::get_stack_pointer_for_vmxoff() noexcept
  {
    return globals::vcpus[per_cpu_data::get_cpu_id()].vmxoff_state_guest_rsp();
  }

  void vmexit_handler::vm_resume() noexcept
  {
    __vmx_vmresume();

    // after vmresume we should never be here
    uint64_t error_code;
    vmx::exit_reason exit_reason;
    vmx::vmread(vmx::vmcs_fields::vm_instruction_error, error_code);
    vmx::vmread(vmx::vmcs_fields::vm_exit_reason, exit_reason);
    __vmx_off();

    PRINT(("vmresume error: 0x%llx\n", error_code));
    PRINT(("Exit reason: %a\n", enum_to_str(exit_reason).data()));

    bug_check(bug_check_codes::vmx_error, error_code);
  }

  vmexit_handler::vmexit_handler()
    : handlers_
    {
    &vmexit_handler::handle_exception_nmi,
    &vmexit_handler::handle_external_interrupt,
    &vmexit_handler::handle_triple_fault,
    &vmexit_handler::handle_init,
    &vmexit_handler::handle_sipi,
    &vmexit_handler::handle_io_smi,
    &vmexit_handler::handle_other_smi,
    &vmexit_handler::handle_pending_virt_intr,
    &vmexit_handler::handle_pending_virt_nmi,
    &vmexit_handler::handle_task_switch,
    &vmexit_handler::handle_cpuid,
    &vmexit_handler::handle_getsec,
    &vmexit_handler::handle_hlt,
    &vmexit_handler::handle_invd,
    &vmexit_handler::handle_invlpg,
    &vmexit_handler::handle_rdpmc,
    &vmexit_handler::handle_rdtsc,
    &vmexit_handler::handle_rsm,
    &vmexit_handler::handle_vmcall,
    &vmexit_handler::handle_vmclear,
    &vmexit_handler::handle_vmlaunch,
    &vmexit_handler::handle_vmptrld,
    &vmexit_handler::handle_vmptrst,
    &vmexit_handler::handle_vmread,
    &vmexit_handler::handle_vmresume,
    &vmexit_handler::handle_vmwrite,
    &vmexit_handler::handle_vmxoff,
    &vmexit_handler::handle_vmxon,
    &vmexit_handler::handle_cr_access,
    &vmexit_handler::handle_dr_access,
    &vmexit_handler::handle_io_instruction,
    &vmexit_handler::handle_msr_read,
    &vmexit_handler::handle_msr_write,
    &vmexit_handler::handle_invalid_guest_state,
    &vmexit_handler::handle_msr_loading,
    &vmexit_handler::handler_stub, // reserved_1
    &vmexit_handler::handle_mwait_instruction,
    &vmexit_handler::handle_monitor_trap_flag,
    &vmexit_handler::handler_stub, // reserved_2
    &vmexit_handler::handle_monitor_instruction,
    &vmexit_handler::handle_pause_instruction,
    &vmexit_handler::handle_mce_during_vmentry,
    &vmexit_handler::handler_stub, // reserved_3
    &vmexit_handler::handle_tpr_below_threshold,
    &vmexit_handler::handle_apic_access,
    &vmexit_handler::handle_virtualized_eoi,
    &vmexit_handler::handle_access_gdtr_or_idtr,
    &vmexit_handler::handle_access_ldtr_or_tr,
    &vmexit_handler::handle_ept_violation,
    &vmexit_handler::handle_ept_misconfig,
    &vmexit_handler::handle_invept,
    &vmexit_handler::handle_rdtscp,
    &vmexit_handler::handle_vmx_preemption_timer_expired,
    &vmexit_handler::handle_invvpid,
    &vmexit_handler::handle_wbinvd,
    &vmexit_handler::handle_xsetbv,
    &vmexit_handler::handle_apic_write,
    &vmexit_handler::handle_rdrand,
    &vmexit_handler::handle_invpcid,
    &vmexit_handler::handle_vmfunc,
    &vmexit_handler::handle_encls,
    &vmexit_handler::handle_rdseed,
    &vmexit_handler::handle_page_modification_log_full,
    &vmexit_handler::handle_xsaves,
    &vmexit_handler::handle_xrstors
    }
  {
  }

  void vmexit_handler::handler_stub(common::guest_regs* regs, vcpu* cpu_obj) noexcept
  {
    PRINT(("Unexpected VMEXIT occured. Reason: %a\n", enum_to_str(cpu_obj->vmexit_reason()).data()));
    bug_check(bug_check_codes::vmx_error, static_cast<uint64_t>(cpu_obj->vmexit_reason()));
  }

  void vmexit_handler::handle_exception_nmi(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_external_interrupt(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_triple_fault(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_init(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_sipi(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_io_smi(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_other_smi(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_pending_virt_intr(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_pending_virt_nmi(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_task_switch(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_cpuid(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_getsec(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_hlt(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_invd(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_invlpg(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_rdpmc(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_rdtsc(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_rsm(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_vmcall(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_vmclear(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_vmlaunch(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_vmptrld(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_vmptrst(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_vmread(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_vmresume(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_vmwrite(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_vmxoff(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_vmxon(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_cr_access(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_dr_access(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_io_instruction(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_msr_read(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_msr_write(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_invalid_guest_state(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_msr_loading(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_mwait_instruction(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_monitor_trap_flag(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_monitor_instruction(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_pause_instruction(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_mce_during_vmentry(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_tpr_below_threshold(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_apic_access(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_access_gdtr_or_idtr(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_access_ldtr_or_tr(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_ept_violation(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_ept_misconfig(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_invept(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_rdtscp(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_vmx_preemption_timer_expired(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_invvpid(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_wbinvd(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_xsetbv(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_apic_write(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_rdrand(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_invpcid(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_rdseed(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_pml_full(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_xsaves(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_xrstors(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_pcommit(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_virtualized_eoi(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_vmfunc(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_encls(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }
  void vmexit_handler::handle_page_modification_log_full(common::guest_regs* regs, vcpu* cpu_obj) { handler_stub(regs, cpu_obj); }

  void kernel_hook_assistant::handle_triple_fault(common::guest_regs* regs, vcpu* cpu_obj)
  {
    PRINT(("Triple fault error occured.\n"));
    PRINT(("CS base  - %016llx, CS - %016llx\n", cpu_obj->guest_cs().base_address, cpu_obj->guest_cs().selector.all));
    PRINT(("RIP  - %016llx, RSP - %016llx\n", cpu_obj->guest_rip(), cpu_obj->guest_rsp()));
    PRINT(("RAX  - %016llx, RCX - %016llx, RDX - %016llx\n", regs->rax, regs->rcx, regs->rdx));
    PRINT(("RBX  - %016llx, RSP - %016llx, RBP - %016llx\n", regs->rbx, 0ull, regs->rbp));
    PRINT(("RSI  - %016llx, RDI - %016llx\n", regs->rsi, regs->rdi));
    PRINT(("R8   - %016llx, R9  - %016llx, R10 - %016llx\n", regs->r8, regs->r9, regs->r10));
    PRINT(("R11  - %016llx, R12 - %016llx, R13 - %016llx\n", regs->r11, regs->r12, regs->r13));
    PRINT(("R14  - %016llx, R15 - %016llx\n", regs->r14, regs->r15));
    bug_check(bug_check_codes::kmode_exception_not_handled);
  }

  void kernel_hook_assistant::handle_vmx_command(common::guest_regs* regs, vcpu* cpu_obj) const noexcept
  {
    x86::rflags_t rflags = cpu_obj->guest_rflags();
    rflags.flags.carry_flag = 1; // cf=1 indicate vmx instructions fail
    cpu_obj->guest_rflags(rflags);
  }

  // We do not support vmx commands
  void kernel_hook_assistant::handle_vmclear(common::guest_regs* regs, vcpu* cpu_obj) { handle_vmx_command(regs, cpu_obj); }
  void kernel_hook_assistant::handle_vmlaunch(common::guest_regs* regs, vcpu* cpu_obj) { handle_vmx_command(regs, cpu_obj); }
  void kernel_hook_assistant::handle_vmptrld(common::guest_regs* regs, vcpu* cpu_obj) { handle_vmx_command(regs, cpu_obj); }
  void kernel_hook_assistant::handle_vmptrst(common::guest_regs* regs, vcpu* cpu_obj) { handle_vmx_command(regs, cpu_obj); }
  void kernel_hook_assistant::handle_vmread(common::guest_regs* regs, vcpu* cpu_obj) { handle_vmx_command(regs, cpu_obj); }
  void kernel_hook_assistant::handle_vmresume(common::guest_regs* regs, vcpu* cpu_obj) { handle_vmx_command(regs, cpu_obj); }
  void kernel_hook_assistant::handle_vmwrite(common::guest_regs* regs, vcpu* cpu_obj) { handle_vmx_command(regs, cpu_obj); }
  void kernel_hook_assistant::handle_vmxoff(common::guest_regs* regs, vcpu* cpu_obj) { handle_vmx_command(regs, cpu_obj); }
  void kernel_hook_assistant::handle_vmxon(common::guest_regs* regs, vcpu* cpu_obj) { handle_vmx_command(regs, cpu_obj); }

  void kernel_hook_assistant::handle_cr_access(common::guest_regs* regs, vcpu* cpu_obj)
  {
    const auto exit_info = cpu_obj->exit_qualification();
    uint64_t* reg_ptr = &regs->rax + exit_info.mov_cr.flags.gp_register;

    // Because its RSP and as we didn't save RSP correctly (because of pushes) so we have to make it points to the GUEST_RSP
    if (exit_info.mov_cr.flags.gp_register == 4)
    {
      *reg_ptr = cpu_obj->guest_rsp();
    }

    switch (exit_info.mov_cr.flags.access_type)
    {
    case vmx::exit_qualification_mov_cr_t::access_to_cr:
    {
      switch (exit_info.mov_cr.flags.cr_number)
      {
      case 0:
      {
        x86::cr0_t old_cr0 = cpu_obj->guest_cr0();
        x86::cr0_t new_cr0 = { .all = *reg_ptr };

        cpu_obj->guest_cr0(new_cr0);
        cpu_obj->cr0_shadow(new_cr0);

        if (old_cr0.flags.paging_enable != new_cr0.flags.paging_enable)
        {
          cpu_obj->switch_guest_paging_mode(new_cr0);
          PRINT(("Processor #%d switching to the long mode.\n", per_cpu_data::get_cpu_id()));
        }

        break;
      }

      case 3:
      {
        // 63 bit invalidates TLB entries. On modern win it will cause bsod.
        x86::cr3_t cr3 = { .all = *reg_ptr };
        cr3.flags.pcid_invalidate = 0;

        cpu_obj->guest_cr3(cr3);
        vmx::invvpid_single_context(vmx::vpid_tag);

        break;
      }

      case 4:
      {
        x86::cr4_t new_cr4 = { .all = *reg_ptr };
        new_cr4.flags.vmx_enable = 0;
        cpu_obj->guest_cr4(new_cr4);
        cpu_obj->cr4_shadow(new_cr4);

        break;
      }

      default:
      {
        PRINT(("Unsupported cr register access detected. Register num %d\n",
          exit_info.mov_cr.flags.cr_number));
        break;
      }
      }

      break;
    }

    case vmx::exit_qualification_mov_cr_t::access_from_cr:
    {
      switch (exit_info.mov_cr.flags.cr_number)
      {
      case 0:
      {
        *reg_ptr = cpu_obj->guest_cr0().all;
        break;
      }

      case 3:
      {
        *reg_ptr = cpu_obj->guest_cr3().all;
        break;
      }

      case 4:
      {
        *reg_ptr = cpu_obj->guest_cr4().all;
        break;
      }

      default:
      {
        PRINT(("Unsupported cr register access detected. Register num %d\n",
          exit_info.mov_cr.flags.cr_number));
        break;
      }
      }

      break;
    }

    default:
    {
      PRINT(("Unsopported cr register operation detected. Operation type %d\n",
        exit_info.mov_cr.flags.access_type));
      break;
    }
    }
  }

  void kernel_hook_assistant::handle_msr_read(common::guest_regs* regs, vcpu* cpu_obj)
  {
    x86::msr::register_content msr = {};

    if ((regs->rcx <= x86::msr::low_msr_high_range) || (regs->rcx >= x86::msr::high_msr_low_range
      && regs->rcx <= x86::msr::high_msr_high_range)
      || (regs->rcx >= x86::msr::reserved_msr_range_low && regs->rcx <= x86::msr::reserved_msr_range_hi))
    {
      PRINT(("RDMSR executed with rcx = 0x%llx\n", regs->rcx));

      switch (regs->rcx)
      {
      case x86::msr::feature_control_msr_t::msr_id:
      {
        // We don't support nested virtualization so we mark VMX related bits to zero.
        x86::msr::feature_control_msr_t feature_control = { .all = __readmsr(regs->rcx) };
        feature_control.fields.enable_vmxon = false;

        msr.all = feature_control.all;

        break;
      }

      default:
      {
        msr.all = __readmsr(regs->rcx);
        break;
      }
      }
    }

    regs->rax = msr.low;
    regs->rdx = msr.high;
  }

  void kernel_hook_assistant::handle_msr_write(common::guest_regs* regs, vcpu* cpu_obj)
  {
    x86::msr::register_content msr;

    if ((regs->rcx <= x86::msr::low_msr_high_range) || (regs->rcx >= x86::msr::high_msr_low_range
      && regs->rcx <= x86::msr::high_msr_high_range)
      || (regs->rcx >= x86::msr::reserved_msr_range_low && regs->rcx <= x86::msr::reserved_msr_range_hi))
    {
      PRINT(("WRMSR executed with rcx = 0x%llx\n", regs->rcx));

      switch (regs->rcx)
      {
      case x86::msr::feature_control_msr_t::msr_id:
      {
        x86::msr::feature_control_msr_t feature_control = { .all = regs->rax | (regs->rdx << 32) };
        feature_control.fields.enable_vmxon = 1;
        feature_control.fields.lock = 1;

        msr.all = feature_control.all;

        break;
      }

      default:
      {
        msr.low = static_cast<uint32_t>(regs->rax);
        msr.high = static_cast<uint32_t>(regs->rdx);

        break;
      }
      }

      __writemsr(regs->rcx, msr.all);
    }
  }

  void kernel_hook_assistant::handle_cpuid(common::guest_regs* regs, vcpu* cpu_obj)
  {
    int32_t cpu_info[4];

    switch (static_cast<x86::cpuid_leaf_functions>(regs->rax))
    {
    case x86::cpuid_leaf_functions::cpuid_version_information:
    {
      // We don't support nested virtualization so we mark hypervisor related bits to zero.
      common::cpuid_eax_01 data = {};
      __cpuidex(reinterpret_cast<int*>(data.cpu_info), static_cast<int32_t>(regs->rax), static_cast<int32_t>(regs->rcx));

      data.feature_information_ecx.hypervisor_present = 0;
      common::memcpy_128bit(cpu_info, data.cpu_info, 1);

      break;
    }

    case x86::cpuid_leaf_functions::processor_brand_string_0:
    {
      cpu_info[0] = 'epuS';
      cpu_info[1] = 'iaBr';
      cpu_info[2] = ' lak';
      cpu_info[3] = 'B 9i';

      break;
    }

    case x86::cpuid_leaf_functions::processor_brand_string_1:
    {
      cpu_info[0] = 'Ejmo';
      cpu_info[1] = 'itid';
      cpu_info[2] = 'x no';
      cpu_info[3] = ' 001';

      break;
    }

    case x86::cpuid_leaf_functions::processor_brand_string_2:
    {
      cpu_info[0] = 'eroc';
      cpu_info[1] = '01 s';
      cpu_info[2] = 'hM 0';
      cpu_info[3] = '   z';

      break;
    }

    default:
    {
      __cpuidex(cpu_info, static_cast<int32_t>(regs->rax), static_cast<int32_t>(regs->rcx));
      break;
    }
    }

    regs->rax = cpu_info[0];
    regs->rbx = cpu_info[1];
    regs->rcx = cpu_info[2];
    regs->rdx = cpu_info[3];
  }

  void kernel_hook_assistant::handle_io_instruction(common::guest_regs* regs, vcpu* cpu_obj)
  {
    PRINT(("We don't support io instructions.\n"));
  }

  void kernel_hook_assistant::handle_xsetbv(common::guest_regs* regs, vcpu* cpu_obj)
  {
    x86::cr4_t cr4 = x86::read<x86::cr4_t>();

    cr4.flags.os_xsave = 1;

    x86::write<x86::cr4_t>(cr4);
    _xsetbv(regs->rcx, regs->rdx << 32 | regs->rax);
  }

  void kernel_hook_assistant::handle_sipi(common::guest_regs* regs, vcpu* cpu_obj)
  {
    PRINT(("handle_sipi has been called with core #%d\n", per_cpu_data::get_cpu_id()));

    vmx::exit_qualification_t exit_qualification = cpu_obj->exit_qualification();

    auto cs = cpu_obj->guest_cs();
    cs.selector.all = exit_qualification.sipi.vector << 8;
    cs.base_address = reinterpret_cast<void*>(exit_qualification.sipi.vector << 12);
    cpu_obj->guest_cs(cs);

    // ICR recives values from SIPI.
    // ICR has "Vector" field that means segment address in real addressing mode.
    // For example: 08h vector value becomes 0800h:0000 address in real mode.
    // 800h multiplies by 16 and we get 8000h physical address. So vector contains address in pages (0x1000).

    cpu_obj->guest_rip(0);
    vmx::invvpid_all_contexts();
    cpu_obj->guest_activity_state(static_cast<uint64_t>(vmx::guest_activity_state::active));
    cpu_obj->skip_instruction(false);
  }

  void kernel_hook_assistant::handle_init(common::guest_regs* regs, vcpu* cpu_obj)
  {
    PRINT(("handle_init has been called with core #%d\n", per_cpu_data::get_cpu_id()));

    // Recreate CPU state after INIT signal

    cpu_obj->guest_rflags(x86::rflags_t{ .all = x86::rflags_t::fixed_bits });
    cpu_obj->guest_rip(0xfff0);

    x86::cr0_t cr0 = {};
    cr0.flags.extension_type = 1;

    cpu_obj->cr0_shadow(cr0);
    x86::write<x86::cr2_t>(x86::cr2_t{});
    cpu_obj->guest_cr3(x86::cr3_t{});
    cpu_obj->cr4_shadow(x86::cr4_t{});

    cpu_obj->guest_cr0(vmx::adjust_guest_cr0(cr0, cpu_obj->secondary_vm_exec_control()));
    cpu_obj->guest_cr4(vmx::adjust_guest_cr4(x86::cr4_t{}));

    x86::segment_access_t segment_access = {};
    segment_access.type = x86::segment_access_t::type_execute_read_accessed;
    segment_access.descriptor_type = 1;
    segment_access.present = 1;

    x86::segment_t<x86::cs_t> cs = {};
    cs.selector.all = 0xf000;
    cs.base_address = reinterpret_cast<void*>(0xffff0000);
    cs.limit = 0xffff;
    cs.access.all = segment_access.all;
    cpu_obj->guest_cs(cs);

    segment_access.type = x86::segment_access_t::type_read_write_accessed;

    x86::segment_t<x86::ss_t> ss = {};
    ss.limit = 0xffff;
    ss.access.all = segment_access.all;
    cpu_obj->guest_ss(ss);

    x86::segment_t<x86::ds_t> ds = {};
    ds.limit = 0xffff;
    ds.access.all = segment_access.all;
    cpu_obj->guest_ds(ds);

    x86::segment_t<x86::es_t> es = {};
    es.limit = 0xffff;
    es.access.all = segment_access.all;
    cpu_obj->guest_es(es);

    x86::segment_t<x86::fs_t> fs = {};
    fs.limit = 0xffff;
    fs.access.all = segment_access.all;
    cpu_obj->guest_fs(fs);

    x86::segment_t<x86::gs_t> gs = {};
    gs.limit = 0xffff;
    gs.access.all = segment_access.all;
    cpu_obj->guest_gs(gs);

    common::cpuid_eax_01 data = {};
    __cpuid(reinterpret_cast<int*>(data.cpu_info), 1);

    regs->rdx = 0x600 | (static_cast<uint64_t>(data.version_information.extended_model_id) << 16);
    regs->rbx = 0;
    regs->rcx = 0;
    regs->rsi = 0;
    regs->rdi = 0;
    regs->rbp = 0;
    regs->r8 = 0;
    regs->r9 = 0;
    regs->r10 = 0;
    regs->r11 = 0;
    regs->r12 = 0;
    regs->r13 = 0;
    regs->r14 = 0;
    regs->r15 = 0;

    cpu_obj->guest_rsp(0);

    auto gdtr = x86::gdtr_t{};
    auto idtr = x86::idtr_t{};

    gdtr.base_address = 0;
    gdtr.limit = 0xffff;
    idtr.base_address = 0;
    idtr.limit = 0xffff;

    cpu_obj->guest_gdtr(gdtr);
    cpu_obj->guest_idtr(idtr);

    segment_access.type = x86::segment_access_t::type_ldt;
    segment_access.descriptor_type = 0;
    x86::segment_t<x86::ldtr_t> ldtr = {};
    ldtr.limit = 0xffff;
    ldtr.access.all = segment_access.all;
    cpu_obj->guest_ldtr(ldtr);

    segment_access.type = x86::segment_access_t::type_32b_tss_busy;
    x86::segment_t<x86::tr_t> tr = {};
    tr.limit = 0xffff;
    tr.access.all = segment_access.all;
    cpu_obj->guest_tr(tr);

    x86::write<x86::dr0_t>(x86::dr0_t{});
    x86::write<x86::dr1_t>(x86::dr1_t{});
    x86::write<x86::dr2_t>(x86::dr2_t{});
    x86::write<x86::dr3_t>(x86::dr3_t{});
    x86::write<x86::dr6_t>(x86::dr6_t{ .all = 0xffff0ff0 });

    cpu_obj->guest_dr7(x86::dr7_t{ .all = 0x400 });
    cpu_obj->guest_efer(x86::msr::efer_t{});

    x86::msr::vmx_entry_ctls_t vmx_entry_ctls = cpu_obj->vm_entry_controls();
    vmx_entry_ctls.flags.ia32e_mode_guest = 0;
    cpu_obj->vm_entry_controls(vmx_entry_ctls, x86::msr::read<x86::msr::vmx_basic_msr_t>());

    cpu_obj->guest_activity_state(static_cast<uint64_t>(vmx::guest_activity_state::wait_for_sipi));
    cpu_obj->skip_instruction(false);
  }

  void kernel_hook_assistant::handle_pending_virt_nmi(common::guest_regs* regs, vcpu* cpu_obj)
  {
    PRINT(("\"handle pending virt nmi\" has been called\n"));

    cpu_obj->set_nmi_window_exiting(false);
    cpu_obj->inject_interrupt(interrupt_templates::nmi());
    cpu_obj->skip_instruction(false);
  }

  void kernel_hook_assistant::handle_exception_nmi(common::guest_regs* regs, vcpu* cpu_obj)
  {
    interrupt curr_interrupt = cpu_obj->get_exit_interrupt();

    PRINT(("Received interrupt with vector %a\n", enum_to_str(curr_interrupt.vector()).data()));

    switch (curr_interrupt.vector())
    {
    case exception_vector::nmi_interrupt:
    {
      // Inject NMI as soon as possible.
      cpu_obj->set_nmi_window_exiting(true);
      break;
    }

    // We map our win driver to system process on this exception during patchguard initialization.
    case exception_vector::divide_error:
    {
      static bool isKeInitAmd64SpecificStateCalled = false;

      if (!isKeInitAmd64SpecificStateCalled)
      {
        uint16_t content;
        bool is_base_finded = false;
        PRINT(("Trying to get address of ntoskrnl.exe\n"));

        uint64_t rip = cpu_obj->guest_rip();
        uint64_t ntoskrnl_base = rip & ~(static_cast<uint64_t>(common::page_size) - 1);
        size_t size_of_scan_area = 0x1000;

        for (size_t j = 0; j < size_of_scan_area; j++, ntoskrnl_base -= common::page_size)
        {
          try
          {
            const auto mapped_guest_content = globals::pt_handler->map_guest_address(cpu_obj->guest_cr3(),
              reinterpret_cast<uint8_t*>(ntoskrnl_base), common::page_size);
            mapped_guest_content->memcpy(&content, 0, sizeof(content));
          }
          catch (...)
          {
            continue;
          }

          if (content == 0x5A4D)
          {
            if (portable_executable::get_kernel_module_export(ntoskrnl_base, "BgkDisplayCharacter"))
            {
              is_base_finded = true;
              break;
            }
          }
        }

        if (is_base_finded)
        {
          PRINT(("ntoskrnl.exe base finded: 0x%llx\n", ntoskrnl_base));
          PRINT(("cr3 = 0x%llx\n", cpu_obj->guest_cr3().all));
          PRINT(("Trying to load our win driver.\n"));

          globals::win_driver_struct->ntoskrnl_base = ntoskrnl_base;
          uint64_t win_driver_entry_point;

          try
          {
            win_driver_entry_point = win_driver::load_image_from_memory(ntoskrnl_base, cpu_obj);
            PRINT(("Win driver successfully loaded. Image entry point: 0x%llx\n", win_driver_entry_point));
          }
          catch (std::exception& e)
          {
            PRINT(("Failed to load win driver. Error: %a\n", e.what()));
          }
        }
        else
        {
          PRINT(("ntoskrnl.exe base hasn't been finded\n"));
        }

        isKeInitAmd64SpecificStateCalled = true;
        break;
      }

      cpu_obj->inject_interrupt(interrupt_templates::divide_error());
      break;
    }

    default:
    {
      PRINT(("Received unknown exception vector in \"handle_exception_nmi\", vector num = %d", curr_interrupt.vector()));
      hh::bug_check(bug_check_codes::unexpected_exception_or_interrupt, static_cast<uint64_t>(curr_interrupt.vector()));
    }
    }

    cpu_obj->skip_instruction(false);
  }

  void kernel_hook_assistant::handle_hlt(common::guest_regs* regs, vcpu* cpu_obj)
  {
    PRINT(("hlt instruction executed.\n"));
    __halt();
  }

  void kernel_hook_assistant::handle_ept_misconfig(common::guest_regs* regs, vcpu* cpu_obj)
  {
    uint64_t guest_phys_address;
    vmx::vmread(vmx::vmcs_fields::guest_physical_address, guest_phys_address);

    PRINT(("Fatal error. EPT Misconfiguration occured.\n"));
    PRINT(("Physical address 0x%llx\n", guest_phys_address));

    bug_check(bug_check_codes::vmx_error);
  }

  void kernel_hook_assistant::handle_vmcall(common::guest_regs* regs, vcpu* cpu_obj)
  {
    auto vmcall_status = common::status::hv_success;

    auto request_num = static_cast<vmx::vmcall_number>(regs->rcx & 0xFFFFFFFF);

    try
    {
      switch (request_num)
      {
      case vmx::vmcall_number::test:
      {
        PRINT(("test vmcall called with params 0x%llx, 0x%llx, 0x%llx\n", regs->rdx, regs->r8, regs->r9));
        PRINT(("core index = %d\n", per_cpu_data::get_cpu_id()));

        break;
      }

      case vmx::vmcall_number::change_page_attrib:
      {
        hook::guest_hook_request_info hook_request_info = {};

        auto mapped_memory = globals::pt_handler->map_guest_address(cpu_obj->guest_cr3(),
          reinterpret_cast<uint8_t*>(regs->rdx), sizeof(hook::guest_hook_request_info));
        mapped_memory->memcpy(&hook_request_info, 0, sizeof(hook_request_info));

        PRINT(("hook_request_info params:\n"));
        PRINT(("target_page_address = 0x%llx, hooked_page_address = 0x%llx\n target_cr3 = 0x%llx, required_attributes = 0x%llx\n",
          hook_request_info.target_page_address, hook_request_info.hooked_page_address,
          hook_request_info.target_cr3.all, hook_request_info.required_attributes.all));

        // EPT shadow hook magic happens here
        globals::hook_handler->perform_page_hook(hook_request_info);

        break;
      }

      // We need to invalidate EPT TLB entries for all logical CPUs after EPT hook.
      // I will add in the future root mode callback that is executed through NMI IPI
      case vmx::vmcall_number::notify_all_to_invalidate_ept:
      {
        per_cpu_data::push_root_mode_callback_to_queue([](void* context) -> void
          {
            uint64_t ept_pointer_flags = reinterpret_cast<uint64_t>(context);

        if (ept_pointer_flags == 0)
        {
          vmx::invept_all_contexts();
        }
        else
        {
          vmx::invept_single_context(ept_pointer_flags);
        }
          }, reinterpret_cast<void*>(globals::ept_handler->get_eptp().flags));

        break;
      }

      case vmx::vmcall_number::invept_all_contexts:
      {
        vmx::invept_all_contexts();
        break;
      }

      case vmx::vmcall_number::invept_single_context:
      {
        vmx::invept_single_context(regs->rdx);
        break;
      }

      // These 4 vmcalls is intended for passing information to
      // our mapped win driver.
      case vmx::vmcall_number::get_win_driver_pool_address:
      {
        regs->rdx = reinterpret_cast<uint64_t>(globals::win_driver_struct->mem_pool_for_allocator_virtual_address);
        break;
      }

      case vmx::vmcall_number::get_win_driver_pool_size:
      {
        regs->rdx = globals::win_driver_struct->mem_pool_size;
        break;
      }

      case vmx::vmcall_number::get_ntoskrnl_base_address:
      {
        regs->rdx = globals::win_driver_struct->ntoskrnl_base;
        break;
      }

      case vmx::vmcall_number::get_physical_address_for_virtual:
      {
        regs->rdx = common::get_physical_address_for_virtual_address_by_cr3(cpu_obj->guest_cr3(),
          reinterpret_cast<void*>(regs->rdx));
        break;
      }

      // For test purpouses.
      case vmx::vmcall_number::panic:
      {
        bug_check(bug_check_codes::kmode_exception_not_handled);
        break;
      }

      case vmx::vmcall_number::unhook_all_pages:
      {
        globals::hook_handler->unhook_all_pages();
        break;
      }

      case vmx::vmcall_number::unhook_single_page:
      {
        uint64_t phys_address = common::get_physical_address_for_virtual_address_by_cr3(cpu_obj->guest_cr3(),
          reinterpret_cast<void*>(regs->rdx));

        phys_address = reinterpret_cast<uint64_t>(PAGE_ALIGN(phys_address));
        globals::hook_handler->unhook_page(phys_address);

        break;
      }

      default:
      {
        PRINT(("Unsupported vmcall number.\n"));
        vmcall_status = common::status::hv_unsuccessful;

        break;
      }
      }
    }
    catch (std::exception& e)
    {
      PRINT(("Exception in vmcall handler occured. %a\n", e.what()));
      vmcall_status = common::status::hv_unsuccessful;
    }

    regs->rax = static_cast<uint64_t>(vmcall_status);
  }

  // Restore EPT entry to hooked one after MTF.
  void kernel_hook_assistant::handle_monitor_trap_flag(common::guest_regs* regs, vcpu* cpu_obj)
  {
    if (*cpu_obj->mtf_restore_point())
    {
      hook::hook_info* page = *cpu_obj->mtf_restore_point();
      globals::ept_handler->set_pml1_and_invalidate_tlb(page->entry_address, page->changed_entry,
        vmx::invvpid_type::invvpid_individual_address);
    }

    cpu_obj->set_monitor_trap_flag(false);
    cpu_obj->skip_instruction(false);
  }

  void kernel_hook_assistant::handle_ept_violation(common::guest_regs* regs, vcpu* cpu_obj)
  {
    try
    {
      const uint64_t guest_physical_address = cpu_obj->exit_guest_physical_address();
      const vmx::exit_qualification_t exit_qualification = cpu_obj->exit_qualification();
      hook::hook_info* hooked_page = globals::hook_handler->get_hooked_page_info(guest_physical_address);

      if (hooked_page == nullptr)
      {
        PRINT(("Unexpected EPT violation.\n"));
        return;
      }

      globals::ept_handler->set_pml1_and_invalidate_tlb(hooked_page->entry_address,
        hooked_page->original_entry, vmx::invvpid_type::invvpid_individual_address);

      uint64_t rip = cpu_obj->guest_rip();
      uint64_t exact_accessed_address = reinterpret_cast<uint64_t>(hooked_page->virtual_address) + (guest_physical_address
        - reinterpret_cast<uint64_t>(PAGE_ALIGN(guest_physical_address)));

      //PRINT(("Ept violation, RIP : 0x%llx, exact address : 0xllx\n", rip, exact_accessed_address));

      // We replace hooked entry for one CPU command to original version using MTF.
      *cpu_obj->mtf_restore_point() = hooked_page;
      cpu_obj->set_monitor_trap_flag(true);
    }
    catch (std::exception& e)
    {
      PRINT((__FUNCTION__": ""exception occured: %a\n", e.what()));
      cpu_obj->inject_interrupt(interrupt_templates::general_protection());
    }

    cpu_obj->skip_instruction(false);
  }
}
