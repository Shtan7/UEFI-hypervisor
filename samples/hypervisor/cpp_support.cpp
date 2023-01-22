#include "uefi.hpp"
#include <exception>
#include <intrin.h>
#include "efi_stub.hpp"
#include "globals.hpp"
#include "memory_manager.hpp"
#include <stdexcept>

using _PVFV = void(__cdecl*)(void); // PVFV = Pointer to Void Func(Void)
using _PIFV = int(__cdecl*)(void); // PIFV = Pointer to Int Func(Void)
using _PVFVP = void(__cdecl*)(void*); // PVFVP = Pointer to Void Func(Void*)

constexpr int max_destructors_count = 64;
static _PVFV onexitarray[max_destructors_count] = {};
static _PVFV* onexitbegin = nullptr, * onexitend = nullptr;

// C initializers:
#pragma section(".CRT$XIA", long, read)
__declspec(allocate(".CRT$XIA")) _PIFV __xi_a[] = { 0 };
#pragma section(".CRT$XIZ", long, read)
__declspec(allocate(".CRT$XIZ")) _PIFV __xi_z[] = { 0 };

// C++ initializers:
#pragma section(".CRT$XCA", long, read)
__declspec(allocate(".CRT$XCA")) _PVFV __xc_a[] = { 0 };
#pragma section(".CRT$XCZ", long, read)
__declspec(allocate(".CRT$XCZ")) _PVFV __xc_z[] = { 0 };

// C pre-terminators:
#pragma section(".CRT$XPA", long, read)
__declspec(allocate(".CRT$XPA")) _PVFV __xp_a[] = { 0 };
#pragma section(".CRT$XPZ", long, read)
__declspec(allocate(".CRT$XPZ")) _PVFV __xp_z[] = { 0 };

// C terminators:
#pragma section(".CRT$XTA", long, read)
__declspec(allocate(".CRT$XTA")) _PVFV __xt_a[] = { 0 };
#pragma section(".CRT$XTZ", long, read)
__declspec(allocate(".CRT$XTZ")) _PVFV __xt_z[] = { 0 };

#pragma data_seg()

#pragma comment(linker, "/merge:.CRT=.rdata")

extern "C" void __std_terminate()
{
  hh::bug_check(hh::bug_check_codes::kmode_exception_not_handled);
}

extern "C" int __cdecl __init_on_exit_array()
{
  onexitend = onexitbegin = onexitarray;
  *onexitbegin = 0;
  return 0;
}

extern "C" int __cdecl atexit(_PVFV fn)
{
  // ToDo: replace with dynamically allocated list of destructors!
  if (onexitend > &onexitarray[max_destructors_count - 1])
    return 1; // Not enough space
  *onexitend = fn;
  onexitend++;
  return 0;
}

int __cdecl _purecall()
{
  // It's abnormal execution, so we should detect it:
  __int2c();
  return 0;
}

static void execute_pvfv_array(_PVFV* begin, _PVFV* end)
{
  _PVFV* fn = begin;
  while (fn != end)
  {
    if (*fn) (**fn)();
    ++fn;
  }
}

static int execute_pifv_array(_PIFV* begin, _PIFV* end)
{
  _PIFV* fn = begin;
  while (fn != end)
  {
    if (*fn)
    {
      int result = (**begin)();
      if (result) return result;
    }
    ++fn;
  }
  return 0;
}

extern "C" int __crt_init()
{
  __init_on_exit_array();
  int result = execute_pifv_array(__xi_a, __xi_z);
  if (result) return result;
  execute_pvfv_array(__xc_a, __xc_z);
  return 0;
}

extern "C" void __crt_deinit()
{
  if (onexitbegin)
  {
    while (--onexitend >= onexitbegin)
    {
      if (*onexitend != 0)(**onexitend)();
    }
  }
  execute_pvfv_array(__xp_a, __xp_z);
  execute_pvfv_array(__xt_a, __xt_z);
}

void __cdecl destroy_array_in_reversed_order(void* arr_begin, size_t element_size,
  size_t count, _PVFVP destructor)
{
  auto* current_obj{ static_cast<uint8_t*>(arr_begin) + element_size * count };

  while (count-- > 0)
  {
    current_obj -= element_size;
    destructor(current_obj);
  }
}

