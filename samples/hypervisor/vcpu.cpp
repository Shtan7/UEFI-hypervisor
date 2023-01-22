#include "vcpu.hpp"
#include "ept_handler.hpp"
#include "vpid.hpp"
#include "globals.hpp"
#include "x86.hpp"
#include "vmexit_handler.hpp"
#include <string>
#include "pt_handler.hpp"
#include "per_cpu_data.hpp"

namespace hh
{
  vcpu::vcpu(std::shared_ptr<hv_event_handlers::vmexit_handler> exit_handler) : guest_state_{},
    vmexit_handler_{ std::move(exit_handler) }
  {
    fxsave_area_ = new(std::align_val_t{ 16 }) common::fxsave_area;
  }

  void vcpu::allocate_vmx_regions()
  {
    const uint64_t core_number = common::get_current_processor_number();
    PRINT(("Allocating vmx regions for core %d\n", core_number));

    enable_vmx_operation();

    allocate_vmx_on_region();
    allocate_vmcs_region();
    allocate_vmm_stack();
    allocate_msr_bitmap();

    // We mark VMX related bits as 'not present' in this MSR in vmexit handler.
    set_msr_bitmap(x86::msr::feature_control_msr_t::msr_id, true, true);
    // You can use vcpu::set_msr_bitmap here if you want to break on RDMSR or WRMSR with a specific MSR register.
  }

  void vcpu::setup_host_gdt()
  {
    x86::gdtr_t gdtr = x86::read<x86::gdtr_t>();
    guest_state_.original_gdtr = gdtr;
    guest_state_.host_guest_gdtr = guest_state_.original_gdtr;

    memcpy(guest_state_.host_guest_gdt_entries, reinterpret_cast<void*>(guest_state_.original_gdtr.base_address),
      guest_state_.original_gdtr.limit);

    x86::tr_t task_register = {};
    task_register.flags.index = (guest_state_.host_guest_gdtr.limit + 1) / x86::gdt_entry_size;

    guest_state_.host_guest_gdtr.base_address = reinterpret_cast<uint64_t>(&guest_state_.host_guest_gdt_entries);
    guest_state_.host_guest_gdtr.limit += sizeof(x86::gdt_entry_t);

    if (guest_state_.host_guest_gdtr.limit > sizeof(guest_state_.host_guest_gdt_entries))
    {
      throw std::exception{ __FUNCTION__": ""Invalid host_guest_gdtr limit." };
    }

    x86::gdt_entry_t tss_entry = {};

    tss_entry.limit(sizeof(guest_state_.host_guest_tss) - 1);
    tss_entry.base_address(&guest_state_.host_guest_tss);
    tss_entry.access.type = x86::segment_access_t::type_32b_tss_available;
    tss_entry.access.present = 1;

    guest_state_.host_guest_gdtr[task_register] = tss_entry;
    x86::write<x86::gdtr64_t>(guest_state_.host_guest_gdtr);
    x86::write<x86::tr_t>(task_register);
  }

  void vcpu::initialize_guest()
  {
    allocate_vmx_regions();
    save_state_and_start_virtualization();
  }

  void vcpu::clear_vmcs_state()
  {
    if (const int clear_status = __vmx_vmclear(&guest_state_.vmcs_region_physical_address))
    {
      PRINT(("VMCS clearing failed with status %d\n", clear_status));
      throw std::exception{ __FUNCTION__": ""failed." };
    }
  }

  void vcpu::load_vmcs()
  {
    if (const int vmptrld_status = __vmx_vmptrld(&guest_state_.vmcs_region_physical_address))
    {
      PRINT(("VMPTRLD failed with status %d\n", vmptrld_status));
      throw std::exception{ __FUNCTION__": ""failed." };
    }
  }

