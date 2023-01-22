#include "exception.hpp"
#include "msr.hpp"
#include "segment.hpp"
#include "x86.hpp"
#include "vcpu.hpp"
#include "efi_stub.hpp"
#include "globals.hpp"
#include "per_cpu_data.hpp"

namespace hh::hv_operations
{
  extern "C" hh::x86::idtr_t host_guest_idtr = {};
  static x86::idt_entry_t host_guest_idt_entries[x86::idt_entry_count] = {};
  extern "C" void default_exception_handlers();

  void initialize_host_idt() noexcept
  {
    auto handler_base = reinterpret_cast<uint64_t>(default_exception_handlers);

    for (size_t j = 0; j < x86::idt_entry_count; j++)
    {
      constexpr uint64_t size_of_handler_till_0x7f = 9;
      constexpr uint64_t size_of_handler_from_0x80 = 12; // after 0x7f push opcode takes 5 bytes instead 2

      uint64_t size_of_handler;

      if (j < 0x80)
      {
        size_of_handler = size_of_handler_till_0x7f;
      }
      else
      {
        size_of_handler = size_of_handler_from_0x80;
      }

      uint64_t handler_address = handler_base + j * size_of_handler;

      host_guest_idt_entries[j].base_address(reinterpret_cast<void*>(handler_address));
      host_guest_idt_entries[j].selector = x86::read<x86::cs_t>();
      host_guest_idt_entries[j].access.type = x86::segment_access_t::type_execute_read_conforming;
      host_guest_idt_entries[j].access.present = 1;
    }

    host_guest_idtr.limit = sizeof(host_guest_idt_entries) - 1;
    host_guest_idtr.base_address = reinterpret_cast<uint64_t>(&host_guest_idt_entries[0]);
  }
}

struct exception_stack
{
  uint64_t r15;
  uint64_t r14;
  uint64_t r13;
  uint64_t r12;
  uint64_t r11;
  uint64_t r10;
  uint64_t r9;
  uint64_t r8;
  uint64_t rdi;
  uint64_t rsi;
  uint64_t rbp;
  uint64_t rbx;
  uint64_t rdx;
  uint64_t rcx;
  uint64_t rax;
  uint64_t interrupt_number;
  uint64_t error_code;
  uint64_t rip;
  uint64_t cs;
  hh::x86::rflags_t rflags;
};

namespace hh
{
  // Main IDT dispatcher for host.
  extern "C" void handle_host_exception(const exception_stack * stack)
  {
    // Enable NMI-window exiting if NMI occurred during VMX root mode so that
    // we can re-inject it when possible.

    if (stack->interrupt_number == static_cast<uint64_t>(exception_vector::nmi_interrupt))
    {
      globals::vcpus[per_cpu_data::get_cpu_id()].set_nmi_window_exiting(true);
      return;
    }

    vmx::exit_reason exit_reason;
    vmx::vmread(vmx::vmcs_fields::vm_exit_reason, exit_reason);

    uint32_t core_id;
    __rdtscp(&core_id);

    // Panic here
    PRINT(("Exception or interrupt 0x%llx(0x%llx)\n", stack->interrupt_number, stack->error_code));
    PRINT(("Interrupt string presentation: %a\n", enum_to_str(static_cast<exception_vector>(stack->interrupt_number)).data()));
    PRINT(("Core id = #%d\n", core_id));
    PRINT(("Last exit reason: %a\n", enum_to_str(exit_reason).data()));
    PRINT(("RIP  - %016llx, CS  - %016llx, RFLAGS - %016llx\n", stack->rip, stack->cs, stack->rflags.all));
    PRINT(("RAX  - %016llx, RCX - %016llx, RDX - %016llx\n", stack->rax, stack->rcx, stack->rdx));
    PRINT(("RBX  - %016llx, RSP - %016llx, RBP - %016llx\n", stack->rbx, 0ull, stack->rbp));
    PRINT(("RSI  - %016llx, RDI - %016llx\n", stack->rsi, stack->rdi));
    PRINT(("R8   - %016llx, R9  - %016llx, R10 - %016llx\n", stack->r8, stack->r9, stack->r10));
    PRINT(("R11  - %016llx, R12 - %016llx, R13 - %016llx\n", stack->r11, stack->r12, stack->r13));
    PRINT(("R14  - %016llx, R15 - %016llx\n", stack->r14, stack->r15));
    PRINT(("CR2  - %016llx\n", __readcr2()));

    bug_check(bug_check_codes::unexpected_exception_or_interrupt, stack->interrupt_number);
  }
}
