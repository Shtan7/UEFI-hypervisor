#include "common.hpp"
#include <intrin.h>

#include "pt.hpp"
#include "uefi.hpp"

extern "C"
{
#include <Pi/PiMultiPhase.h>
#include <Protocol/MpService.h>
#include <Uefi/UefiBaseType.h>
#include <Library/PrintLib.h>
#include <Library/SerialPortLib.h>
}

#include <exception>
#include <cstdarg>
#include "globals.hpp"
#include "vmx.hpp"
#include "vcpu.hpp"

// Set - Unset bit
#define BITS_PER_LONG (sizeof(UINT32) * 8)
#define BITMAP_ENTRY(nr, bmap) ((bmap))[(nr) / BITS_PER_LONG]
#define BITMAP_SHIFT(nr) ((nr) % BITS_PER_LONG)

namespace hh::common
{
  constexpr uint32_t max_debug_message_size = 0x1000;

  spinlock_guard::spinlock_guard(volatile long* lock) noexcept : lock_{ lock }
  {
    lock_spinlock();
  }

  spinlock_guard::~spinlock_guard() noexcept
  {
    if (lock_ != nullptr)
    {
      unlock();
    }
  }

  bool spinlock_guard::try_lock() noexcept
  {
    return (!(*lock_) && !_interlockedbittestandset(lock_, 0));
  }

  void spinlock_guard::unlock() noexcept
  {
    *lock_ = 0;
  }

  spinlock_guard::spinlock_guard(spinlock_guard&& obj) noexcept
  {
    lock_ = obj.lock_;
    obj.lock_ = nullptr;
  }

  spinlock_guard& spinlock_guard::operator=(spinlock_guard&& obj) noexcept
  {
    if (lock_ != nullptr)
    {
      unlock();
    }

    lock_ = obj.lock_;
    obj.lock_ = nullptr;

    return *this;
  }

  void spinlock_guard::lock_spinlock() noexcept
  {
    uint32_t wait = 1;

    while (!try_lock())
    {
      for (uint32_t j = 0; j < wait; j++)
      {
        _mm_pause();
      }

      if (wait * 2 > max_wait_)
      {
        wait = max_wait_;
      }
      else
      {
        wait *= 2;
      }
    }
  }

  void* memcpy_128bit(void* dest, const void* src, size_t len) noexcept
  {
    const auto* s = static_cast<const __m128i*>(src);
    auto d = static_cast<__m128i*>(dest);

    while (len--)
    {
      _mm_storeu_si128(d++, _mm_lddqu_si128(s++));
    }

    return dest;
  }

  void* memset_256bit(void* dest, const __m256i val, size_t len) noexcept
  {
    auto ptr = static_cast<__m256i*>(dest);

    while (len--)
    {
      _mm256_storeu_si256(ptr++, val);
    }

    return dest;
  }

  uint32_t get_active_processors_count()
  {
    if (globals::gEfiMpServiceProtocol == nullptr)
    {
      return 1;
    }

    uint64_t number_of_processors;
    uint64_t number_of_enabled_processors;

    auto status = globals::gEfiMpServiceProtocol->GetNumberOfProcessors(globals::gEfiMpServiceProtocol,
      &number_of_processors,
      &number_of_enabled_processors);

    if (EFI_ERROR(status))
    {
      throw std::exception{ __FUNCTION__": ""Failed to get number of processors." };
    }

    return number_of_enabled_processors;
  }

  void run_on_all_processors(all_cpus_callback callback, void* context)
  {
    callback(context);

    if (get_active_processors_count() == 1)
    {
      return;
    }

    auto status = globals::gEfiMpServiceProtocol->StartupAllAPs(globals::gEfiMpServiceProtocol,
      callback,
      true,
      nullptr,
      0,
      context,
      nullptr);

    if (EFI_ERROR(status))
    {
      throw std::exception{ __FUNCTION__": ""StartupAllAPs failed." };
    }
  }

  double log2(double d) noexcept
  {
    return log(d) * M_LOG2E;
  }

  void* physical_address_to_virtual_address(uint64_t physical_address)
  {
    // 1 to 1 mapping
    return reinterpret_cast<void*>(physical_address);
  }

  uint64_t virtual_address_to_physical_address(void* virtual_address)
  {
    // 1 to 1 mapping
    return reinterpret_cast<uint64_t>(virtual_address);
  }