  void vcpu::setup_vmcs(void* stack_ptr) noexcept
  {
    const auto basic_msr = x86::msr::read<x86::msr::vmx_basic_msr_t>();
    const auto gdtr = x86::read<x86::gdtr_t>();

    host_gdtr(gdtr);
    host_idtr(globals::host_guest_idtr);

    // Set custom gs base for per cpu data in root mode.
    auto gs = x86::segment_t{ gdtr, x86::read<x86::gs_t>() };
    gs.base_address = &globals::cpu_related_data[common::get_current_processor_number()];

    host_es(x86::segment_t{ gdtr, x86::read<x86::es_t>() });
    host_cs(x86::segment_t{ gdtr, x86::read<x86::cs_t>() });
    host_ss(x86::segment_t{ gdtr, x86::read<x86::ss_t>() });
    host_ds(x86::segment_t{ gdtr, x86::read<x86::ds_t>() });
    host_fs(x86::segment_t{ gdtr, x86::read<x86::fs_t>() });
    host_gs(gs);
    host_tr(x86::segment_t{ gdtr, x86::read<x86::tr_t>() });

    // Setting the link pointer to the required value for 4KB VMCS.
    vmcs_link_pointer(~0ull);

    guest_debugctl(x86::msr::read<x86::msr::debugctl_t>());

    guest_es(x86::segment_t{ gdtr, x86::read<x86::es_t>() });
    guest_cs(x86::segment_t{ gdtr, x86::read<x86::cs_t>() });
    guest_ss(x86::segment_t{ gdtr, x86::read<x86::ss_t>() });
    guest_ds(x86::segment_t{ gdtr, x86::read<x86::ds_t>() });
    guest_fs(x86::segment_t{ gdtr, x86::read<x86::fs_t>() });
    guest_gs(x86::segment_t{ gdtr, x86::read<x86::gs_t>() });
    guest_tr(x86::segment_t{ gdtr, x86::read<x86::tr_t>() });
    guest_ldtr(x86::segment_t{ gdtr,x86::read<x86::ldtr_t>() });

    x86::msr::vmx_procbased_ctls_t procbased_ctls = {};
    procbased_ctls.flags.activate_secondary_controls = 1;
    procbased_ctls.flags.use_msr_bitmaps = 1;
    cpu_based_vm_exec_control(procbased_ctls, basic_msr);

    x86::msr::vmx_procbased_ctls2_t procbased_ctls2 = {};
    procbased_ctls2.flags.enable_rdtscp = 1;
    procbased_ctls2.flags.enable_ept = 1;
    procbased_ctls2.flags.enable_invpcid = 1;
    procbased_ctls2.flags.enable_xsaves = 1;
    procbased_ctls2.flags.enable_vpid = 1;
    procbased_ctls2.flags.unrestricted_guest = 1;
    procbased_ctls2.flags.enable_xsaves = 1;
    secondary_vm_exec_control(procbased_ctls2);

    x86::msr::vmx_pinbased_ctls_t pinbased_ctls = {};
    pinbased_ctls.flags.nmi_exiting = 1;
    pinbased_ctls.flags.virtual_nmis = 1;
    pin_based_vm_exec_control(pinbased_ctls, basic_msr);

    x86::msr::vmx_exit_ctls_t exit_ctls = {};
    exit_ctls.flags.ia32e_mode_host = 1;
    exit_ctls.flags.load_efer = 1;
    exit_ctls.flags.save_efer = 1;
    vm_exit_controls(exit_ctls, basic_msr);

    x86::msr::vmx_entry_ctls_t entry_ctls = {};
    entry_ctls.flags.ia32e_mode_guest = 1;
    entry_ctls.flags.load_ia32_efer = 1;
    vm_entry_controls(entry_ctls, basic_msr);

    // If bit is setted in host_guest mask then when guest tries to read it
    // he gets a bit from cr shadow mask.
    // But if guest tries to set it then vm exit occurs.
    x86::cr4_t cr4_host_guest_mask = {};
    cr4_host_guest_mask.flags.vmx_enable = 1;

    x86::cr0_t cr0_host_guest_mask = {};
    cr0_host_guest_mask.flags.numeric_error = 1;
    cr0_host_guest_mask.flags.paging_enable = 1;

    cr0_guest_host_mask(cr0_host_guest_mask);
    cr4_guest_host_mask(cr4_host_guest_mask);

    cr0_shadow(x86::read<x86::cr0_t>());
    cr4_shadow({});

    guest_cr0(vmx::adjust_cr0(x86::read<x86::cr0_t>()));
    guest_cr3(x86::read<x86::cr3_t>());
    guest_cr4(vmx::adjust_cr4(x86::read<x86::cr4_t>()));

    x86::dr7_t dr7 = {};
    dr7.flags.reserved_1 = 1;
    guest_dr7(dr7);

    host_cr0(vmx::adjust_cr0(x86::read<x86::cr0_t>()));
    host_cr4(vmx::adjust_cr4(x86::read<x86::cr4_t>()));
    host_cr3(x86::cr3_t{ .all = globals::pt_handler->get_cr3().all });

    guest_gdtr(gdtr);
    guest_idtr(x86::read<x86::idtr_t>());
    guest_rflags(x86::read<x86::rflags_t>());

    guest_sysenter_cs(x86::msr::read<x86::msr::sysenter_cs>());
    guest_sysenter_eip(x86::msr::read<x86::msr::sysenter_eip>());
    guest_sysenter_esp(x86::msr::read<x86::msr::sysenter_esp>());

    host_sysenter_cs(x86::msr::read<x86::msr::sysenter_cs>());
    host_sysenter_eip(x86::msr::read<x86::msr::sysenter_eip>());
    host_sysenter_esp(x86::msr::read<x86::msr::sysenter_esp>());

    msr_bitmap(guest_state_.msr_bitmap_physical_address);

    host_efer(x86::msr::read<x86::msr::efer_t>());
    guest_efer(x86::msr::read<x86::msr::efer_t>());

    // Intercept divide_error exception to map our win driver
    // or skip patchguard initialization.
    vmx::exception_bitmap exc_bitmap = {};
    exc_bitmap.divide_error = 1;
    exception_bitmap(exc_bitmap);

    ept_pointer(globals::ept_handler->get_eptp().flags);

    // Set up VPID
    /* For all processors, we will use a VPID = 1. This allows the processor to separate caching
    of EPT structures away from the regular OS page translation tables in the TLB. */
    vpid(vmx::vpid_tag);

    guest_rsp(reinterpret_cast<uint64_t>(stack_ptr));
    guest_rip(reinterpret_cast<uint64_t>(&vcpu::restore_state));

    host_rsp(reinterpret_cast<uint64_t>(guest_state_.vmm_stack) + vmx::vmm_stack_size);
    host_rip(reinterpret_cast<uint64_t>(&hv_event_handlers::vmexit_handler::vmexit_entry));
  }

  void vcpu::virtualize_current_system(void* stack_ptr)
  {
    const int processor_id = common::get_current_processor_number();

    PRINT(("======================== Virtualizing Current System (Logical Core : 0x%x) ========================\n", processor_id));

    setup_host_gdt();
    clear_vmcs_state();
    load_vmcs();

    setup_vmcs(stack_ptr);

    __vmx_vmlaunch();
    __vmx_off();

    auto error = vm_instruction_error();

    PRINT(("VMLAUNCH error: %a\n", enum_to_str(error).data()));
    throw std::exception{ __FUNCTION__": ""VMLAUNCH failed." };
  }

  void vcpu::set_msr_bitmap(uint64_t msr, bool read_detection, bool write_detection)
  {
    if (!read_detection && !write_detection)
    {
      throw std::exception{ __FUNCTION__": ""Invalid msr read / write flag combination." };
    }

    if (msr <= x86::msr::low_msr_high_range)
    {
      if (read_detection)
      {
        common::set_bit(guest_state_.msr_bitmap_virtual_address, msr, true);
      }

      if (write_detection)
      {
        common::set_bit(guest_state_.msr_bitmap_virtual_address
          + x86::msr::low_msr_write_bitmap_offset, msr, true);
      }
    }
    else if (x86::msr::high_msr_low_range <= msr && msr <= x86::msr::high_msr_high_range)
    {
      if (read_detection)
      {
        common::set_bit(guest_state_.msr_bitmap_virtual_address
          + x86::msr::high_msr_read_bitmap_offset, msr - x86::msr::high_msr_low_range, true);
      }

      if (write_detection)
      {
        common::set_bit(guest_state_.msr_bitmap_virtual_address
          + x86::msr::high_msr_write_bitmap_offset, msr - x86::msr::high_msr_low_range, true);
      }
    }
    else
    {
      throw std::exception{ __FUNCTION__": ""Invalid msr range." };
    }
  }

  void vcpu::allocate_msr_bitmap()
  {
    guest_state_.msr_bitmap_virtual_address = new (std::align_val_t{ common::page_size }) uint8_t[common::page_size];
    guest_state_.msr_bitmap_physical_address = common::virtual_address_to_physical_address(guest_state_.msr_bitmap_virtual_address);

    memset(guest_state_.msr_bitmap_virtual_address, 0, common::page_size);

    PRINT(("Msr Bitmap Virtual Address : 0x%llx\n", guest_state_.msr_bitmap_virtual_address));
    PRINT(("Msr Bitmap Physical Address : 0x%llx\n", guest_state_.msr_bitmap_physical_address));
  }

