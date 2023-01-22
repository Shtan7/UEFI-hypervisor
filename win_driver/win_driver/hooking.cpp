#include "hooking.hpp"
#include "common.hpp"
#include "vmcall.hpp"
#include <ntimage.h>

namespace hh::hook
{
  hook_context::self& hook_context::set_cr3(x86::cr3_t new_cr3) noexcept
  {
    dir_base_ = std::make_unique<common::directory_base_guard>(new_cr3);
    return *this;
  }

  hook_context::self& hook_context::set_target_address(void* target_address) noexcept
  {
    target_address_ = static_cast<uint8_t*>(target_address);
    return *this;
  }

  hook_context::self& hook_context::set_read() noexcept
  {
    attributes_.read = 1;
    return *this;
  }

  hook_context::self& hook_context::set_write() noexcept
  {
    attributes_.write = 1;
    return *this;
  }

  hook_context::self& hook_context::set_exec() noexcept
  {
    attributes_.exec = 1;
    return *this;
  }

  hook_context::self& hook_context::set_functions(void* hook_function, void** orig_function) noexcept
  {
    hook_function_ = hook_function;
    orig_function_ = orig_function;

    return *this;
  }

  void hook_builder::write_absolute_jmp(uint8_t* target_buffer, uint64_t where_to_jmp) const noexcept
  {
#pragma warning(push)
#pragma warning(disable : 4309)

    target_buffer[0] = 0xFF;
    target_buffer[1] = 0x25; // jmp qword ptr [where_to_jump]
    int32_t relative_offset = {};
    *reinterpret_cast<int32_t*>(&target_buffer[2]) = relative_offset;
    *reinterpret_cast<uint64_t*>(&target_buffer[6]) = where_to_jmp;

#pragma warning(pop)
  }

  void hook_builder::write_absolute_ret(uint8_t* target_buffer, uint64_t where_to_jmp) const noexcept
  {
#pragma warning(push)
#pragma warning(disable : 4309)

    uint32_t part_1 = (where_to_jmp & 0xFFFFFFFF00000000) >> 32;
    uint32_t part_2 = where_to_jmp & 0x00000000FFFFFFFF;

    target_buffer[0] = 0x48;
    target_buffer[1] = 0x83;
    target_buffer[2] = 0xEC;
    target_buffer[3] = 0x08; // sub rsp, 8
    target_buffer[4] = 0xC7;
    target_buffer[5] = 0x04;
    target_buffer[6] = 0x24; // mov dword ptr [rsp], part_2

    *reinterpret_cast<uint32_t*>(&target_buffer[7]) = part_2; // mov [rsp], dword_part_2

    target_buffer[11] = 0xC7;
    target_buffer[12] = 0x44;
    target_buffer[13] = 0x24;
    target_buffer[14] = 0x04; // mov dwrod ptr [rsp+4], part_1

    *reinterpret_cast<uint32_t*>(&target_buffer[15]) = part_1; // mov [rsp+4], dword_part_1

    target_buffer[19] = 0xC3; // ret

#pragma warning(pop)
  }

  hook_builder::hook_builder() : hooked_pages_list_{}, lde_{}
  {}

  void hook_builder::unhook_all_pages() noexcept
  {
    __vmcall(vmcall_number::unhook_all_pages);
    hooked_pages_list_.clear();
  }

  void hook_builder::unhook_single_page(void* ptr)
  {
    const auto target_page = std::ranges::find(hooked_pages_list_, ptr, &hook::page_details_guest::target_address);

    if (target_page == hooked_pages_list_.end())
    {
      throw std::exception{ "Page to unhook hasn't found." };
    }

    if (__vmcall(vmcall_number::unhook_single_page, reinterpret_cast<uint64_t>(ptr)) != status::hv_success)
    {
      throw std::exception{ "Failed to unhook page." };
    }

    hooked_pages_list_.erase(target_page);
  }

  void hook_builder::hook_instruction_in_memory(hook_context& context)
  {
    static constexpr uint32_t hook_size = 20;

    const common::virtual_address va = { .all = reinterpret_cast<uint64_t>(context.target_address_) };
    const uint32_t size_of_hooked_instructions = lde_.get_instructions_length(context.target_address_, hook_size);

    if(va.page_offset + hook_size > common::page_size - 1)
    {
      throw std::exception{ "Cannot perform ept hook across page boundaries." };
    }

    hooked_pages_list_.back().trampoline = std::make_shared<uint8_t[]>(common::max_trampoline_size);
    memcpy(hooked_pages_list_.back().trampoline.get(), context.target_address_, size_of_hooked_instructions);

    write_absolute_jmp(hooked_pages_list_.back().trampoline.get() + size_of_hooked_instructions,
      reinterpret_cast<uint64_t>(context.target_address_ + size_of_hooked_instructions));

    *context.orig_function_ = hooked_pages_list_.back().trampoline.get();

    write_absolute_ret(&hooked_pages_list_.back().fake_page_contents[va.page_offset], reinterpret_cast<uint64_t>(context.hook_function_));
  }

