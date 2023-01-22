#include "per_cpu_data.hpp"
#include "globals.hpp"
#include "common.hpp"
#include "vcpu.hpp"

namespace hh
{
  per_cpu_data* per_cpu_data::get_this() noexcept
  {
    return reinterpret_cast<per_cpu_data*>(__readgsqword(0));
  }

  per_cpu_data::per_cpu_data() : this_ptr_{ this }, callback_queue_{}, core_id_{ common::get_current_processor_number() },
                                 vcpu_ptr_{ globals::vcpus + common::get_current_processor_number() }, status_flag_{}, queue_lock_{}
  {}

  bool per_cpu_data::callback_ready_status() noexcept
  {
    per_cpu_data* this_ptr = get_this();
    return this_ptr->status_flag_.load();
  }

  void per_cpu_data::push_root_mode_callback_to_queue(root_mode_callback* callback, void* context)
  {
    for(size_t j = 0; j < globals::number_of_cpus; j++)
    {
      per_cpu_data* this_ptr = &globals::cpu_related_data[j];
      common::spinlock_guard _{ &this_ptr->queue_lock_ };

      this_ptr->callback_queue_.emplace_back(callback, context);
      this_ptr->status_flag_ = true;
    }
  }

  std::pair<per_cpu_data::root_mode_callback*, std::shared_ptr<void>> per_cpu_data::pop_root_mode_callback_from_queue() noexcept
  {
    per_cpu_data* this_ptr = get_this();

    common::spinlock_guard _{ &this_ptr->queue_lock_ };

    if(this_ptr->callback_queue_.empty())
    {
      this_ptr->status_flag_ = false;
      return {};
    }

    std::pair<root_mode_callback*, std::shared_ptr<void>> result = this_ptr->callback_queue_.back();
    this_ptr->callback_queue_.pop_back();

    return result;
  }

  uint64_t per_cpu_data::get_cpu_id() noexcept
  {
    const per_cpu_data* this_ptr = get_this();
    return this_ptr->core_id_;
  }

  vcpu* per_cpu_data::get_vcpu() noexcept
  {
    const per_cpu_data* this_ptr = get_this();
    return this_ptr->vcpu_ptr_;
  }
}