  void vcpu::allocate_vmm_stack()
  {
    guest_state_.vmm_stack = new (std::align_val_t{ common::page_size }) uint8_t[vmx::vmm_stack_size];

    PRINT(("Vmm Stack for logical processor : 0x%llx\n", guest_state_.vmm_stack));
  }

  void vcpu::enable_vmx_operation() const noexcept
  {
    x86::cr0_t cr0 = x86::read<x86::cr0_t>();
    x86::cr4_t cr4 = x86::read<x86::cr4_t>();
    cr4.flags.vmx_enable = 1;

    cr4 = vmx::adjust_cr4(cr4);
    cr0 = vmx::adjust_cr0(cr0);

    x86::write<x86::cr4_t>(cr4);
    x86::write<x86::cr0_t>(cr0);
  }

  void vcpu::allocate_vmx_on_region()
  {
    uint8_t* vmxon_region = new (std::align_val_t{ common::page_size }) uint8_t[vmx::vmxon_size];
    uint64_t vmxon_physical_address = common::virtual_address_to_physical_address(vmxon_region);

    memset(vmxon_region, 0, vmx::vmxon_size);

    PRINT(("VMXON region address 0x%llx\n", vmxon_region));
    PRINT(("VMXON physical address 0x%llx\n", vmxon_physical_address));

    // get IA32_VMX_BASIC_MSR RevisionId
    x86::msr::vmx_basic_msr_t vmx_basic_msr = x86::msr::read<x86::msr::vmx_basic_msr_t>();

    // Changing Revision Identifier
    *reinterpret_cast<uint64_t*>(vmxon_region) = vmx_basic_msr.fields.revision_identifier;

    if (const int vmxon_status = __vmx_on(&vmxon_physical_address))
    {
      PRINT(("Executing VMXON instruction failed with status %d\n", vmxon_status));
      throw std::exception{ __FUNCTION__": ""Executing VMXON instruction failed." };
    }

    guest_state_.vmxon_region_physical_address = vmxon_physical_address;
    guest_state_.vmxon_region_virtual_address = vmxon_region;
  }

  void vcpu::allocate_vmcs_region()
  {
    uint8_t* vmcs_region = new (std::align_val_t{ common::page_size }) uint8_t[vmx::vmcs_size];
    const uint64_t vmcs_physical_address = common::virtual_address_to_physical_address(vmcs_region);

    memset(vmcs_region, 0, vmx::vmcs_size);

    PRINT(("VMCS region address 0x%llx\n", vmcs_region));
    PRINT(("VMCS region physical address 0x%llx\n", vmcs_physical_address));

    // get IA32_VMX_BASIC_MSR RevisionId
    const x86::msr::vmx_basic_msr_t vmx_basic_msr = x86::msr::read<x86::msr::vmx_basic_msr_t>();

    // Changing Revision Identifier
    *reinterpret_cast<uint64_t*>(vmcs_region) = vmx_basic_msr.fields.revision_identifier;

    guest_state_.vmcs_region_physical_address = vmcs_physical_address;
    guest_state_.vmcs_region_virtual_address = vmcs_region;
  }

  vcpu::~vcpu()
  {
    delete[] guest_state_.vmcs_region_virtual_address;
    delete[] guest_state_.vmxon_region_virtual_address;
    delete[] guest_state_.msr_bitmap_virtual_address;
    delete[] guest_state_.vmm_stack;
    delete fxsave_area_;
  }

  uint64_t vcpu::vmxoff_state_guest_rip() const noexcept
  {
    return guest_state_.vmxoff_state.guest_rip;
  }

  void vcpu::vmxoff_state_guest_rip(uint64_t rip) noexcept
  {
    guest_state_.vmxoff_state.guest_rip = rip;
  }

  uint64_t vcpu::vmxoff_state_guest_rsp() const noexcept
  {
    return guest_state_.vmxoff_state.guest_rsp;
  }

  void vcpu::vmxoff_state_guest_rsp(uint64_t rsp) noexcept
  {
    guest_state_.vmxoff_state.guest_rsp = rsp;
  }

  bool vcpu::vmxoff_executed() const noexcept
  {
    return guest_state_.vmxoff_state.is_vmxoff_executed;
  }

  void vcpu::vmxoff_executed(bool bit) noexcept
  {
    guest_state_.vmxoff_state.is_vmxoff_executed = bit;
  }

  vmx::exit_reason vcpu::vmexit_reason() const noexcept
  {
    vmx::exit_reason vmexit_reason;
    vmx::vmread(vmx::vmcs_fields::vm_exit_reason, vmexit_reason);

    return vmexit_reason;
  }

  std::shared_ptr<hv_event_handlers::vmexit_handler> vcpu::vmexit_handler() const noexcept
  {
    return vmexit_handler_;
  }

  void vcpu::vmexit_handler(std::shared_ptr<hv_event_handlers::vmexit_handler> ptr) noexcept
  {
    vmexit_handler_ = std::move(ptr);
  }

  x86::segment_t<x86::cs_t> vcpu::host_cs() const noexcept
  {
    x86::segment_t<x86::cs_t> cs;
    vmx::vmread(vmx::vmcs_fields::host_cs_selector, cs.selector);

    return cs;
  }