  void* get_address_by_ssdt(ssdt_numbers ssdt_function_number_, bool is_win32k, void* pe_base)
  {
    static uint64_t nt_table = 0;
    static uint64_t win32k_table = 0;

    const auto ssdt_function_number = static_cast<uint32_t>(ssdt_function_number_);
    const auto* dos_header = reinterpret_cast<const IMAGE_DOS_HEADER*>(pe_base);
    const auto* nt_header = reinterpret_cast<IMAGE_NT_HEADERS*>((reinterpret_cast<uint64_t>(pe_base) + dos_header->e_lfanew));
    char* target_address_of_function = nullptr;

    if (nt_table == 0)
    {
      auto* section = reinterpret_cast<IMAGE_SECTION_HEADER*>(reinterpret_cast<uint64_t>(nt_header) + sizeof(IMAGE_NT_HEADERS));

      for (int j = 0; j < nt_header->FileHeader.NumberOfSections; j++)
      {
        if (section[j].Characteristics & IMAGE_SCN_CNT_CODE
          && section[j].Characteristics & IMAGE_SCN_MEM_EXECUTE
          && !(section[j].Characteristics & IMAGE_SCN_MEM_DISCARDABLE))
        {
          // find address of KiSystemServiceRepeat that contains 'lea r11, KeServiceDescriptorTableShadow'
          target_address_of_function = reinterpret_cast<char*>(
            find_pattern(reinterpret_cast<char*>(section[j].VirtualAddress) + reinterpret_cast<uint64_t>(pe_base),
              section[j].Misc.VirtualSize, patterns::ssdt_shadow_table.pattern, patterns::ssdt_shadow_table.mask));

          if (target_address_of_function != nullptr)
          {
            break;
          }
        }
      }

      if (target_address_of_function == nullptr)
      {
        throw std::exception{ "Cannot find address of ssdt table." };
      }

      target_address_of_function += 0x3; // offset to relative address from 'lea r11, KeServiceDescriptorTableShadow'
      long relative_offset = *reinterpret_cast<long*>(target_address_of_function);
      target_address_of_function += relative_offset; // add rip relative offset
      target_address_of_function += 4; // rip relative offset computes from the end of opcode

      nt_table = reinterpret_cast<uint64_t>(target_address_of_function);
      win32k_table = nt_table + 0x20;  // currently offset is 0x20. You can find it with windbg and 'x win32k!W32pServiceTable' command. It shows current table address and
      // if you look at 'x nt!KeServiceDescriptorTableShadow' you see W32pServiceTable address below.

      /* ATTENTION! Your signatures and offsets may differ or may not. You must obtain your signatures with your
        Windows version if you want to use this code. */
    }

    const SSDTStruct* ssdt = reinterpret_cast<SSDTStruct*>(is_win32k ? win32k_table : nt_table);
    const uint64_t ssdt_base = reinterpret_cast<uint64_t>(ssdt->pServiceTable);

    if (ssdt_base == 0)
    {
      throw std::exception{ "SSDT data corrupted." };
    }

    target_address_of_function = reinterpret_cast<char*>((ssdt->pServiceTable[ssdt_function_number] >> 4) + ssdt_base);

    if (target_address_of_function == nullptr)
    {
      throw std::exception{ "Got nullptr from SSDT entry." };
    }

    return target_address_of_function;
  }

  void* find_pattern(void* start_address, uint64_t size_of_scan_section, std::string_view pattern, std::string_view mask) noexcept
  {
    for (uint32_t i = 0; i < size_of_scan_section; i++)
    {
      bool found = true;

      for (uint32_t j = 0; j < mask.size(); j++)
      {
        found &= (mask[j] == '?' || pattern[j] == *(reinterpret_cast<const char*>(start_address) + j + i));
      }

      if (found)
      {
        return reinterpret_cast<char*>(start_address) + i;
      }
    }

    return nullptr;
  }
}