void __cdecl construct_array(void* arr_begin, size_t element_size,
  size_t count, _PVFVP constructor, _PVFVP destructor)
{
  auto* current_obj{ static_cast<uint8_t*>(arr_begin) };
  size_t idx{ 0 };

  try
  {
    for (; idx < count; ++idx)
    {
      constructor(current_obj);
      current_obj += element_size;
    }
  }
  catch (...)
  {
    destroy_array_in_reversed_order(arr_begin, element_size, idx, destructor);
    throw;
  }
}

extern "C" void __cdecl __ehvec_ctor(void* arr_begin, size_t element_size,
  size_t count, _PVFVP constructor, _PVFVP destructor)
{
  construct_array(arr_begin, element_size, count, constructor, destructor);
}

extern "C" void __cdecl __ehvec_dtor(void* arr_end, size_t element_size,
  size_t count, _PVFVP destructor)
{
  destroy_array_in_reversed_order(arr_end, element_size, count, destructor);
}

#define allocate_pool() if(hh::globals::boot_state) \
{ \
  if(EFI_ERROR(gBS->AllocatePool(EfiRuntimeServicesData, size, &pointer))) \
  { \
    hh::bug_check(hh::bug_check_codes::install_more_memory); \
  } \
} \
else \
{ \
  hh::bug_check(hh::bug_check_codes::boot_services_unavailable); \
} \

#define deallocate_pool() if(hh::globals::boot_state) \
{ \
  gBS->FreePool(pointer); \
} \
else \
{ \
  hh::bug_check(hh::bug_check_codes::boot_services_unavailable); \
} \

void* __cdecl operator new(size_t size)
{
  void* pointer;

  if (hh::globals::mem_manager != nullptr)
  {
    pointer = hh::globals::mem_manager->allocate(size);
  }
  else
  {
    allocate_pool();
  }

  if(pointer == nullptr)
  {
    throw std::bad_alloc{};
  }

  return pointer;
}

void* __cdecl operator new(size_t size, std::align_val_t align)
{
  void* pointer;

  if (hh::globals::mem_manager != nullptr)
  {
    pointer = hh::globals::mem_manager->allocate_align(size, align);
  }
  else
  {
    allocate_pool();
  }

  if (pointer == nullptr)
  {
    throw std::bad_alloc{};
  }

  return pointer;
}

void* __cdecl operator new[](size_t size)
{
  void* pointer;

  if (hh::globals::mem_manager != nullptr)
  {
    pointer = hh::globals::mem_manager->allocate(size);
  }
  else
  {
    allocate_pool();
  }

  if (pointer == nullptr)
  {
    throw std::bad_alloc{};
  }

  return pointer;
}

void* __cdecl operator new[](size_t size, std::align_val_t align)
{
  void* pointer;

  if (hh::globals::mem_manager != nullptr)
  {
    pointer = hh::globals::mem_manager->allocate_align(size, align);
  }
  else
  {
    allocate_pool();
  }

  if (pointer == nullptr)
  {
    throw std::bad_alloc{};
  }

  return pointer;
}

void __cdecl operator delete(void* pointer)
{
  if (pointer == nullptr)
  {
    return;
  }

  if (hh::globals::mem_manager != nullptr)
  {
    hh::globals::mem_manager->deallocate(pointer);
  }
  else
  {
    deallocate_pool();
  }
}

void __cdecl operator delete(void* pointer, [[maybe_unused]] std::align_val_t align)
{
  if (pointer == nullptr)
  {
    return;
  }

  if (hh::globals::mem_manager != nullptr)
  {
    hh::globals::mem_manager->deallocate(pointer);
  }
  else
  {
    deallocate_pool();
  }
}

void __cdecl operator delete(void* pointer, [[maybe_unused]] size_t size)
{
  if (pointer == nullptr)
  {
    return;
  }

  if (hh::globals::mem_manager != nullptr)
  {
    hh::globals::mem_manager->deallocate(pointer);
  }
  else
  {
    deallocate_pool();
  }
}

void __cdecl operator delete(void* pointer, [[maybe_unused]] size_t size, [[maybe_unused]] std::align_val_t align)
{
  if (pointer == nullptr)
  {
    return;
  }

  if (hh::globals::mem_manager != nullptr)
  {
    hh::globals::mem_manager->deallocate(pointer);
  }
  else
  {
    deallocate_pool();
  }
}

void __cdecl operator delete[](void* pointer)
{
  if (pointer == nullptr)
  {
    return;
  }

  if (hh::globals::mem_manager != nullptr)
  {
    hh::globals::mem_manager->deallocate(pointer);
  }
  else
  {
    deallocate_pool();
  }
}

