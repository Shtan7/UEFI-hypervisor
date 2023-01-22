#pragma once

typedef struct _EFI_MP_SERVICES_PROTOCOL EFI_MP_SERVICES_PROTOCOL;

namespace hh
{
  class memory_manager;
  class vcpu;
  class hook_builder;
  class per_cpu_data;

  namespace x86
  {
    struct idtr64_t;
  }

  namespace ept
  {
    class ept_handler;
  }

  namespace pt
  {
    class pt_handler;
  }

  namespace win_driver
  {
    struct win_driver_info;
  }

  namespace globals
  {
    inline memory_manager* mem_manager = {};
    inline EFI_MP_SERVICES_PROTOCOL* gEfiMpServiceProtocol = {};
    inline ept::ept_handler* ept_handler = {};
    inline pt::pt_handler* pt_handler = {};
    inline hook_builder* hook_handler = {};
    inline vcpu* vcpus = {};
    extern "C" x86::idtr64_t host_guest_idtr;
    extern "C" unsigned char __ImageBase;
    inline win_driver::win_driver_info* win_driver_struct = {};
    inline bool skip_init = {};
    inline per_cpu_data* cpu_related_data = {};
    inline unsigned long long number_of_cpus = {};
    inline bool panic_status = false;
    inline bool boot_state = true;
  }
}
