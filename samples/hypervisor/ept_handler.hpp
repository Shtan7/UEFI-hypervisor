#pragma once
#include <list>
#include "ept.hpp"
#include "delete_constructors.hpp"
#include "pt.hpp"
#include "vpid.hpp"

namespace hh
{
  namespace vmx
  {
    enum class invept_type;
  }

  namespace ept
  {
    // Class handles all EPT related operations.
    class ept_handler : non_relocatable
    {
    private:
      ept_state ept_state_;
      std::list<std::shared_ptr<vmm::dynamic_split>> splitted_pml2_;
      volatile long pml1_modification_and_invalidation_lock_ = {};

    private:
      void setup_pml2_entry(pml2_entry* new_entry, uint64_t page_frame_number) const noexcept;
      void create_identity_page_table();
      void build_mttr_map() noexcept;
      void is_ept_features_supported() const;

    public:
      ept_handler();
      void initialize_ept();
      eptp get_eptp() const noexcept;
      void set_pml1_and_invalidate_tlb(pml1_entry* entry_address, pml1_entry entry_value, vmx::invvpid_type invalidation_type) noexcept;
      void split_large_page(uint64_t physical_address);
      pml2_entry* get_pml2_entry(uint64_t physical_address);
      pml1_entry* get_pml1_entry(uint64_t physical_address);
      ~ept_handler() noexcept;
    };
  }
}
