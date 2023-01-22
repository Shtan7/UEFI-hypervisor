#include <ntddk.h>
#include <string>
#include "hooking.hpp"
#include "common.hpp"

namespace hh
{
  NTSTATUS NtCreateFile(
    PHANDLE FileHandle, ACCESS_MASK DesiredAccess,
    POBJECT_ATTRIBUTES ObjectAttributes, PIO_STATUS_BLOCK IoStatusBlock,
    PLARGE_INTEGER AllocationSize, ULONG FileAttributes,
    ULONG ShareAccess, ULONG CreateDisposition,
    ULONG CreateOptions, PVOID EaBuffer, ULONG EaLength) noexcept
  {
    try
    {
      // it throws if address doesn't map physical memory
      common::get_physical_address_for_virtual_address_by_cr3(x86::read<x86::cr3_t>(), ObjectAttributes->ObjectName->Buffer);
      common::get_physical_address_for_virtual_address_by_cr3(x86::read<x86::cr3_t>(), ObjectAttributes->ObjectName->Buffer
        + ObjectAttributes->ObjectName->Length);

      if (const auto file_name = std::wstring_view(ObjectAttributes->ObjectName->Buffer, ObjectAttributes->ObjectName->Length);
        file_name.contains(L"you_cant_open_me"))
      {
        return STATUS_ACCESS_DENIED;
      }
    }
    catch(...)
    {
      PRINT(("Unrecheable address exception has been triggered.\n"));
    }

    return hook::pointers::NtCreateFileOrig(FileHandle, DesiredAccess, ObjectAttributes, IoStatusBlock, AllocationSize, FileAttributes, ShareAccess,
      CreateDisposition, CreateOptions, EaBuffer, EaLength);
  }
}
