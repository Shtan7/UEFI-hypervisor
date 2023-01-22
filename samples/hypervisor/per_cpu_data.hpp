#pragma once
#include <cstdint>
#include <list>
#include "delete_constructors.hpp"
#include <memory>
#include <optional>
#include <atomic>

namespace hh
{
  class vcpu;

  // Class reads data from GS segment base. We set custom GS base for root mode.
  class per_cpu_data : non_relocatable
  {
  public:
    using root_mode_callback = void(void*);

  private:
    per_cpu_data* this_ptr_;
    std::list<std::pair<root_mode_callback*, std::shared_ptr<void>>> callback_queue_;
    uint64_t core_id_;
    vcpu* vcpu_ptr_;
    std::atomic<uint32_t> status_flag_;
    volatile long queue_lock_;

  private:
    static per_cpu_data* get_this() noexcept;

  public:
    per_cpu_data();
    static bool callback_ready_status() noexcept;
    static void push_root_mode_callback_to_queue(root_mode_callback* callback, void* context);
    static std::pair<root_mode_callback*, std::shared_ptr<void>> pop_root_mode_callback_from_queue() noexcept;
    static uint64_t get_cpu_id() noexcept;
    static vcpu* get_vcpu() noexcept;
  };
}
