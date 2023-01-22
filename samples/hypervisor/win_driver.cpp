#include "win_driver.hpp"
#include "pe.hpp"
#include "globals.hpp"
#include "pt_handler.hpp"
#include "vcpu.hpp"
#include "common.hpp"

// Can't include headers directly because EDK2 and win headers conflict.
extern "C" uint64_t allocate_pages_from_uefi_pool(uint64_t number_of_pages);

namespace hh::win_driver
{
  uint64_t load_image_from_memory(uint64_t ntoskrnl_base, vcpu* cpu_obj)
  {
    const IMAGE_NT_HEADERS* nt_headers = portable_executable::get_nt_headers(win_driver_raw);

    memcpy(globals::win_driver_struct->image_base_physical_address, win_driver_raw, nt_headers->OptionalHeader.SizeOfHeaders);

    const IMAGE_SECTION_HEADER* current_image_section = IMAGE_FIRST_SECTION(nt_headers);

    for (size_t j = 0; j < nt_headers->FileHeader.NumberOfSections; j++)
    {
      auto section = reinterpret_cast<void*>(reinterpret_cast<uint64_t>(globals::win_driver_struct->image_base_physical_address)
        + current_image_section[j].VirtualAddress);
      memcpy(section, win_driver_raw + current_image_section[j].PointerToRawData, current_image_section[j].SizeOfRawData);
    }

    portable_executable::relocate_image_by_delta(portable_executable::get_relocs(globals::win_driver_struct->image_base_physical_address),
      reinterpret_cast<uint64_t>(globals::win_driver_struct->image_base_virtual_address) - nt_headers->OptionalHeader.ImageBase);

    portable_executable::resolve_imports(ntoskrnl_base,
      portable_executable::get_imports(globals::win_driver_struct->image_base_physical_address));

    uint64_t win_driver_entry_point = reinterpret_cast<uint64_t>(globals::win_driver_struct->image_base_virtual_address)
      + nt_headers->OptionalHeader.AddressOfEntryPoint;;

    uint64_t guest_rip = cpu_obj->guest_rip();
    uint64_t guest_rsp = cpu_obj->guest_rsp();
    guest_rsp -= 8;

    auto mapped_address = globals::pt_handler->map_guest_address(cpu_obj->guest_cr3(),
      reinterpret_cast<uint8_t*>(guest_rsp), 8);

    for (size_t j = 0; j < sizeof(guest_rip); j++)
    {
      (*mapped_address)[j] = reinterpret_cast<uint8_t*>(&guest_rip)[j];
    }

    cpu_obj->guest_rsp(guest_rsp);
    cpu_obj->guest_rip(win_driver_entry_point);

    return win_driver_entry_point;
  }

  void initialize_memory_for_win_driver()
  {
    constexpr uint32_t pool_size = 0x1000;
    globals::win_driver_struct = new win_driver_info{};
    uint64_t allocated_address = allocate_pages_from_uefi_pool(pool_size);

    common::memset_256bit(reinterpret_cast<void*>(allocated_address), {}, pool_size * common::page_size / sizeof(__m256));

    globals::win_driver_struct->mem_pool_for_allocator_physical_address = reinterpret_cast<uint8_t*>(allocated_address);
    globals::win_driver_struct->mem_pool_for_allocator_virtual_address = globals::win_driver_struct->mem_pool_for_allocator_physical_address;
    globals::win_driver_struct->mem_pool_size = pool_size * common::page_size;

    const IMAGE_NT_HEADERS* nt_headers = portable_executable::get_nt_headers(win_driver_raw);
    const uint32_t image_size = nt_headers->OptionalHeader.SizeOfImage;
    const uint32_t image_size_in_pages = image_size / common::page_size + (image_size % common::page_size ? 1 : 0);

    allocated_address = allocate_pages_from_uefi_pool(image_size_in_pages);

    common::memset_256bit(reinterpret_cast<void*>(allocated_address), {}, image_size_in_pages * common::page_size / sizeof(__m256));

    globals::win_driver_struct->image_base_physical_address = reinterpret_cast<uint8_t*>(allocated_address);
    globals::win_driver_struct->image_base_virtual_address = globals::win_driver_struct->image_base_physical_address;
  }
}
