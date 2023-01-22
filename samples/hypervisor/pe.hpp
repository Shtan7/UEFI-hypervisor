#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <Windows.h>

// Tools for mapping and parsing windows drivers and executables.

namespace hh::portable_executable
{
  struct reloc_info
  {
    uint64_t address;
    uint16_t* item;
    uint32_t count;
  };

  struct import_function_info
  {
    std::string name;
    uint64_t* address;
  };

  struct import_info
  {
    std::string module_name;
    std::vector<import_function_info> function_datas;
  };

  using vec_sections = std::vector<IMAGE_SECTION_HEADER>;
  using vec_relocs = std::vector<reloc_info>;
  using vec_imports = std::vector<import_info>;

  PIMAGE_NT_HEADERS64 get_nt_headers(void* image_base) noexcept;
  vec_relocs get_relocs(void* image_base);
  vec_imports get_imports(void* image_base);
  void relocate_image_by_delta(const vec_relocs& relocs, uint64_t delta) noexcept;
  void resolve_imports(uint64_t ntoskrnl_base, const vec_imports& imports);
  uint64_t get_kernel_module_export(uint64_t kernel_module_base, std::string_view function_name);
}
