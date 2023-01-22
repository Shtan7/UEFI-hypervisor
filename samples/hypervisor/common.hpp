#pragma once
#include "delete_constructors.hpp"
#include <cstdint>
#include <intrin.h>
#include <memory>
#include <type_traits>
#include "asm.hpp"
#include "x86.hpp"

#define M_LOG2E    1.44269504088896340736   // log2(e)
#define DECLSPEC_ALIGN(x)   __declspec(align(x))
#define PANIC globals::panic_status = true; __halt
#define PAGE_ALIGN(Va) ((void*)((unsigned long long)(Va) & ~(0x1000 - 1)))

#if HH_DEBUG

#define PRINT(_a_) common::print_formatted _a_

#else

#define PRINT(_a_)

#endif


namespace hh
{
  class vcpu;
}

namespace hh::common
{
  using all_cpus_callback = void(*)(void*);

  template <typename T>
  union u64_t
  {
    static_assert(sizeof(T) <= sizeof(uint64_t));
    static_assert(std::is_trivial_v<T>);

    uint64_t as_uint64_t;
    T        as_value;
  };

  enum class status
  {
    hv_success,
    hv_unsuccessful,
  };

  struct fxsave_area
  {
    uint16_t control_word;
    uint16_t status_word;
    uint16_t tag_word;
    uint16_t last_instruction_opcode;

    union
    {
      struct
      {
        uint64_t rip;
        uint64_t rdp;
      };

      struct
      {
        uint32_t ip_offset;
        uint32_t ip_selector;
        uint32_t operand_pointer_offset;
        uint32_t operand_pointer_selector;
      };
    };

    uint32_t mxcsr;
    uint32_t mxcsr_mask;

    __m128   st_register[8];    // st0-st7 (mm0-mm7), only 80 bits per register
    // (upper 48 bits of each register are reserved)

    __m128   xmm_register[16];  // xmm0-xmm15
    uint8_t  reserved_1[48];

    union
    {
      uint8_t reserved_2[48];
      uint8_t software_available[48];
    };
  };

  struct guest_regs
  {
    fxsave_area* fx_area;
    uint64_t rax;                  // 0x00         
    uint64_t rcx;
    uint64_t rdx;                  // 0x10
    uint64_t rbx;
    uint64_t rsp;                  // 0x20         // rsp is not stored here
    uint64_t rbp;
    uint64_t rsi;                  // 0x30
    uint64_t rdi;
    uint64_t r8;                   // 0x40
    uint64_t r9;
    uint64_t r10;                  // 0x50
    uint64_t r11;
    uint64_t r12;                  // 0x60
    uint64_t r13;
    uint64_t r14;                  // 0x70
    uint64_t r15;
  };

  struct cpuid_eax_01
  {
    union
    {
      struct
      {
        uint32_t cpu_info[4];
      };

      struct
      {
        uint32_t eax;
        uint32_t ebx;
        uint32_t ecx;
        uint32_t edx;
      };

      struct
      {
        union
        {
          uint32_t flags;

          struct
          {
            uint32_t stepping_id : 4;
            uint32_t model : 4;
            uint32_t family_id : 4;
            uint32_t processor_type : 2;
            uint32_t reserved1 : 2;
            uint32_t extended_model_id : 4;
            uint32_t extended_family_id : 8;
            uint32_t reserved2 : 4;
          };
        } version_information;

        union
        {
          uint32_t flags;

          struct
          {
            uint32_t brand_index : 8;
            uint32_t clflush_line_size : 8;
            uint32_t max_addressable_ids : 8;
            uint32_t initial_apic_id : 8;
          };
        } additional_information;

        union
        {
          uint32_t flags;

          struct
          {
            uint32_t streaming_simd_extensions_3 : 1;
            uint32_t pclmulqdq_instruction : 1;
            uint32_t ds_area_64bit_layout : 1;
            uint32_t monitor_mwait_instruction : 1;
            uint32_t cpl_qualified_debug_store : 1;
            uint32_t virtual_machine_extensions : 1;
            uint32_t safer_mode_extensions : 1;
            uint32_t enhanced_intel_speedstep_technology : 1;
            uint32_t thermal_monitor_2 : 1;
            uint32_t supplemental_streaming_simd_extensions_3 : 1;
            uint32_t l1_context_id : 1;
            uint32_t silicon_debug : 1;
            uint32_t fma_extensions : 1;
            uint32_t cmpxchg16b_instruction : 1;
            uint32_t xtpr_update_control : 1;
            uint32_t perfmon_and_debug_capability : 1;
            uint32_t reserved1 : 1;
            uint32_t process_context_identifiers : 1;
            uint32_t direct_cache_access : 1;
            uint32_t sse41_support : 1;
            uint32_t sse42_support : 1;
            uint32_t x2apic_support : 1;
            uint32_t movbe_instruction : 1;
            uint32_t popcnt_instruction : 1;
            uint32_t tsc_deadline : 1;
            uint32_t aesni_instruction_extensions : 1;
            uint32_t xsave_xrstor_instruction : 1;
            uint32_t osx_save : 1;
            uint32_t avx_support : 1;
            uint32_t half_precision_conversion_instructions : 1;
            uint32_t rdrand_instruction : 1;
            uint32_t hypervisor_present : 1;
          };
        } feature_information_ecx;

