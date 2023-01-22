#include "pe.hpp"
#include <memory>
#include <ranges>
#include "pt_handler.hpp"
#include "globals.hpp"
#include <algorithm>
#include "vcpu.hpp"
#include "per_cpu_data.hpp"

namespace hh::portable_executable
{
  static bool iequals(const std::string_view& lhs, const std::string_view& rhs)
  {
    auto to_lower{ std::ranges::views::transform(std::tolower) };
    return std::ranges::equal(lhs | to_lower, rhs | to_lower);
  }

  IMAGE_NT_HEADERS64* get_nt_headers(void* image_base) noexcept
  {
    const auto dos_header = static_cast<IMAGE_DOS_HEADER*>(image_base);

    if (dos_header->e_magic != IMAGE_DOS_SIGNATURE)
      return nullptr;

    const auto nt_headers = reinterpret_cast<IMAGE_NT_HEADERS64*>(reinterpret_cast<uint64_t>(image_base) + dos_header->e_lfanew);

    if (nt_headers->Signature != IMAGE_NT_SIGNATURE)
      return nullptr;

    return nt_headers;
  }

  vec_relocs get_relocs(void* image_base)
  {
    const PIMAGE_NT_HEADERS64 nt_headers = get_nt_headers(image_base);

    if (!nt_headers)
      return {};

    vec_relocs relocs;
    uint32_t reloc_va = nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress;

    if (!reloc_va)
      return {};

    auto current_base_relocation = reinterpret_cast<PIMAGE_BASE_RELOCATION>(reinterpret_cast<uint64_t>(image_base) + reloc_va);
    const auto reloc_end = reinterpret_cast<uint64_t>(current_base_relocation)
    + nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size;

    while (current_base_relocation->VirtualAddress && current_base_relocation->VirtualAddress < reloc_end && current_base_relocation->SizeOfBlock)
    {
      reloc_info reloc_info;

      reloc_info.address = reinterpret_cast<uint64_t>(image_base) + current_base_relocation->VirtualAddress;
      reloc_info.item = reinterpret_cast<uint16_t*>(reinterpret_cast<uint64_t>(current_base_relocation) + sizeof(IMAGE_BASE_RELOCATION));
      reloc_info.count = (current_base_relocation->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(uint16_t);

      relocs.push_back(reloc_info);

      current_base_relocation = reinterpret_cast<PIMAGE_BASE_RELOCATION>(reinterpret_cast<uint64_t>(current_base_relocation)
        + current_base_relocation->SizeOfBlock);
    }

    return relocs;
  }

  vec_imports get_imports(void* image_base)
  {
    const IMAGE_NT_HEADERS64* nt_headers = get_nt_headers(image_base);

    if (!nt_headers)
      return {};

    uint32_t import_va = nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;

    //not imports necesary
    if (!import_va)
      return {};

    vec_imports imports;

    auto current_import_descriptor = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(reinterpret_cast<uint64_t>(image_base) + import_va);

    while (current_import_descriptor->FirstThunk)
    {
      import_info import_info;

      import_info.module_name = std::string{ reinterpret_cast<char*>(reinterpret_cast<uint64_t>(image_base) + current_import_descriptor->Name) };

      auto current_first_thunk = reinterpret_cast<IMAGE_THUNK_DATA64*>(reinterpret_cast<uint64_t>(image_base)
        + current_import_descriptor->FirstThunk);
      auto current_original_first_thunk = reinterpret_cast<IMAGE_THUNK_DATA64*>(reinterpret_cast<uint64_t>(image_base)
        + current_import_descriptor->OriginalFirstThunk);

      while (current_original_first_thunk->u1.Function)
      {
        import_function_info import_function_data;

        auto thunk_data = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(reinterpret_cast<uint64_t>(image_base) + current_original_first_thunk->u1.AddressOfData);

        import_function_data.name = thunk_data->Name;
        import_function_data.address = &current_first_thunk->u1.Function;

        import_info.function_datas.push_back(import_function_data);

        ++current_original_first_thunk;
        ++current_first_thunk;
      }

      imports.push_back(import_info);
      ++current_import_descriptor;
    }

    return imports;
  }

  void relocate_image_by_delta(const vec_relocs& relocs, uint64_t delta) noexcept
  {
    for (const auto& current_reloc : relocs)
    {
      for (auto i = 0u; i < current_reloc.count; ++i)
      {
        const uint16_t type = current_reloc.item[i] >> 12;
        const uint16_t offset = current_reloc.item[i] & 0xFFF;

        if (type == IMAGE_REL_BASED_DIR64)
          *reinterpret_cast<uint64_t*>(current_reloc.address + offset) += delta;
      }
    }
  }

  uint64_t get_kernel_module_export(uint64_t kernel_module_base, std::string_view function_name)
  {
    if (!kernel_module_base)
      return 0;

    IMAGE_DOS_HEADER dos_header = {};
    IMAGE_NT_HEADERS64 nt_headers = {};

    const auto mapped_dos_header =
      globals::pt_handler->map_guest_address(globals::vcpus[per_cpu_data::get_cpu_id()].guest_cr3(),
      reinterpret_cast<uint8_t*>(kernel_module_base), sizeof(dos_header));
    mapped_dos_header->memcpy(&dos_header, 0, sizeof(dos_header));

    if(dos_header.e_magic != IMAGE_DOS_SIGNATURE)
    {
      return 0;
    }

    const auto mapped_nt_headers =
      globals::pt_handler->map_guest_address(globals::vcpus[per_cpu_data::get_cpu_id()].guest_cr3(),
      reinterpret_cast<uint8_t*>(kernel_module_base + dos_header.e_lfanew), sizeof(nt_headers));
    mapped_nt_headers->memcpy(&nt_headers, 0, sizeof(nt_headers));

    if(nt_headers.Signature != IMAGE_NT_SIGNATURE)
    {
      return 0;
    }

    const auto export_base = nt_headers.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
    const auto export_base_size = nt_headers.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size;

    if (!export_base || !export_base_size)
      return 0;

    std::unique_ptr<IMAGE_EXPORT_DIRECTORY> export_data{ reinterpret_cast<IMAGE_EXPORT_DIRECTORY*>(new uint8_t[export_base_size]) };

    const auto mapped_export_data =
      globals::pt_handler->map_guest_address(globals::vcpus[per_cpu_data::get_cpu_id()].guest_cr3(),
      reinterpret_cast<uint8_t*>(kernel_module_base + export_base), export_base_size);
    mapped_export_data->memcpy(export_data.get(), 0, export_base_size);

    const auto delta = reinterpret_cast<uint64_t>(export_data.get()) - export_base;
    const auto name_table = reinterpret_cast<uint32_t*>(export_data->AddressOfNames + delta);
    const auto ordinal_table = reinterpret_cast<uint16_t*>(export_data->AddressOfNameOrdinals + delta);
    const auto function_table = reinterpret_cast<uint32_t*>(export_data->AddressOfFunctions + delta);

    for (auto i = 0u; i < export_data->NumberOfNames; ++i)
    {
      const std::string current_function_name = std::string{ reinterpret_cast<char*>(name_table[i] + delta) };

      if (iequals(current_function_name, function_name))
      {
        const auto function_ordinal = ordinal_table[i];
        const auto function_address = kernel_module_base + function_table[function_ordinal];

        if (function_address >= kernel_module_base + export_base && function_address <= kernel_module_base + export_base + export_base_size)
        {
          return 0; // No forwarded exports on 64bit?
        }

        return function_address;
      }
    }

    return 0;
  }

  void resolve_imports(uint64_t ntoskrnl_base, const vec_imports& imports)
  {
    for (const auto& current_import : imports)
    {
      if (current_import.module_name != "ntoskrnl.exe")
      {
        PRINT(("Unacceptable module name: %a\n", current_import.module_name.c_str()));
        throw std::exception{ __FUNCTION__": ""Inaccessible import module. We can only get export from ntoskrnl.exe." };
      }

      for (auto& current_function_data : current_import.function_datas)
      {
        const uint64_t function_address = get_kernel_module_export(ntoskrnl_base, current_function_data.name);

        if (!function_address)
        {
          PRINT(("Can't resolve import: %a\n", current_function_data.name.c_str()));
          throw std::exception{ __FUNCTION__": ""Failed to resolve import." };
        }

        *current_function_data.address = function_address;
      }
    }
  }
}
