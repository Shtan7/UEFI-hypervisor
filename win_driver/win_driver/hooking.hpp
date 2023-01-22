#pragma once
#include <ntddk.h>
#include "lde.hpp"
#include "common.hpp"
#include <cstdint>
#include <list>
#include <memory>
#include <string>
#include "../../samples/hypervisor/delete_constructors.hpp"
#include "vmcall.hpp"

namespace hh
{
  namespace x86
  {
    struct cr3_t;
  }

  namespace hook
  {
    struct SSDTStruct
    {
      LONG* pServiceTable;
      PVOID pCounterTable;
      ULONGLONG NumberOfServices;
      PCHAR pArgumentTable;
    };

    union page_attribs
    {
      struct
      {
        uint8_t read : 1;
        uint8_t write : 1;
        uint8_t exec : 1;
      };

      uint8_t all;
    };

    struct page_details_guest
    {
      void* target_address;
      std::shared_ptr<uint8_t[]> fake_page_contents;
      std::shared_ptr<uint8_t[]> trampoline;
      page_attribs attributes;
    };

    struct pattern_entry
    {
      std::string_view pattern;
      std::string_view mask;
    };

    struct guest_hook_request_info
    {
      void* target_page_address;
      void* hooked_page_address;
      x86::cr3_t target_cr3;
      page_attribs required_attributes;
    };

    namespace patterns
    {
      inline constexpr pattern_entry ssdt_shadow_table{ "\x4C\x8D\x1D\x00\x00\x00\x00\xF7\x43\x00\x00\x00\x00\x00", "xxx????xx?????" };
    }

    namespace pointers
    {
      extern "C" inline NTSTATUS(*NtCreateFileOrig)(
        PHANDLE FileHandle, ACCESS_MASK DesiredAccess,
        POBJECT_ATTRIBUTES ObjectAttributes, PIO_STATUS_BLOCK IoStatusBlock,
        PLARGE_INTEGER AllocationSize, ULONG FileAttributes,
        ULONG ShareAccess, ULONG CreateDisposition,
        ULONG CreateOptions, PVOID EaBuffer,
        ULONG EaLength) = nullptr;
    }

    enum class ssdt_numbers : uint32_t
    {
      NtCreateFile = 0x55,
    };

    // Auxiliary class that provides hook information for the hook builder
    class hook_context : non_copyable
    {
      using self = hook_context;
      friend class hook_builder;

    private:
      std::unique_ptr<common::directory_base_guard> dir_base_;
      uint8_t* target_address_;
      void* hook_function_;
      void** orig_function_;
      page_attribs attributes_;

    public:
      hook_context() noexcept = default;
      hook_context(hook_context&&) noexcept = default;
      self& operator=(hook_context&&) noexcept = default;
      virtual ~hook_context() noexcept = default;
      self& set_cr3(x86::cr3_t new_cr3) noexcept;
      self& set_target_address(void* target_address) noexcept;
      self& set_read() noexcept;
      self& set_write() noexcept;
      self& set_exec() noexcept;
      self& set_functions(void* hook_function, void** orig_function) noexcept;
    };

    // Class setups hook trampoline and fake page with hook
    class hook_builder : non_relocatable
    {
    private:
      std::list<hook::page_details_guest> hooked_pages_list_;
      disassembler lde_;

    private:
      void write_absolute_jmp(uint8_t* target_buffer, uint64_t where_to_jmp) const noexcept;
      void write_absolute_ret(uint8_t* target_buffer, uint64_t where_to_jmp) const noexcept;
      void hook_instruction_in_memory(hook_context& context);

    public:
      hook_builder();
      void unhook_all_pages() noexcept;
      void unhook_single_page(void* ptr);
      void ept_hook(auto&& context);
    };

    void hook_builder::ept_hook(auto&& context)
    {
      const uint32_t pages_number_before = hooked_pages_list_.size();

      try
      {
        if (context.attributes_.all == 0)
        {
          throw std::exception{ "Incorrect page hook mask." };
        }

        hooked_pages_list_.emplace_back();
        void* target_page_va = PAGE_ALIGN(context.target_address_);
        hooked_pages_list_.back().target_address = target_page_va;
        hooked_pages_list_.back().fake_page_contents
          = std::shared_ptr<uint8_t[]>{ new (std::align_val_t{common::page_size}) uint8_t[common::page_size] };

        RtlCopyBytes(hooked_pages_list_.back().fake_page_contents.get(), target_page_va, common::page_size);
        hook_instruction_in_memory(context);

        guest_hook_request_info info = {};
        info.target_page_address = target_page_va;
        info.hooked_page_address = hooked_pages_list_.back().fake_page_contents.get();
        info.target_cr3 = x86::cr3_t{ x86::read<x86::cr3_t>() };
        info.required_attributes = context.attributes_;

        if (__vmcall(vmcall_number::change_page_attrib, reinterpret_cast<uint64_t>(&info)) != status::hv_success)
        {
          throw std::exception{ "Failed to change page attribs." };
        }
      }
      catch (std::exception& e)
      {
        PRINT((__FUNCTION__": ""failed. %s\n", e.what()));

        if (pages_number_before != hooked_pages_list_.size())
        {
          hooked_pages_list_.pop_back();
        }
      }
    }

    // Get address of kernel function by SSDT.
    void* get_address_by_ssdt(ssdt_numbers ssdt_function_number, bool is_win32k, void* pe_base);
    // Find address of signature.
    void* find_pattern(void* start_address, uint64_t size_of_scan_section, std::string_view pattern, std::string_view mask) noexcept;
  }
}
