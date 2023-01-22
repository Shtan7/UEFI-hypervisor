#include "globals.hpp"

extern "C"
{
#include "..\..\edk2 libs\vshacks.h"
#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/ShellLib.h>
}

#include "uefi.hpp"
#include "efi_stub.hpp"
#include "cpp_support.hpp"
#include <cstdint>
#include "common.hpp"
#include "hypervisor.hpp"
#include <exception>
#include "win_driver.hpp"
#include <intrin.h>

using namespace hh;

extern "C" EFI_GUID gEfiSampleDriverProtocolGuid = EFI_SAMPLE_DRIVER_PROTOCOL_GUID;

// We run on any UEFI Specification
extern "C" const UINT32 _gUefiDriverRevision = 0;

// Our name
extern "C" CHAR8 * gEfiCallerBaseName = const_cast<CHAR8*>("HH uefi");
static EFI_EVENT exit_boot_services_event = {};
static EFI_EVENT virtual_notify_event = {};
extern "C" EFI_GUID gEfiEventExitBootServicesGuid;
extern "C" EFI_GUID gEfiEventVirtualAddressChangeGuid;

extern "C" EFI_STATUS EFIAPI UefiUnload(IN EFI_HANDLE ImageHandle)
{
  // This code should be compiled out and never called 

  bug_check(bug_check_codes::kmode_exception_not_handled);
  return EFI_SUCCESS;
}

// You must not call boot services after this event.
static void exit_boot_event_handler(EFI_EVENT event, void* context)
{
  PRINT((__FUNCTION__": ""has been called.\n"));
  gBS->CloseEvent(exit_boot_services_event);
  globals::boot_state = false;
}

// Convert host addresses to guest for hooking purposes in the future.
static void set_virtual_address_map_event_handler(EFI_EVENT event, void* context)
{
  PRINT((__FUNCTION__": ""has been called.\n"));
  PRINT(("Pointers before:\n"));

  PRINT(("ImageBase = 0x%llx\n", globals::win_driver_struct->image_base_virtual_address));
  PRINT(("MemPool address = 0x%llx\n", globals::win_driver_struct->mem_pool_for_allocator_virtual_address));

  if (EFI_ERROR(gRT->ConvertPointer(0, &globals::win_driver_struct->image_base_virtual_address)) ||
    EFI_ERROR(gRT->ConvertPointer(0, &globals::win_driver_struct->mem_pool_for_allocator_virtual_address)))
  {
    PRINT((__FUNCTION__": ""failed.\n"));
    bug_check(bug_check_codes::kmode_exception_not_handled);
  }

  PRINT(("Pointers after:\n"));
  PRINT(("ImageBase = 0x%llx\n", globals::win_driver_struct->image_base_virtual_address));
  PRINT(("MemPool address = 0x%llx\n", globals::win_driver_struct->mem_pool_for_allocator_virtual_address));

  virtual_notify_event = nullptr;
}

// We use avx memset in some places.
extern "C" void enable_avx();

extern "C" EFI_STATUS EFIAPI UefiMain(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE * SystemTable)
{
  enable_avx();
  __crt_init();

  PRINT(("ImageBase of our application is 0x%llx\n", &globals::__ImageBase));

  gBS->CreateEventEx(EVT_NOTIFY_SIGNAL,
    TPL_NOTIFY,
    set_virtual_address_map_event_handler,
    nullptr,
    &gEfiEventVirtualAddressChangeGuid,
    &virtual_notify_event);

  gBS->CreateEventEx(EVT_NOTIFY_SIGNAL,
    TPL_NOTIFY,
    exit_boot_event_handler,
    nullptr,
    &gEfiEventExitBootServicesGuid,
    &exit_boot_services_event);

  try
  {
    win_driver::initialize_memory_for_win_driver();
    hv_operations::initialize_hypervisor();
  }
  catch (std::exception& e)
  {
    PRINT(("Exception occured during hypervisor initialization. %a\n", e.what()));
    bug_check(bug_check_codes::vmx_error);
  }

  return EFI_SUCCESS;
}