void __cdecl operator delete[](void* pointer, [[maybe_unused]] std::align_val_t align)
{
  if (pointer == nullptr)
  {
    return;
  }

  if (hh::globals::mem_manager != nullptr)
  {
    hh::globals::mem_manager->deallocate(pointer);
  }
  else
  {
    deallocate_pool();
  }
}

void __cdecl operator delete[](void* pointer, [[maybe_unused]] size_t size)
{
  if (pointer == nullptr)
  {
    return;
  }

  if (hh::globals::mem_manager != nullptr)
  {
    hh::globals::mem_manager->deallocate(pointer);
  }
  else
  {
    deallocate_pool();
  }
}

void __cdecl operator delete[](void* pointer, [[maybe_unused]] size_t size, [[maybe_unused]] std::align_val_t align)
{
  if(pointer == nullptr)
  {
    return;
  }

  if (hh::globals::mem_manager != nullptr)
  {
    hh::globals::mem_manager->deallocate(pointer);
  }
  else
  {
    deallocate_pool();
  }
}

[[noreturn]]
static void RaiseException(hh::bug_check_codes code)
{
  hh::bug_check(code);
}

[[noreturn]]
void __cdecl _invalid_parameter_noinfo_noreturn()
{
  throw std::invalid_argument{ "Invalid argument has been passed to CRT function." };
}

namespace std
{
  [[noreturn]]
  void __cdecl _Xbad_alloc()
  {
    throw std::bad_alloc{};
  }

  [[noreturn]]
  void __cdecl _Xinvalid_argument(_In_z_ const char* m_str)
  {
    throw std::invalid_argument{ m_str };
  }

  [[noreturn]]
  void __cdecl _Xlength_error(_In_z_ const char* m_str)
  {
    throw std::length_error{ m_str };
  }

  [[noreturn]]
  void __cdecl _Xout_of_range(_In_z_ const char* m_str)
  {
    throw std::out_of_range{ m_str };
  }

  [[noreturn]]
  void __cdecl _Xoverflow_error(_In_z_ const char* m_str)
  {
    throw overflow_error{ m_str };
  }

  [[noreturn]]
  void __cdecl _Xruntime_error(_In_z_ const char* m_str)
  {
    throw std::runtime_error{ m_str };
  }

  [[noreturn]]
  void __cdecl RaiseHandler(const std::exception& e)
  {
    throw e;
  }

  _Prhand _Raise_handler = &RaiseHandler;
}

[[noreturn]]
void __cdecl _invoke_watson(
  [[maybe_unused]] wchar_t const* const expression,
  [[maybe_unused]] wchar_t const* const function_name,
  [[maybe_unused]] wchar_t const* const file_name,
  [[maybe_unused]] unsigned int   const line_number,
  [[maybe_unused]] uintptr_t      const reserved)
{
  RaiseException(hh::bug_check_codes::kmode_exception_not_handled);
}

extern "C" void* memmove(void* dest, const void* src, size_t len)
{
  char* d = static_cast<char*>(dest);
  const char* s = static_cast<const char*>(src);
  if (d < s)
    while (len--)
      *d++ = *s++;
  else
  {
    char* lasts = const_cast<char*>(s) + (len - 1);
    char* lastd = d + (len - 1);
    while (len--)
      *lastd-- = *lasts--;
  }
  return dest;
}

void* memcpy(void* dest, const void* src, size_t len)
{
  char* d = static_cast<char*>(dest);
  const char* s = static_cast<const char*>(src);
  while (len--)
    *d++ = *s++;
  return dest;
}

extern "C" int tolower(int ch)
{
  return ch += (static_cast<unsigned char>(ch - 'A') < 26) << 5;
}

extern "C" int toupper(int ch)
{
  return ch += (static_cast<unsigned char>(ch - 'a') < 26) << 5;
}

// For <unordered_set> and <unordered_map> support:
#ifdef _AMD64_
#pragma function(ceilf)
_Check_return_ float __cdecl ceilf(_In_ float _X)
{
  int v = static_cast<int>(_X);
  return static_cast<float>(_X > static_cast<float>(v) ? v + 1 : v);
}
#else
#pragma function(ceil)
_Check_return_ double __cdecl ceil(_In_ double _X)
{
  int v = static_cast<int>(_X);
  return static_cast<double>(_X > static_cast<double>(v) ? v + 1 : v);
}
#endif
