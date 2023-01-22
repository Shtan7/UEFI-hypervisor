#include <cstdint>
#include <ntddk.h>
#include <intrin.h>
#include "cpp_support.hpp"
#include "vmcall.hpp"
#include "globals.hpp"
#include "memory_manager.hpp"
#include "common.hpp"
#include "hooking.hpp"
#include "hook_functions.hpp"

using namespace hh;

void install_hooks(void* ntoskrnl_base)
{
  globals::hook_builder->ept_hook(hook::hook_context{}
    .set_target_address(hook::get_address_by_ssdt(hook::ssdt_numbers::NtCreateFile, false, ntoskrnl_base))
    .set_exec()
    .set_functions(hh::NtCreateFile, reinterpret_cast<void**>(&hook::pointers::NtCreateFileOrig)));

  __vmcall(vmcall_number::notify_all_to_invalidate_ept);
}

extern "C" NTSTATUS DriverEntry()
{
  // construct global, static variables
  __crt_init();

  const uint64_t ntoskrnl_base = __vmcall_with_returned_value(vmcall_number::get_ntoskrnl_base_address);
  auto pool_address = reinterpret_cast<uint8_t*>(__vmcall_with_returned_value(vmcall_number::get_win_driver_pool_address));
  const uint32_t pool_size = __vmcall_with_returned_value(vmcall_number::get_win_driver_pool_size);

  try
  {
    globals::mem_manager = new (pool_address) tlsf_allocator(pool_address + 0x1000, pool_size - 0x1000);
    globals::hook_builder = new hook::hook_builder;

    install_hooks(reinterpret_cast<void*>(ntoskrnl_base));
  }
  catch(std::exception& e)
  {
    PRINT(("Caught exception in win driver: %a\n", e.what()));
  }

	return STATUS_SUCCESS;
}
