#include "common.hpp"
#include <cstdarg>
#include <cstdint>
#include <intrin.h>
#include <ntddk.h>
#include "printf.hpp"
#include <exception>
#include "pt.hpp"

namespace hh::common
{
  spinlock_guard::spinlock_guard(volatile long* lock) noexcept : lock_{ lock }
  {
    lock_spinlock();
  }

  spinlock_guard::~spinlock_guard() noexcept
  {
    unlock();
  }

  bool spinlock_guard::try_lock() noexcept
  {
    return (!(*lock_) && !_interlockedbittestandset(lock_, 0));
  }

  void spinlock_guard::unlock() noexcept
  {
    *lock_ = 0;
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

  directory_base_guard::directory_base_guard(x86::cr3_t new_cr3) noexcept : old_cr3_{ x86::read<x86::cr3_t>() }
  {
    x86::write<x86::cr3_t>(new_cr3);
  }

  directory_base_guard::~directory_base_guard() noexcept
  {
    x86::write<x86::cr3_t>(old_cr3_);
  }

  uint64_t virtual_address_to_physical_address(void* virtual_address) noexcept
  {
    return MmGetPhysicalAddress(virtual_address).QuadPart;
  }

  uint64_t physical_address_to_virtual_address(uint64_t physical_address) noexcept
  {
    PHYSICAL_ADDRESS physical_addr{ .QuadPart = static_cast<long long>(physical_address) };

    return reinterpret_cast<uint64_t>(MmGetVirtualForPhysical(physical_addr));
  }

#define BAUD_LOW_OFFSET    0x00
#define BAUD_HIGH_OFFSET   0x01
#define IER_OFFSET         0x01
#define LCR_SHADOW_OFFSET  0x01
#define FCR_SHADOW_OFFSET  0x02
#define IR_CONTROL_OFFSET  0x02
#define FCR_OFFSET         0x02
#define EIR_OFFSET         0x02
#define BSR_OFFSET         0x03
#define LCR_OFFSET         0x03
#define MCR_OFFSET         0x04
#define LSR_OFFSET         0x05
#define MSR_OFFSET         0x06

#define LSR_TXRDY  0x20
#define LSR_RXDA   0x01
#define DLAB       0x01
#define MCR_DTRC   0x01
#define MCR_RTS    0x02
#define MSR_CTS    0x10
#define MSR_DSR    0x20
#define MSR_RI     0x40
#define MSR_DCD    0x80

  // ---------------------------------------------
  // UART Settings
  // ---------------------------------------------
  UINT16  gUartBase = 0x3F8;
  UINT64  gBps = 115200;
  UINT8   gData = 8;
  UINT8   gStop = 1;
  UINT8   gParity = 0;
  UINT8   gBreakSet = 0;

  extern "C" int _fltused = 0x9875;

  // copy pasted from EDK2
  UINT64 SerialPortWrite(UINT8* Buffer, UINT64 NumberOfBytes) noexcept
  {
    UINT64  Result;
    UINT8  Data;

    if (Buffer == NULL)
    {
      return 0;
    }

    Result = NumberOfBytes;

    while ((NumberOfBytes--) != 0)
    {

      // Wait for the serial port to be ready.
      do
      {
        Data = __inbyte(gUartBase + LSR_OFFSET);
      } while ((Data & LSR_TXRDY) == 0);

      __outbyte(gUartBase, *Buffer++);
    }

    return Result;
  }

  uint64_t get_physical_address_for_virtual_address_by_cr3(x86::cr3_t guest_cr3, void* virtual_address_guest)
  {
    virtual_address va_parts = { .all = reinterpret_cast<uint64_t>(virtual_address_guest) };
    uint64_t final_ptr;

    do
    {
      const auto pml4 = reinterpret_cast<pt::page_entry*>
        (physical_address_to_virtual_address(guest_cr3.flags.page_frame_number << page_shift));

      if (!pml4[va_parts.pml4_index].fields.present)
      {
        PRINT(("address = 0x%llx\n", virtual_address_guest));
        throw std::exception{ __FUNCTION__": ""PML4 isn't present." };
      }

      const auto pml3 = reinterpret_cast<pt::page_entry*>
        (physical_address_to_virtual_address(pml4[va_parts.pml4_index].fields.page_frame_number << page_shift));

      if (!pml3[va_parts.pdpt_index].fields.present)
      {
        throw std::exception{ __FUNCTION__": ""PML3 isn't present." };
      }

      if (pml3[va_parts.pdpt_index].fields.large_page)
      {
        final_ptr = physical_address_to_virtual_address(pml3[va_parts.pdpt_index].pdpt_large.page_frame_number << page_shift_1gb);

        final_ptr += va_parts.all & page_1gb_offset_mask;

        break;
      }

      const auto pml2 = reinterpret_cast<pt::page_entry*>
        (physical_address_to_virtual_address(pml3[va_parts.pdpt_index].fields.page_frame_number << page_shift));

      if (!pml2[va_parts.pd_index].pd.present)
      {
        throw std::exception{ __FUNCTION__": ""PML2 isn't present." };
      }

      if (pml2[va_parts.pd_index].pd.large_page)
      {
        final_ptr = physical_address_to_virtual_address(pml2[va_parts.pd_index].pd_large.page_frame_number << page_shift_2mb);

        final_ptr += va_parts.all & page_2mb_offset_mask;

        break;
      }

      const auto pml1 = reinterpret_cast<pt::page_entry*>
        (physical_address_to_virtual_address(pml2[va_parts.pd_index].fields.page_frame_number << page_shift));

      if (!pml1[va_parts.pt_index].fields.present)
      {
        throw std::exception{ __FUNCTION__": ""PML1 isn't present." };
      }

      final_ptr = physical_address_to_virtual_address(pml1[va_parts.pt_index].fields.page_frame_number << page_shift);
      final_ptr += va_parts.page_offset;

    } while (false);

    return final_ptr;
  }

  void print_formatted(const char* text, ...) noexcept
  {
    constexpr uint32_t max_debug_message_size = 0x1000;
    static char buff[max_debug_message_size];

    va_list args;
    va_start(args, text);

    const uint32_t bytes_to_write = snprintf(buff, max_debug_message_size, text, args);
    SerialPortWrite(reinterpret_cast<uint8_t*>(buff), bytes_to_write);

    va_end(args);
  }
}