  void vcpu::host_cs(x86::segment_t<x86::cs_t> cs) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::host_cs_selector, cs.selector.flags.index * 8);
  }

  x86::segment_t<x86::es_t> vcpu::host_es() const noexcept
  {
    x86::segment_t<x86::es_t> es;
    vmx::vmread(vmx::vmcs_fields::host_es_selector, es.selector);

    return es;
  }

  void vcpu::host_es(x86::segment_t<x86::es_t> es) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::host_es_selector, es.selector.flags.index * 8);
  }

  x86::segment_t<x86::fs_t> vcpu::host_fs() const noexcept
  {
    x86::segment_t<x86::fs_t> fs;
    vmx::vmread(vmx::vmcs_fields::host_fs_selector, fs.selector);
    vmx::vmread(vmx::vmcs_fields::host_fs_base, fs.base_address);

    return fs;
  }

  void vcpu::host_fs(x86::segment_t<x86::fs_t> fs) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::host_fs_selector, fs.selector.flags.index * 8);
    vmx::vmwrite(vmx::vmcs_fields::host_fs_base, fs.base_address);
  }

  x86::segment_t<x86::gs_t> vcpu::host_gs() const noexcept
  {
    x86::segment_t<x86::gs_t> gs;
    vmx::vmread(vmx::vmcs_fields::host_gs_selector, gs.selector);
    vmx::vmread(vmx::vmcs_fields::host_gs_base, gs.base_address);

    return gs;
  }

  void vcpu::host_gs(x86::segment_t<x86::gs_t> gs) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::host_gs_selector, gs.selector.flags.index * 8);
    vmx::vmwrite(vmx::vmcs_fields::host_gs_base, gs.base_address);
  }

  x86::segment_t<x86::ss_t> vcpu::host_ss() const noexcept
  {
    x86::segment_t<x86::ss_t> ss;
    vmx::vmread(vmx::vmcs_fields::host_ss_selector, ss.selector);

    return ss;
  }

  void vcpu::host_ss(x86::segment_t<x86::ss_t> ss) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::host_ss_selector, ss.selector.flags.index * 8);
  }

  x86::segment_t<x86::tr_t> vcpu::host_tr() const noexcept
  {
    x86::segment_t<x86::tr_t> tr;
    vmx::vmread(vmx::vmcs_fields::host_tr_selector, tr.selector);
    vmx::vmread(vmx::vmcs_fields::host_tr_base, tr.base_address);

    return tr;
  }

  void vcpu::host_tr(x86::segment_t<x86::tr_t> tr) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::host_tr_selector, tr.selector.flags.index * 8);
    vmx::vmwrite(vmx::vmcs_fields::host_tr_base, tr.base_address);
  }

  x86::segment_t<x86::ds_t> vcpu::host_ds() const noexcept
  {
    x86::segment_t<x86::ds_t> ds;
    vmx::vmread(vmx::vmcs_fields::host_ss_selector, ds.selector);

    return ds;
  }

  void vcpu::host_ds(x86::segment_t<x86::ds_t> ds) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::host_ss_selector, ds.selector.flags.index * 8);
  }

  void vcpu::vmcs_link_pointer(uint64_t link_pointer) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::vmcs_link_pointer, link_pointer);
  }

  x86::segment_t<x86::cs_t> vcpu::guest_cs() const noexcept
  {
    x86::segment_t<x86::cs_t> cs;
    vmx::vmread(vmx::vmcs_fields::guest_cs_base, cs.base_address);
    vmx::vmread(vmx::vmcs_fields::guest_cs_limit, cs.limit);
    vmx::vmread(vmx::vmcs_fields::guest_cs_ar_bytes, cs.access);
    vmx::vmread(vmx::vmcs_fields::guest_cs_selector, cs.selector);

    return cs;
  }

  void vcpu::guest_cs(x86::segment_t<x86::cs_t> cs) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::guest_cs_base, cs.base_address);
    vmx::vmwrite(vmx::vmcs_fields::guest_cs_limit, cs.limit);
    vmx::vmwrite(vmx::vmcs_fields::guest_cs_ar_bytes, cs.access);
    vmx::vmwrite(vmx::vmcs_fields::guest_cs_selector, cs.selector);
  }

  x86::segment_t<x86::ds_t> vcpu::guest_ds() const noexcept
  {
    x86::segment_t<x86::ds_t> ds;
    vmx::vmread(vmx::vmcs_fields::guest_ds_base, ds.base_address);
    vmx::vmread(vmx::vmcs_fields::guest_ds_limit, ds.limit);
    vmx::vmread(vmx::vmcs_fields::guest_ds_ar_bytes, ds.access);
    vmx::vmread(vmx::vmcs_fields::guest_ds_selector, ds.selector);

    return ds;
  }

  void vcpu::guest_ds(x86::segment_t<x86::ds_t> ds) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::guest_ds_base, ds.base_address);
    vmx::vmwrite(vmx::vmcs_fields::guest_ds_limit, ds.limit);
    vmx::vmwrite(vmx::vmcs_fields::guest_ds_ar_bytes, ds.access);
    vmx::vmwrite(vmx::vmcs_fields::guest_ds_selector, ds.selector);
  }

  x86::segment_t<x86::es_t> vcpu::guest_es() const noexcept
  {
    x86::segment_t<x86::es_t> es;
    vmx::vmread(vmx::vmcs_fields::guest_es_base, es.base_address);
    vmx::vmread(vmx::vmcs_fields::guest_es_limit, es.limit);
    vmx::vmread(vmx::vmcs_fields::guest_es_ar_bytes, es.access);
    vmx::vmread(vmx::vmcs_fields::guest_es_selector, es.selector);

    return es;
  }

  void vcpu::guest_es(x86::segment_t<x86::es_t> es) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::guest_es_base, es.base_address);
    vmx::vmwrite(vmx::vmcs_fields::guest_es_limit, es.limit);
    vmx::vmwrite(vmx::vmcs_fields::guest_es_ar_bytes, es.access);
    vmx::vmwrite(vmx::vmcs_fields::guest_es_selector, es.selector);
  }

  x86::segment_t<x86::fs_t> vcpu::guest_fs() const noexcept
  {
    x86::segment_t<x86::fs_t> fs;
    vmx::vmread(vmx::vmcs_fields::guest_fs_base, fs.base_address);
    vmx::vmread(vmx::vmcs_fields::guest_fs_limit, fs.limit);
    vmx::vmread(vmx::vmcs_fields::guest_fs_ar_bytes, fs.access);
    vmx::vmread(vmx::vmcs_fields::guest_fs_selector, fs.selector);

    return fs;
  }

  void vcpu::guest_fs(x86::segment_t<x86::fs_t> fs) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::guest_fs_base, fs.base_address);
    vmx::vmwrite(vmx::vmcs_fields::guest_fs_limit, fs.limit);
    vmx::vmwrite(vmx::vmcs_fields::guest_fs_ar_bytes, fs.access);
    vmx::vmwrite(vmx::vmcs_fields::guest_fs_selector, fs.selector);
  }

  x86::segment_t<x86::gs_t> vcpu::guest_gs() const noexcept
  {
    x86::segment_t<x86::gs_t> gs;
    vmx::vmread(vmx::vmcs_fields::guest_gs_base, gs.base_address);
    vmx::vmread(vmx::vmcs_fields::guest_gs_limit, gs.limit);
    vmx::vmread(vmx::vmcs_fields::guest_gs_ar_bytes, gs.access);
    vmx::vmread(vmx::vmcs_fields::guest_gs_selector, gs.selector);

    return gs;
  }

  void vcpu::guest_gs(x86::segment_t<x86::gs_t> gs) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::guest_gs_base, gs.base_address);
    vmx::vmwrite(vmx::vmcs_fields::guest_gs_limit, gs.limit);
    vmx::vmwrite(vmx::vmcs_fields::guest_gs_ar_bytes, gs.access);
    vmx::vmwrite(vmx::vmcs_fields::guest_gs_selector, gs.selector);
  }

  x86::segment_t<x86::ss_t> vcpu::guest_ss() const noexcept
  {
    x86::segment_t<x86::ss_t> ss;
    vmx::vmread(vmx::vmcs_fields::guest_ss_base, ss.base_address);
    vmx::vmread(vmx::vmcs_fields::guest_ss_limit, ss.limit);
    vmx::vmread(vmx::vmcs_fields::guest_ss_ar_bytes, ss.access);
    vmx::vmread(vmx::vmcs_fields::guest_ss_selector, ss.selector);

    return ss;
  }

  void vcpu::guest_ss(x86::segment_t<x86::ss_t> ss) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::guest_ss_base, ss.base_address);
    vmx::vmwrite(vmx::vmcs_fields::guest_ss_limit, ss.limit);
    vmx::vmwrite(vmx::vmcs_fields::guest_ss_ar_bytes, ss.access);
    vmx::vmwrite(vmx::vmcs_fields::guest_ss_selector, ss.selector);
  }

  x86::segment_t<x86::tr_t> vcpu::guest_tr() const noexcept
  {
    x86::segment_t<x86::tr_t> tr;
    vmx::vmread(vmx::vmcs_fields::guest_tr_base, tr.base_address);
    vmx::vmread(vmx::vmcs_fields::guest_tr_limit, tr.limit);
    vmx::vmread(vmx::vmcs_fields::guest_tr_ar_bytes, tr.access);
    vmx::vmread(vmx::vmcs_fields::guest_tr_selector, tr.selector);

    return tr;
  }

  void vcpu::guest_tr(x86::segment_t<x86::tr_t> tr) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::guest_tr_base, tr.base_address);
    vmx::vmwrite(vmx::vmcs_fields::guest_tr_limit, tr.limit);
    vmx::vmwrite(vmx::vmcs_fields::guest_tr_ar_bytes, tr.access);
    vmx::vmwrite(vmx::vmcs_fields::guest_tr_selector, tr.selector);
  }

  x86::segment_t<x86::ldtr_t> vcpu::guest_ldtr() const noexcept
  {
    x86::segment_t<x86::ldtr_t> ldtr;
    vmx::vmread(vmx::vmcs_fields::guest_ldtr_base, ldtr.base_address);
    vmx::vmread(vmx::vmcs_fields::guest_ldtr_limit, ldtr.limit);
    vmx::vmread(vmx::vmcs_fields::guest_ldtr_ar_bytes, ldtr.access);
    vmx::vmread(vmx::vmcs_fields::guest_ldtr_selector, ldtr.selector);

    return ldtr;
  }

  void vcpu::guest_ldtr(x86::segment_t<x86::ldtr_t> ldtr) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::guest_ldtr_base, ldtr.base_address);
    vmx::vmwrite(vmx::vmcs_fields::guest_ldtr_limit, ldtr.limit);
    vmx::vmwrite(vmx::vmcs_fields::guest_ldtr_ar_bytes, ldtr.access);
    vmx::vmwrite(vmx::vmcs_fields::guest_ldtr_selector, ldtr.selector);
  }

  x86::cr0_t vcpu::guest_cr0() const noexcept
  {
    x86::cr0_t cr0;
    vmx::vmread(vmx::vmcs_fields::guest_cr0, cr0);

    return cr0;
  }

  void vcpu::guest_cr0(x86::cr0_t cr0) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::guest_cr0, cr0);
  }

  x86::cr3_t vcpu::guest_cr3() const noexcept
  {
    x86::cr3_t cr3;
    vmx::vmread(vmx::vmcs_fields::guest_cr3, cr3);

    return cr3;
  }

  void vcpu::guest_cr3(x86::cr3_t cr3) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::guest_cr3, cr3);
  }

  x86::cr4_t vcpu::guest_cr4() const noexcept
  {
    x86::cr4_t cr4;
    vmx::vmread(vmx::vmcs_fields::guest_cr4, cr4);

    return cr4;
  }

  void vcpu::guest_cr4(x86::cr4_t cr4) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::guest_cr4, cr4);
  }

  x86::dr7_t vcpu::guest_dr7() const noexcept
  {
    x86::dr7_t dr7;
    vmx::vmread(vmx::vmcs_fields::guest_dr7, dr7);

    return dr7;
  }

  void vcpu::guest_dr7(x86::dr7_t dr7) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::guest_dr7, dr7);
  }

  x86::msr::debugctl_t vcpu::guest_debugctl() const noexcept
  {
    x86::msr::debugctl_t debugctl;
    vmx::vmread(vmx::vmcs_fields::guest_ia32_debugctl, debugctl);

    return debugctl;
  }
  void vcpu::guest_debugctl(x86::msr::debugctl_t debugctl) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::guest_ia32_debugctl, debugctl);
  }

  uint64_t vcpu::guest_rsp() const noexcept
  {
    uint64_t rsp;
    vmx::vmread(vmx::vmcs_fields::guest_rsp, rsp);

    return rsp;
  }

  void vcpu::guest_rsp(uint64_t rsp) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::guest_rsp, rsp);
  }

  uint64_t vcpu::guest_rip() const noexcept
  {
    uint64_t rip;
    vmx::vmread(vmx::vmcs_fields::guest_rip, rip);

    return rip;
  }

  void vcpu::guest_rip(uint64_t rip) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::guest_rip, rip);
  }

  x86::rflags_t vcpu::guest_rflags() const noexcept
  {
    x86::rflags_t rflags;
    vmx::vmread(vmx::vmcs_fields::guest_rflags, rflags);

    return rflags;
  }

  void vcpu::guest_rflags(x86::rflags_t rflags) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::guest_rflags, rflags);
  }

  x86::gdtr_t vcpu::guest_gdtr() const noexcept
  {
    x86::gdtr_t gdtr;
    vmx::vmread(vmx::vmcs_fields::guest_gdtr_base, gdtr.base_address);
    vmx::vmread(vmx::vmcs_fields::guest_gdtr_limit, gdtr.limit);

    return gdtr;
  }

  void vcpu::guest_gdtr(x86::gdtr_t gdtr) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::guest_gdtr_base, gdtr.base_address);
    vmx::vmwrite(vmx::vmcs_fields::guest_gdtr_limit, gdtr.limit);
  }

  x86::idtr_t vcpu::guest_idtr() const noexcept
  {
    x86::idtr_t idtr;
    vmx::vmread(vmx::vmcs_fields::guest_idtr_base, idtr.base_address);
    vmx::vmread(vmx::vmcs_fields::guest_idtr_limit, idtr.limit);

    return idtr;
  }

  void vcpu::guest_idtr(x86::idtr_t idtr) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::guest_idtr_base, idtr.base_address);
    vmx::vmwrite(vmx::vmcs_fields::guest_idtr_limit, idtr.limit);
  }

  void vcpu::tsc_offset(uint64_t value) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::tsc_offset, value);
  }

  uint64_t vcpu::tsc_offset() const noexcept
  {
    uint64_t value;
    vmx::vmread(vmx::vmcs_fields::tsc_offset, value);

    return value;
  }

  void vcpu::tsc_multiplier(uint64_t value) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::tsc_multiplier, value);
  }

  uint64_t vcpu::tsc_multiplier() const noexcept
  {
    uint64_t value;
    vmx::vmread(vmx::vmcs_fields::tsc_multiplier, value);

    return value;
  }

  void vcpu::page_fault_error_code_mask(ept::pagefault_error_code mask) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::page_fault_error_code_mask, mask);
  }

  ept::pagefault_error_code vcpu::page_fault_error_code_mask() const noexcept
  {
    ept::pagefault_error_code mask;
    vmx::vmread(vmx::vmcs_fields::page_fault_error_code_mask, mask);

    return mask;
  }

  void vcpu::page_fault_error_code_match(ept::pagefault_error_code match) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::page_fault_error_code_match, match);
  }

  ept::pagefault_error_code vcpu::page_fault_error_code_match() const noexcept
  {
    ept::pagefault_error_code match;
    vmx::vmread(vmx::vmcs_fields::page_fault_error_code_mask, match);

    return match;
  }

  void vcpu::cpu_based_vm_exec_control(x86::msr::vmx_procbased_ctls_t procbased_ctls, x86::msr::vmx_basic_msr_t basic_msr) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::cpu_based_vm_exec_control, vmx::adjust_controls(procbased_ctls.all, basic_msr.fields.vmx_capability_hint
      ? x86::msr::vmx_true_procbased_ctls::msr_id : x86::msr::vmx_procbased_ctls_t::msr_id));
  }

  x86::msr::vmx_procbased_ctls_t vcpu::cpu_based_vm_exec_control() const noexcept
  {
    x86::msr::vmx_procbased_ctls_t procbased_ctls;
    vmx::vmread(vmx::vmcs_fields::cpu_based_vm_exec_control, procbased_ctls);

    return procbased_ctls;
  }

  void vcpu::secondary_vm_exec_control(x86::msr::vmx_procbased_ctls2_t procbased_ctls2) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::secondary_vm_exec_control, vmx::adjust_controls(procbased_ctls2.all, x86::msr::vmx_procbased_ctls2_t::msr_id));
  }

  x86::msr::vmx_procbased_ctls2_t vcpu::secondary_vm_exec_control() const noexcept
  {
    x86::msr::vmx_procbased_ctls2_t procbased_ctls2;
    vmx::vmread(vmx::vmcs_fields::secondary_vm_exec_control, procbased_ctls2);

    return procbased_ctls2;
  }

  void vcpu::pin_based_vm_exec_control(x86::msr::vmx_pinbased_ctls_t pinbased_ctls, x86::msr::vmx_basic_msr_t basic_msr) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::pin_based_vm_exec_control, vmx::adjust_controls(0,
      basic_msr.fields.vmx_capability_hint ? x86::msr::vmx_true_pinbased_ctls::msr_id : x86::msr::vmx_pinbased_ctls_t::msr_id));
  }

  x86::msr::vmx_pinbased_ctls_t vcpu::pin_based_vm_exec_control() const noexcept
  {
    x86::msr::vmx_pinbased_ctls_t pinbased_ctls;
    vmx::vmread(vmx::vmcs_fields::pin_based_vm_exec_control, pinbased_ctls);

    return pinbased_ctls;
  }

  void vcpu::vm_exit_controls(x86::msr::vmx_exit_ctls_t exit_ctls, x86::msr::vmx_basic_msr_t basic_msr) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::vm_exit_controls, vmx::adjust_controls(exit_ctls.all,
      basic_msr.fields.vmx_capability_hint ? x86::msr::vmx_true_exit_ctls::msr_id : x86::msr::vmx_exit_ctls_t::msr_id));
  }

  x86::msr::vmx_exit_ctls_t vcpu::vm_exit_controls() const noexcept
  {
    x86::msr::vmx_exit_ctls_t exit_ctls;
    vmx::vmread(vmx::vmcs_fields::vm_exit_controls, exit_ctls);

    return exit_ctls;
  }

  void vcpu::vm_entry_controls(x86::msr::vmx_entry_ctls_t entry_ctls, x86::msr::vmx_basic_msr_t basic_msr) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::vm_entry_controls, vmx::adjust_controls(entry_ctls.all,
      basic_msr.fields.vmx_capability_hint ? x86::msr::vmx_true_entry_ctls::msr_id : x86::msr::vmx_entry_ctls_t::msr_id));
  }

  x86::msr::vmx_entry_ctls_t vcpu::vm_entry_controls() const noexcept
  {
    x86::msr::vmx_entry_ctls_t entry_ctls;
    vmx::vmread(vmx::vmcs_fields::vm_entry_controls, entry_ctls);

    return entry_ctls;
  }

  x86::cr0_t vcpu::cr0_guest_host_mask() const noexcept
  {
    x86::cr0_t cr0;
    vmx::vmread(vmx::vmcs_fields::cr0_guest_host_mask, cr0);

    return cr0;
  }

  void vcpu::cr0_guest_host_mask(x86::cr0_t cr0) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::cr0_guest_host_mask, cr0);
  }

  x86::cr0_t vcpu::cr0_shadow() const noexcept
  {
    x86::cr0_t cr0;
    vmx::vmread(vmx::vmcs_fields::cr0_read_shadow, cr0);

    return cr0;
  }

  void vcpu::cr0_shadow(x86::cr0_t cr0) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::cr0_read_shadow, cr0);
  }

  x86::cr4_t vcpu::cr4_guest_host_mask() const noexcept
  {
    x86::cr4_t cr4;
    vmx::vmread(vmx::vmcs_fields::cr4_guest_host_mask, cr4);

    return cr4;
  }

  void vcpu::cr4_guest_host_mask(x86::cr4_t cr4) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::cr4_guest_host_mask, cr4);
  }

  x86::cr4_t vcpu::cr4_shadow() const noexcept
  {
    x86::cr4_t cr4;
    vmx::vmread(vmx::vmcs_fields::cr4_read_shadow, cr4);

    return cr4;
  }

  void vcpu::cr4_shadow(x86::cr4_t cr4) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::cr4_read_shadow, cr4);
  }

  x86::cr0_t vcpu::host_cr0() const noexcept
  {
    x86::cr0_t cr0;
    vmx::vmread(vmx::vmcs_fields::host_cr0, cr0);

    return cr0;
  }

  void vcpu::host_cr0(x86::cr0_t cr0) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::host_cr0, cr0);
  }

  x86::cr3_t vcpu::host_cr3() const noexcept
  {
    x86::cr3_t cr3;
    vmx::vmread(vmx::vmcs_fields::host_cr3, cr3);

    return cr3;
  }

  void vcpu::host_cr3(x86::cr3_t cr3) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::host_cr3, cr3);
  }

  x86::cr4_t vcpu::host_cr4() const noexcept
  {
    x86::cr4_t cr4;
    vmx::vmread(vmx::vmcs_fields::host_cr4, cr4);

    return cr4;
  }

  void vcpu::host_cr4(x86::cr4_t cr4) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::host_cr4, cr4);
  }

  uint64_t vcpu::host_rsp() const noexcept
  {
    uint64_t rsp;
    vmx::vmread(vmx::vmcs_fields::host_rsp, rsp);

    return rsp;
  }

  void vcpu::host_rsp(uint64_t rsp) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::host_rsp, rsp);
  }

  uint64_t vcpu::host_rip() const noexcept
  {
    uint64_t rip;
    vmx::vmread(vmx::vmcs_fields::host_rip, rip);

    return rip;
  }

  void vcpu::host_rip(uint64_t rip) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::host_rip, rip);
  }

  //
  // The base addresses for GDTR and IDTR are loaded from the
  // GDTR base-address field and the IDTR base-address field,
  // respectively.
  // If the processor supports the Intel 64 architecture and
  // the processor supports N < 64 linear address bits, each
  // of bits 63:N of each base address is set to the value of
  // bit N–1 of that base address.
  // The GDTR and IDTR limits are each set to FFFFH.
  // (ref: Vol3C[27.5.2(Loading Host Segment and Descriptor-Table Registers)])
  //

  x86::gdtr_t vcpu::host_gdtr() const noexcept
  {
    x86::gdtr_t gdtr;
    vmx::vmread(vmx::vmcs_fields::host_gdtr_base, gdtr.base_address);
    gdtr.limit = 0xffff;

    return gdtr;
  }

  void vcpu::host_gdtr(x86::gdtr_t gdtr) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::host_gdtr_base, gdtr.base_address);
  }

  x86::idtr_t vcpu::host_idtr() const noexcept
  {
    x86::idtr_t idtr;
    vmx::vmread(vmx::vmcs_fields::host_idtr_base, idtr.base_address);
    idtr.limit = 0xffff;

    return idtr;
  }

  void vcpu::host_idtr(x86::idtr_t idtr) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::host_idtr_base, idtr.base_address);
  }

  void vcpu::guest_sysenter_cs(uint64_t value) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::guest_sysenter_cs, value);
  }

  uint64_t vcpu::guest_sysenter_cs() const noexcept
  {
    uint64_t value;
    vmx::vmread(vmx::vmcs_fields::guest_sysenter_cs, value);

    return value;
  }

  void vcpu::guest_sysenter_eip(uint64_t value) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::guest_sysenter_eip, value);
  }

  uint64_t vcpu::guest_sysenter_eip() const noexcept
  {
    uint64_t value;
    vmx::vmread(vmx::vmcs_fields::guest_sysenter_eip, value);

    return value;
  }

  void vcpu::guest_sysenter_esp(uint64_t value) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::guest_sysenter_esp, value);
  }

  uint64_t vcpu::guest_sysenter_esp() const noexcept
  {
    uint64_t value;
    vmx::vmread(vmx::vmcs_fields::guest_sysenter_esp, value);

    return value;
  }

  x86::msr::efer_t vcpu::guest_efer() const noexcept
  {
    x86::msr::efer_t efer;
    vmx::vmread(vmx::vmcs_fields::guest_efer, efer);

    return efer;
  }

  void vcpu::guest_efer(x86::msr::efer_t efer) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::guest_efer, efer);
  }

  uint64_t vcpu::guest_activity_state() const noexcept
  {
    uint64_t activity_state;
    vmx::vmread(vmx::vmcs_fields::guest_activity_state, activity_state);

    return activity_state;
  }

  void vcpu::guest_activity_state(uint64_t value) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::guest_activity_state, value);
  }

  void vcpu::switch_guest_paging_mode(x86::cr0_t cr0) noexcept
  {
    x86::msr::efer_t efer = guest_efer();
    efer.fields.ia32e_mode_active = (cr0.flags.paging_enable & efer.fields.ia32e_mode_enable);
    guest_efer(efer);

    x86::msr::vmx_entry_ctls_t entry_cntrls = vm_entry_controls();
    entry_cntrls.flags.ia32e_mode_guest = efer.fields.ia32e_mode_active;
    vm_entry_controls(entry_cntrls, x86::msr::read<x86::msr::vmx_basic_msr_t>());
    vmx::invvpid_single_context(vpid());
  }

  void vcpu::host_sysenter_cs(uint64_t value) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::host_sysenter_cs, value);
  }

  uint64_t vcpu::host_sysenter_cs() const noexcept
  {
    uint64_t value;
    vmx::vmread(vmx::vmcs_fields::host_sysenter_cs, value);

    return value;
  }

  void vcpu::host_sysenter_eip(uint64_t value) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::host_sysenter_eip, value);
  }

  uint64_t vcpu::host_sysenter_eip() const noexcept
  {
    uint64_t value;
    vmx::vmread(vmx::vmcs_fields::host_sysenter_eip, value);

    return value;
  }

  void vcpu::host_sysenter_esp(uint64_t value) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::host_sysenter_esp, value);
  }

  uint64_t vcpu::host_sysenter_esp() const noexcept
  {
    uint64_t value;
    vmx::vmread(vmx::vmcs_fields::host_sysenter_esp, value);

    return value;
  }

  x86::msr::efer_t vcpu::host_efer() const noexcept
  {
    x86::msr::efer_t efer;
    vmx::vmread(vmx::vmcs_fields::host_efer, efer);

    return efer;
  }

  void vcpu::host_efer(x86::msr::efer_t efer) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::host_efer, efer);
  }

  exception_error_code vcpu::entry_interruption_error_code() const noexcept
  {
    exception_error_code result;
    vmx::vmread(vmx::vmcs_fields::vm_entry_exception_error_code, result);

    return result;
  }

  void vcpu::entry_interruption_error_code(exception_error_code error_code) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::vm_entry_exception_error_code, error_code);
  }

  void vcpu::entry_instruction_length(uint32_t instruction_length) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::vm_entry_instruction_len, instruction_length);
  }

  uint64_t vcpu::msr_bitmap() const noexcept
  {
    uint64_t address;
    vmx::vmread(vmx::vmcs_fields::msr_bitmap, address);

    return address;
  }

  void vcpu::msr_bitmap(uint64_t phys_address) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::msr_bitmap, phys_address);
  }

  uint64_t vcpu::ept_pointer() const noexcept
  {
    uint64_t pointer;
    vmx::vmread(vmx::vmcs_fields::ept_pointer, pointer);

    return pointer;
  }

  void vcpu::ept_pointer(uint64_t value) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::ept_pointer, value);
  }

  uint64_t vcpu::vpid() const noexcept
  {
    uint64_t value;
    vmx::vmread(vmx::vmcs_fields::virtual_processor_id, value);

    return value;
  }

  void vcpu::vpid(uint64_t value) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::virtual_processor_id, value);
  }

  vmx::instruction_error vcpu::vm_instruction_error() const noexcept
  {
    vmx::instruction_error result;
    vmx::vmread(vmx::vmcs_fields::vm_instruction_error, result);

    return result;
  }

  vmx::exception_bitmap vcpu::exception_bitmap() const noexcept
  {
    vmx::exception_bitmap result;
    vmx::vmread(vmx::vmcs_fields::exception_bitmap, result);

    return result;
  }

  void vcpu::exception_bitmap(vmx::exception_bitmap exception_bitmap) noexcept
  {
    vmx::vmwrite(vmx::vmcs_fields::exception_bitmap, exception_bitmap);
  }

  vmx::exit_qualification_t vcpu::exit_qualification() const noexcept
  {
    vmx::exit_qualification_t exit_qualification = {};
    vmx::vmread(vmx::vmcs_fields::exit_qualification, exit_qualification);

    return exit_qualification;
  }

  uint32_t vcpu::exit_instruction_length() const noexcept
  {
    uint32_t result;
    vmx::vmread(vmx::vmcs_fields::vm_exit_instruction_len, result);

    return result;
  }

  hook::hook_info** vcpu::mtf_restore_point() noexcept
  {
    return &guest_state_.mtf_ept_hook_restore_point;
  }

  void vcpu::set_monitor_trap_flag(bool set) noexcept
  {
    x86::msr::vmx_procbased_ctls_t ctls = cpu_based_vm_exec_control();
    ctls.flags.monitor_trap_flag = set;
    vmx::vmwrite(vmx::vmcs_fields::cpu_based_vm_exec_control, ctls);
  }

  void vcpu::resume_to_next_instruction() noexcept
  {
    if (!guest_state_.vmxoff_state.is_vmxoff_executed && guest_state_.increment_rip)
    {
      uint64_t rip = guest_rip();
      rip += exit_instruction_length();
      guest_rip(rip);
    }
  }

  void vcpu::skip_instruction(bool set) noexcept
  {
    guest_state_.increment_rip = set;
  }

  bool vcpu::skip_instruction() noexcept
  {
    return guest_state_.increment_rip;
  }

  void vcpu::set_nmi_window_exiting(bool set)
  {
    // Enable nmi window to inject NMI as soon as possible.

    x86::msr::vmx_procbased_ctls_t ctls = cpu_based_vm_exec_control();
    ctls.flags.nmi_window_exiting = set;
    vmx::vmwrite(vmx::vmcs_fields::cpu_based_vm_exec_control, ctls);
  }

  void vcpu::inject_interrupt(interrupt input_interrupt)
  {
    if (!input_interrupt.valid())
    {
      return;
    }

    entry_interruption_info(input_interrupt.vmx_interrupt_info());

    if (input_interrupt.type() == vmx::interrupt_type::hardware_exception)
    {
      switch (input_interrupt.vector())
      {
      case exception_vector::invalid_tss:
      case exception_vector::segment_not_present:
      case exception_vector::stack_segment_fault:
      case exception_vector::general_protection:
      case exception_vector::page_fault:
      case exception_vector::double_fault:
      case exception_vector::alignment_check:
        entry_interruption_error_code(input_interrupt.error_code());
        break;

      default:
        break;
      }
    }

    switch (input_interrupt.type())
    {
    case vmx::interrupt_type::external:
    case vmx::interrupt_type::nmi:
    case vmx::interrupt_type::hardware_exception:
    case vmx::interrupt_type::other_event:
    default:
      break;

    case vmx::interrupt_type::software:
    case vmx::interrupt_type::privileged_exception:
    case vmx::interrupt_type::software_exception:
      if (input_interrupt.rip_increment == -1)
      {
        input_interrupt.rip_increment = static_cast<int>(exit_instruction_length());
      }

      if (input_interrupt.rip_increment > 0)
      {
        entry_instruction_length(input_interrupt.rip_increment);
      }

      break;
    }
  }

  void vcpu::entry_interruption_info(vmx::interrupt_info info)
  {
    vmx::vmwrite(vmx::vmcs_fields::vm_entry_intr_info, info);
  }

  uint64_t vcpu::exit_guest_physical_address() const noexcept
  {
    uint64_t address;
    vmx::vmread(vmx::vmcs_fields::guest_physical_address, address);

    return address;
  }

  uint64_t vcpu::exit_guest_linear_address() const noexcept
  {
    uint64_t address;
    vmx::vmread(vmx::vmcs_fields::guest_linear_address, address);

    return address;
  }

  common::fxsave_area* vcpu::fxsave_area() noexcept
  {
    return fxsave_area_;
  }

  interrupt vcpu::get_exit_interrupt()
  {
    interrupt result = { exit_interruption_info(), exit_interruption_error_code(), static_cast<int>(exit_instruction_length()) };
    return result;
  }

  exception_error_code vcpu::exit_interruption_error_code() const noexcept
  {
    exception_error_code result;
    vmx::vmread(vmx::vmcs_fields::vm_exit_intr_error_code, result);

    return result;
  }

  vmx::interrupt_info vcpu::exit_interruption_info() const noexcept
  {
    vmx::interrupt_info result;
    vmx::vmread(vmx::vmcs_fields::vm_exit_intr_info, result);

    return result;
  }
}