  uint32_t get_current_processor_number()
  {
    if (globals::gEfiMpServiceProtocol == nullptr)
    {
      // there is only one core
      return 0;
    }

    uint64_t result = {};

    auto status = globals::gEfiMpServiceProtocol->WhoAmI(globals::gEfiMpServiceProtocol, &result);

    if (EFI_ERROR(status))
    {
      throw std::exception{ __FUNCTION__": ""WhoAmI failed." };
    }

    return result;
  }

  void print_formatted(const char* text, ...) noexcept
  {
    static volatile long print_lock = {};

    common::spinlock_guard _{ &print_lock };
    static char buff[max_debug_message_size];
    va_list args;

    va_start(args, text);
    uint32_t bytes_to_write = AsciiVSPrint(buff, max_debug_message_size - 0x50, text, args);
    SerialPortWrite(reinterpret_cast<uint8_t*>(buff), bytes_to_write);
    va_end(args);
  }

  void set_bit(void* address, uint64_t bit, bool set) noexcept
  {
    if (set)
    {
      BITMAP_ENTRY(bit, static_cast<uint32_t*>(address)) |= (1UL << BITMAP_SHIFT(bit));
    }
    else
    {
      BITMAP_ENTRY(bit, static_cast<uint32_t*>(address)) &= ~(1UL << BITMAP_SHIFT(bit));
    }
  }

  uint8_t get_bit(void* address, uint64_t bit) noexcept
  {
    uint64_t byte, k;

    byte = bit / 8;
    k = 7 - bit % 8;

    return static_cast<uint8_t*>(address)[byte] & (1 << k);
  }

  uint64_t get_physical_address_for_virtual_address_by_cr3(x86::cr3_t guest_cr3, void* virtual_address_guest)
  {
    virtual_address va_parts = { .all = reinterpret_cast<uint64_t>(virtual_address_guest) };
    uint64_t final_ptr;

    do
    {
      const auto pml4 = static_cast<pt::page_entry*>
        (physical_address_to_virtual_address(guest_cr3.flags.page_frame_number << page_shift));

      if (!pml4[va_parts.pml4_index].fields.present)
      {
        PRINT(("address = 0x%llx\n", virtual_address_guest));
        throw std::exception{ __FUNCTION__": ""PML4 isn't present." };
      }

      const auto pml3 = static_cast<pt::page_entry*>
        (physical_address_to_virtual_address(pml4[va_parts.pml4_index].fields.page_frame_number << page_shift));

      if (!pml3[va_parts.pdpt_index].fields.present)
      {
        throw std::exception{ __FUNCTION__": ""PML3 isn't present." };
      }

      if (pml3[va_parts.pdpt_index].fields.large_page)
      {
        final_ptr = reinterpret_cast<uint64_t>(physical_address_to_virtual_address
        (pml3[va_parts.pdpt_index].pdpt_large.page_frame_number << page_shift_1gb));

        final_ptr += va_parts.all & page_1gb_offset_mask;

        break;
      }

      const auto pml2 = static_cast<pt::page_entry*>
        (physical_address_to_virtual_address(pml3[va_parts.pdpt_index].fields.page_frame_number << page_shift));

      if (!pml2[va_parts.pd_index].pd.present)
      {
        throw std::exception{ __FUNCTION__": ""PML2 isn't present." };
      }

      if (pml2[va_parts.pd_index].pd.large_page)
      {
        final_ptr = reinterpret_cast<uint64_t>(physical_address_to_virtual_address
        (pml2[va_parts.pd_index].pd_large.page_frame_number << page_shift_2mb));

        final_ptr += va_parts.all & page_2mb_offset_mask;

        break;
      }

      const auto pml1 = static_cast<pt::page_entry*>
        (physical_address_to_virtual_address(pml2[va_parts.pd_index].fields.page_frame_number << page_shift));

      if (!pml1[va_parts.pt_index].fields.present)
      {
        throw std::exception{ __FUNCTION__": ""PML1 isn't present." };
      }

      final_ptr = reinterpret_cast<uint64_t>(physical_address_to_virtual_address(pml1[va_parts.pt_index].fields.page_frame_number << page_shift));
      final_ptr += va_parts.page_offset;

    } while (false);

    return final_ptr;
  }
}