        union
        {
          uint32_t flags;

          struct
          {
            uint32_t floating_point_unit_on_chip : 1;
            uint32_t virtual_8086_mode_enhancements : 1;
            uint32_t debugging_extensions : 1;
            uint32_t page_size_extension : 1;
            uint32_t timestamp_counter : 1;
            uint32_t rdmsr_wrmsr_instructions : 1;
            uint32_t physical_address_extension : 1;
            uint32_t machine_check_exception : 1;
            uint32_t cmpxchg8b : 1;
            uint32_t apic_on_chip : 1;
            uint32_t reserved1 : 1;
            uint32_t sysenter_sysexit_instructions : 1;
            uint32_t memory_type_range_registers : 1;
            uint32_t page_global_bit : 1;
            uint32_t machine_check_architecture : 1;
            uint32_t conditional_move_instructions : 1;
            uint32_t page_attribute_table : 1;
            uint32_t page_size_extension_36bit : 1;
            uint32_t processor_serial_number : 1;
            uint32_t clflush : 1;
            uint32_t reserved2 : 1;
            uint32_t debug_store : 1;
            uint32_t thermal_control_msrs_for_acpi : 1;
            uint32_t mmx_support : 1;
            uint32_t fxsave_fxrstor_instructions : 1;
            uint32_t sse_support : 1;
            uint32_t sse2_support : 1;
            uint32_t self_snoop : 1;
            uint32_t hyper_threading_technology : 1;
            uint32_t thermal_monitor : 1;
            uint32_t reserved3 : 1;
            uint32_t pending_break_enable : 1;
          };
        } feature_information_edx;
      };
    };
  };

  union virtual_address
  {
    struct
    {
      uint64_t page_offset : 12;
      uint64_t pt_index : 9;
      uint64_t pd_index : 9;
      uint64_t pdpt_index : 9;
      uint64_t pml4_index : 9;
      uint64_t unused : 16;
    };

    uint64_t all;
  };

  struct cpuid_t
  {
    int eax;
    int ebx;
    int ecx;
    int edx;
  };

  // RAII spinlock.
  class spinlock_guard : non_copyable
  {
  private:
    static constexpr uint32_t max_wait_ = 65536;
    volatile long* lock_;

  private:
    bool try_lock() noexcept;
    void lock_spinlock() noexcept;
    void unlock() noexcept;

  public:
    spinlock_guard(spinlock_guard&&) noexcept;
    spinlock_guard& operator=(spinlock_guard&&) noexcept;
    explicit spinlock_guard(volatile long* lock) noexcept;
    ~spinlock_guard() noexcept;
  };

  constexpr uint64_t compile_time_log2(uint64_t x) noexcept
  {
    return x == 1 ? 0 : 1 + compile_time_log2(x / 2);
  }

  void* memcpy_128bit(void* dest, const void* src, size_t len) noexcept;
  void* memset_256bit(void* dest, const __m256i val, size_t len) noexcept;

  // Get total number of CPUs using UEFI services.
  uint32_t get_active_processors_count();

  // Use it only in boot stage. Usage after exit from boot services is prohibited.
  void run_on_all_processors(all_cpus_callback callback, void* context);
  double log2(double d) noexcept;

  // There is only 1 to 1 virt - phys mapping, so we don't need these functions.
  // But in the future I may add full address conversion at set_virtual_address_map_event_handler and
  // these functions would be needed in that situation.
  void* physical_address_to_virtual_address(uint64_t physical_address);
  uint64_t virtual_address_to_physical_address(void* virtual_address);

  // Get cpu number using UEFI services.
  uint32_t get_current_processor_number();
  void print_formatted(const char* text, ...) noexcept;

  // Set chosen bit.
  void set_bit(void* address, uint64_t bit, bool set) noexcept;

  // Get chosen bit.
  uint8_t get_bit(void* address, uint64_t bit) noexcept;
  uint64_t get_physical_address_for_virtual_address_by_cr3(x86::cr3_t guest_cr3, void* virtual_address);

  inline constexpr uint32_t page_size = 0x1000;
  inline constexpr uint64_t size_2mb = 512 * common::page_size;
  inline constexpr uint64_t size_1gb = size_2mb * 512;
  inline constexpr uint32_t page_shift = 12;
  inline constexpr uint32_t page_shift_2mb = 21;
  inline constexpr uint32_t page_shift_1gb = 30;
  inline constexpr uint64_t page_1gb_offset_mask = (~0ull) >> (64 - page_shift_1gb);
  inline constexpr uint64_t page_2mb_offset_mask = (~0ull) >> (64 - page_shift_2mb);
  inline constexpr uint64_t page_4kb_offset_mask = (~0ull) >> (64 - page_shift);
}
