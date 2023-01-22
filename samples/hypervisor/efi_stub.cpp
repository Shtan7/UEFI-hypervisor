#include "uefi.hpp"
#include "efi_stub.hpp"
#include <cstdint>
#include <string>
#include "common.hpp"
#include "globals.hpp"
#include "enum_to_str.hpp"

namespace hh
{
  // Allocate page aligned memory from UEFI heap.
  extern "C" uint64_t allocate_pages_from_uefi_pool(uint64_t number_of_pages)
  {
    uint64_t allocated_address;

    if (EFI_ERROR(gBS->AllocatePages(AllocateAnyPages, EfiRuntimeServicesData, number_of_pages, &allocated_address)))
    {
      throw std::exception{ "Cannot allocate pages for win driver pool." };
    }

    for (size_t j = 0; j < number_of_pages * 0x1000; j++)
    {
      reinterpret_cast<uint8_t*>(allocated_address)[j] = 0xf5;
    }

    return allocated_address;
  }

  // Our main way to stop execution after critical error.
  void bug_check(const bug_check_codes code, const uint64_t arg0, const uint64_t arg1,
    const uint64_t arg2, const uint64_t arg3) noexcept
  {
    PRINT(("Critical error has been ocurred. Bugcheck code: 0x%llx, string presentation: %a\n"
      "bugcheck args: 0x%llx, 0x%llx, 0x%llx, 0x%llx\n",
      code, enum_to_str(code).data(), arg0, arg1, arg2, arg3));

    PANIC();
  }
}
