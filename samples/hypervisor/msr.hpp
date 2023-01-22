#pragma once
#include <cstdint>

namespace hh
{
  namespace x86
  {
    struct rflags_t;

    namespace msr
    {
      inline constexpr uint32_t reserved_msr_range_low = 0x40000000;
      inline constexpr uint32_t reserved_msr_range_hi = 0x400000f0;

      struct x2apic
      {
        inline static constexpr uint32_t to_x2(uint32_t x)
        {
          return x / 0x10;
        }

        static constexpr uint32_t msr_id = 0x800;
        using result_type = uint64_t;
      };

      struct apic_base
      {
        static constexpr uint32_t msr_id = 0x01b;
        using result_type = uint64_t;
      };

      struct feature_control
      {
        static constexpr uint32_t msr_id = 0x03a;
        using result_type = uint64_t;
      };

      struct vmx_basic
      {
        static constexpr uint32_t msr_id = 0x480;
        using result_type = uint64_t;
      };

      struct vmx_misc
      {
        static constexpr uint32_t msr_id = 0x485;
        using result_type = uint64_t;
      };

      struct vmx_cr0_fixed0
      {
        static constexpr uint32_t msr_id = 0x486;
        using result_type = uint64_t;
      };

      struct vmx_cr0_fixed1
      {
        static constexpr uint32_t msr_id = 0x487;
        using result_type = uint64_t;
      };

      struct vmx_cr4_fixed0
      {
        static constexpr uint32_t msr_id = 0x488;
        using result_type = uint64_t;
      };

      struct vmx_cr4_fixed1
      {
        static constexpr uint32_t msr_id = 0x489;
        using result_type = uint64_t;
      };

      struct vmx_vmcs_enum
      {
        static constexpr uint32_t msr_id = 0x48a;
        using result_type = uint64_t;
      };

      struct vmx_entry_ctls_t
      {
        static constexpr uint32_t msr_id = 0x00000484;
        using result_type = vmx_entry_ctls_t;

        union
        {
          uint64_t all;

          struct
          {
            uint64_t reserved_1 : 2;
            /**
            * @brief Load guest debug controls (dr7 & IA32_DEBUGCTL_MSR) (forced to 1 on the 'first' VT-x capable CPUs; this actually
            *        includes the newest Nehalem CPUs)
            *
            * [Bit 2] This control determines whether DR7 and the IA32_DEBUGCTL MSR are loaded on VM entry. The first processors to
            * support the virtual-machine extensions supported only the 1-setting of this control.
            */
            uint64_t load_debug_controls : 1;
            uint64_t reserved_2 : 6;
            /**
            * @brief 64 bits guest mode. Must be 0 for CPUs that don't support AMD64
            *
            * [Bit 9] On processors that support Intel 64 architecture, this control determines whether the logical processor is in
            * IA-32e mode after VM entry. Its value is loaded into IA32_EFER.LMA as part of VM entry. This control must be 0 on
            * processors that do not support Intel 64 architecture.
            */
            uint64_t ia32e_mode_guest : 1;
            /**
            * @brief In SMM mode after VM-entry
            *
            * [Bit 10] This control determines whether the logical processor is in system-management mode (SMM) after VM entry. This
            * control must be 0 for any VM entry from outside SMM.
            */
            uint64_t entry_to_smm : 1;
            /**
            * @brief Disable dual treatment of SMI and SMM; must be zero for VM-entry outside of SMM
            *
            * [Bit 11] If set to 1, the default treatment of SMIs and SMM is in effect after the VM entry. This control must be 0 for
            * any VM entry from outside SMM
            *
            * @see Vol3C[34.15.7(Deactivating the Dual-Monitor Treatment)]
            */
            uint64_t deactivate_dual_monitor_treatment : 1;
            uint64_t reserved_3 : 1;
            /**
            * @brief Whether the guest IA32_PERF_GLOBAL_CTRL MSR is loaded on VM-entry
            *
            * [Bit 13] This control determines whether the IA32_PERF_GLOBAL_CTRL MSR is loaded on VM entry.
            */
            uint64_t load_ia32_perf_global_ctrl : 1;
            /**
            * @brief Whether the guest IA32_PAT MSR is loaded on VM-entry
            *
            * [Bit 14] This control determines whether the IA32_PAT MSR is loaded on VM entry.
            */
            uint64_t load_ia32_pat : 1;
            /**
            * @brief Whether the guest IA32_EFER MSR is loaded on VM-entry
            *
            * [Bit 15] This control determines whether the IA32_EFER MSR is loaded on VM entry.
            */
            uint64_t load_ia32_efer : 1;
            /**
            * [Bit 16] This control determines whether the IA32_BNDCFGS MSR is loaded on VM entry.
            */
            uint64_t load_ia32_bndcfgs : 1;
            /**
            * [Bit 17] If this control is 1, Intel Processor Trace does not produce a paging information packet (PIP) on a VM entry or
            * a VMCS packet on a VM entry that returns from SMM.
            *
            * @see Vol3C[35(INTEL(R) PROCESSOR TRACE)]
            */
            uint64_t conceal_vmx_from_pt : 1;
          }flags;
        };
      };

      struct vmx_procbased_ctls2_t
      {
        static constexpr uint32_t msr_id = 0x0000048B;
        using result_type = vmx_procbased_ctls2_t;

        union
        {
          uint64_t all;

          struct
          {
            /**
             * @brief Virtualize APIC access
             *
             * [Bit 0] If this control is 1, the logical processor treats specially accesses to the page with the APICaccess address.
             *
             * @see Vol3C[29.4(VIRTUALIZING MEMORY-MAPPED APIC ACCESSES)]
             */
            uint64_t virtualize_apic_accesses : 1;
            /**
             * @brief EPT supported/enabled
             *
             * [Bit 1] If this control is 1, extended page tables (EPT) are enabled.
             *
             * @see Vol3C[28.2(THE EXTENDED PAGE TABLE MECHANISM (EPT))]
             */
            uint64_t enable_ept : 1;
            /**
             * @brief Descriptor table instructions cause VM-exits
             *
             * [Bit 2] This control determines whether executions of LGDT, LIDT, LLDT, LTR, SGDT, SIDT, SLDT, and STR cause VM exits.
             */
            uint64_t descriptor_table_exiting : 1;
            /**
             * @brief RDTSCP supported/enabled
             *
             * [Bit 3] If this control is 0, any execution of RDTSCP causes an invalid-opcode exception (\#UD).
             */
            uint64_t enable_rdtscp : 1;
            /**
             * @brief Virtualize x2APIC mode
             *
             * [Bit 4] If this control is 1, the logical processor treats specially RDMSR and WRMSR to APIC MSRs (in the range
             * 800H-8FFH).
             *
             * @see Vol3C[29.5(VIRTUALIZING MSR-BASED APIC ACCESSES)]
             */
            uint64_t virtualize_x2apic_mode : 1;
            /**
             * @brief VPID supported/enabled
             *
             * [Bit 5] If this control is 1, cached translations of linear addresses are associated with a virtualprocessor identifier
             * (VPID).
             *
             * @see Vol3C[28.1(VIRTUAL PROCESSOR IDENTIFIERS (VPIDS))]
             */
            uint64_t enable_vpid : 1;
            /**
             * @brief VM-exit when executing the WBINVD instruction
             *
             * [Bit 6] This control determines whether executions of WBINVD cause VM exits.
             */
            uint64_t wbinvd_exiting : 1;
            /**
             * @brief Unrestricted guest execution
             *
             * [Bit 7] This control determines whether guest software may run in unpaged protected mode or in realaddress mode.
             */
            uint64_t unrestricted_guest : 1;
            /**
             * @brief APIC register virtualization
             *
             * [Bit 8] If this control is 1, the logical processor virtualizes certain APIC accesses.
             *
             * @see Vol3C[29.4(VIRTUALIZING MEMORY-MAPPED APIC ACCESSES)]
             * @see Vol3C[29.5(VIRTUALIZING MSR-BASED APIC ACCESSES)]
             */
            uint64_t apic_register_virtualization : 1;
            /**
             * @brief Virtual-interrupt delivery
             *
             * [Bit 9] This controls enables the evaluation and delivery of pending virtual interrupts as well as the emulation of
             * writes to the APIC registers that control interrupt prioritization.
             */
            uint64_t virtual_interrupt_delivery : 1;
            /**
             * @brief A specified number of pause loops cause a VM-exit
             *
             * [Bit 10] This control determines whether a series of executions of PAUSE can cause a VM exit.
             *
             * @see Vol3C[24.6.13(Controls for PAUSE-Loop Exiting)]
             * @see Vol3C[25.1.3(Instructions That Cause VM Exits Conditionally)]
             */
            uint64_t pause_loop_exiting : 1;
            /**
             * @brief VM-exit when executing RDRAND instructions
             *
             * [Bit 11] This control determines whether executions of RDRAND cause VM exits.
             */
            uint64_t rdrand_exiting : 1;
            /**
             * @brief Enables INVPCID instructions
             *
             * [Bit 12] If this control is 0, any execution of INVPCID causes a \#UD.
             */
            uint64_t enable_invpcid : 1;
            /**
             * @brief Enables VMFUNC instructions
             *
             * [Bit 13] Setting this control to 1 enables use of the VMFUNC instruction in VMX non-root operation.
             *
             * @see Vol3C[25.5.5(VM Functions)]
             */
            uint64_t enable_vm_functions : 1;
            /**
             * @brief Enables VMCS shadowing
             *
             * [Bit 14] If this control is 1, executions of VMREAD and VMWRITE in VMX non-root operation may access a shadow VMCS
             * (instead of causing VM exits).
             *
             * @see {'Vol3C[24.10(VMCS TYPES': 'ORDINARY AND SHADOW)]'}
             * @see Vol3C[30.3(VMX INSTRUCTIONS)]
             */
            uint64_t vmcs_shadowing : 1;
            /**
             * @brief Enables ENCLS VM-exits
             *
             * [Bit 15] If this control is 1, executions of ENCLS consult the ENCLS-exiting bitmap to determine whether the instruction
             * causes a VM exit.
             *
             * @see Vol3C[24.6.16(ENCLS-Exiting Bitmap)]
             * @see Vol3C[25.1.3(Instructions That Cause VM Exits Conditionally)]
             */
            uint64_t enable_encls_exiting : 1;
            /**
             * @brief VM-exit when executing RDSEED
             *
             * [Bit 16] This control determines whether executions of RDSEED cause VM exits.
             */
            uint64_t rdseed_exiting : 1;
            /**
             * @brief Enables page-modification logging
             *
             * [Bit 17] If this control is 1, an access to a guest-physical address that sets an EPT dirty bit first adds an entry to
             * the page-modification log.
             *
             * @see Vol3C[28.2.5(Page-Modification Logging)]
             */
            uint64_t enable_pml : 1;
            /**
             * @brief Controls whether EPT-violations may cause
             *
             * [Bit 18] If this control is 1, EPT violations may cause virtualization exceptions (\#VE) instead of VM exits.
             *
             * @see Vol3C[25.5.6(Virtualization Exceptions)]
             */
            uint64_t ept_violation_ve : 1;
            /**
             * @brief Conceal VMX non-root operation from Intel processor trace (PT)
             *
             * [Bit 19] If this control is 1, Intel Processor Trace suppresses from PIPs an indication that the processor was in VMX
             * non-root operation and omits a VMCS packet from any PSB+ produced in VMX nonroot operation.
             *
             * @see Vol3C[35(INTEL(R) PROCESSOR TRACE)]
             */
            uint64_t conceal_vmx_from_pt : 1;
            /**
             * @brief Enables XSAVES/XRSTORS instructions
             *
             * [Bit 20] If this control is 0, any execution of XSAVES or XRSTORS causes a \#UD.
             */
            uint64_t enable_xsaves : 1;
            /**
             * [Bit 22] If this control is 1, EPT execute permissions are based on whether the linear address being accessed is
             * supervisor mode or user mode.
             *
             * @see Vol3C[28(VMX SUPPORT FOR ADDRESS TRANSLATION)]
             */
            uint64_t reserved_1 : 1;
            /**
             * [Bit 22] If this control is 1, EPT execute permissions are based on whether the linear address being accessed is
             * supervisor mode or user mode.
             *
             * @see Vol3C[28(VMX SUPPORT FOR ADDRESS TRANSLATION)]
             */
            uint64_t mode_based_execute_control_for_ept : 1;
            uint64_t reserved_2 : 2;
            /**
             * @brief Use TSC scaling
             *
             * [Bit 25] This control determines whether executions of RDTSC, executions of RDTSCP, and executions of RDMSR that read
             * from the IA32_TIME_STAMP_COUNTER MSR return a value modified by the TSC multiplier field.
             *
             * @see Vol3C[24.6.5(Time-Stamp Counter Offset and Multiplier)]
             * @see Vol3C[25.3(CHANGES TO INSTRUCTION BEHAVIOR IN VMX NON-ROOT OPERATION)]
             */
            uint64_t use_tsc_scaling : 1;
          }flags;
        };
      };

      union mtrr_physbase_register_t
      {
        static constexpr uint32_t msr_id = 0x00000200;
        using result_type = mtrr_physbase_register_t;

        uint64_t all;

        struct
        {
          /**
          * [Bits 7:0] Specifies the memory type for the range.
          */
          uint64_t type : 8;
          uint64_t reserved_1 : 4;

          /**
          * [Bits 47:12] Specifies the base address of the address range. This 24-bit value, in the case where MAXPHYADDR is 36
          * bits, is extended by 12 bits at the low end to form the base address (this automatically aligns the address on a 4-KByte
          * boundary).
          */
          uint64_t page_frame_number : 36;
          uint64_t reserved_2 : 16;
        }flags;
      };

      union mtrr_physmask_register_t
      {
        static constexpr uint32_t msr_id = 0x00000201;
        using result_type = mtrr_physmask_register_t;

        uint64_t all;

        struct
        {
          /**
          * [Bits 7:0] Specifies the memory type for the range.
          */
          uint64_t type : 8;
          uint64_t reserved_1 : 3;

          /**
          * [Bit 11] Enables the register pair when set; disables register pair when clear.
          */
          uint64_t valid : 1;

          /**
          * [Bits 47:12] Specifies a mask (24 bits if the maximum physical address size is 36 bits, 28 bits if the maximum physical
          * address size is 40 bits). The mask determines the range of the region being mapped, according to the following
          * relationships:
          * - Address_Within_Range AND PhysMask = PhysBase AND PhysMask
          * - This value is extended by 12 bits at the low end to form the mask value.
          * - The width of the PhysMask field depends on the maximum physical address size supported by the processor.
          * CPUID.80000008H reports the maximum physical address size supported by the processor. If CPUID.80000008H is not
          * available, software may assume that the processor supports a 36-bit physical address size.
          *
          * @see Vol3A[11.11.3(Example Base and Mask Calculations)]
          */
          uint64_t page_frame_number : 36;
          uint64_t reserved_2 : 16;
        }flags;
      };

      union mttr_capabilities_register_t
      {
        static constexpr uint32_t msr_id = 0x000000FE;
        using result_type = mttr_capabilities_register_t;

        uint64_t all;

        struct
        {
          /**
          * @brief VCNT (variable range registers count) field
          *
          * [Bits 7:0] Indicates the number of variable ranges implemented on the processor.
          */
          uint64_t variable_range_count : 8;

          /**
          * @brief FIX (fixed range registers supported) flag
          *
          * [Bit 8] Fixed range MTRRs (MSR_IA32_MTRR_FIX64K_00000 through MSR_IA32_MTRR_FIX4K_0F8000) are supported when set; no fixed range
          * registers are supported when clear.
          */
          uint64_t fixed_range_supported : 1;
          uint64_t reserved_1 : 1;

          /**
          * @brief WC (write combining) flag
          *
          * [Bit 10] The write-combining (WC) memory type is supported when set; the WC type is not supported when clear.
          */
          uint64_t wc_supported : 1;

          /**
          * @brief SMRR (System-Management Range Register) flag
          *
          * [Bit 11] The system-management range register (SMRR) interface is supported when bit 11 is set; the SMRR interface is
          * not supported when clear.
          */
          uint64_t smrr_supported : 1;
          uint64_t reserved_2 : 52;
        }flags;
      };

      union vmx_ept_vpid_cap_register_t
      {
        static constexpr uint32_t msr_id = 0x48c;
        using result_type = vmx_ept_vpid_cap_register_t;

        uint64_t all;

        struct
        {
          /**
          * [Bit 0] When set to 1, the processor supports execute-only translations by EPT. This support allows software to
          * configure EPT paging-structure entries in which bits 1:0 are clear (indicating that data accesses are not allowed) and
          * bit 2 is set (indicating that instruction fetches are allowed).
          */
          uint64_t execute_only_pages : 1;
          uint64_t reserved_1 : 5;

          /**
          * [Bit 6] Indicates support for a page-walk length of 4.
          */
          uint64_t page_walk_length_4 : 1;
          uint64_t reserved_2 : 1;

          /**
          * [Bit 8] When set to 1, the logical processor allows software to configure the EPT paging-structure memory type to be
          * uncacheable (UC).
          *
          * @see Vol3C[24.6.11(Extended-Page-Table Pointer (EPTP))]
          */
          uint64_t memory_type_uncacheable : 1;
          uint64_t reserved_3 : 5;

          /**
          * [Bit 14] When set to 1, the logical processor allows software to configure the EPT paging-structure memory type to be
          * write-back (WB).
          */
          uint64_t memory_type_write_back : 1;
          uint64_t reserved_4 : 1;

          /**
          * [Bit 16] When set to 1, the logical processor allows software to configure a EPT PDE to map a 2-Mbyte page (by setting
          * bit 7 in the EPT PDE).
          */
          uint64_t pde_2mb_pages : 1;

          /**
          * [Bit 17] When set to 1, the logical processor allows software to configure a EPT PDPTE to map a 1-Gbyte page (by setting
          * bit 7 in the EPT PDPTE).
          */
          uint64_t pdpte_1gb_pages : 1;
          uint64_t reserved_5 : 2;

          /**
          * [Bit 20] If bit 20 is read as 1, the INVEPT instruction is supported.
          *
          * @see Vol3C[30(VMX INSTRUCTION REFERENCE)]
          * @see Vol3C[28.3.3.1(Operations that Invalidate Cached Mappings)]
          */
          uint64_t invept : 1;

          /**
          * [Bit 21] When set to 1, accessed and dirty flags for EPT are supported.
          *
          * @see Vol3C[28.2.4(Accessed and Dirty Flags for EPT)]
          */
          uint64_t ept_accessed_and_dirty_flags : 1;

          /**
          * [Bit 22] When set to 1, the processor reports advanced VM-exit information for EPT violations. This reporting is done
          * only if this bit is read as 1.
          *
          * @see Vol3C[27.2.1(Basic VM-Exit Information)]
          */
          uint64_t advanced_vmexit_ept_violations_information : 1;
          uint64_t reserved_6 : 2;

          /**
          * [Bit 25] When set to 1, the single-context INVEPT type is supported.
          *
          * @see Vol3C[30(VMX INSTRUCTION REFERENCE)]
          * @see Vol3C[28.3.3.1(Operations that Invalidate Cached Mappings)]
          */
          uint64_t invept_single_context : 1;

          /**
          * [Bit 26] When set to 1, the all-context INVEPT type is supported.
          *
          * @see Vol3C[30(VMX INSTRUCTION REFERENCE)]
          * @see Vol3C[28.3.3.1(Operations that Invalidate Cached Mappings)]
          */
          uint64_t invept_all_contexts : 1;
          uint64_t reserved_7 : 5;

          /**
          * [Bit 32] When set to 1, the INVVPID instruction is supported.
          */
          uint64_t invvpid : 1;
          uint64_t reserved_8 : 7;

          /**
          * [Bit 40] When set to 1, the individual-address INVVPID type is supported.
          */
          uint64_t invvpid_individual_address : 1;

          /**
          * [Bit 41] When set to 1, the single-context INVVPID type is supported.
          */
          uint64_t invvpid_single_context : 1;

          /**
          * [Bit 42] When set to 1, the all-context INVVPID type is supported.
          */
          uint64_t invvpid_all_contexts : 1;

          /**
          * [Bit 43] When set to 1, the single-context-retaining-globals INVVPID type is supported.
          */
          uint64_t invvpid_single_context_retain_globals : 1;
          uint64_t reserved_9 : 20;
        }flags;
      };

      struct vmx_true_pinbased_ctls
      {
        static constexpr uint32_t msr_id = 0x48d;
        using result_type = uint64_t;
      };

      struct vmx_true_exit_ctls
      {
        static constexpr uint32_t msr_id = 0x48f;
        using result_type = uint64_t;
      };

      struct vmx_true_entry_ctls
      {
        static constexpr uint32_t msr_id = 0x490;
        using result_type = uint64_t;
      };

      struct vmx_vmfunc
      {
        static constexpr uint32_t msr_id = 0x491;
        using result_type = uint64_t;
      };

      struct sysenter_cs
      {
        static constexpr uint32_t msr_id = 0x174;
        using result_type = uint64_t;
      };

      struct sysenter_esp
      {
        static constexpr uint32_t msr_id = 0x175;
        using result_type = uint64_t;
      };

      struct sysenter_eip
      {
        static constexpr uint32_t msr_id = 0x176;
        using result_type = uint64_t;
      };

      struct debugctl
      {
        static constexpr uint32_t msr_id = 0x1d9;
        using result_type = uint64_t;
      };

      struct shadow_gs_base
      {
        static constexpr uint32_t msr_id = 0xc0000102;
        using result_type = uint64_t;
      };

      inline constexpr uint32_t low_msr_high_range = 0x00001FFF;
      inline constexpr uint32_t high_msr_low_range = 0xC0000000;
      inline constexpr uint32_t high_msr_high_range = 0xC0001FFF;

      inline constexpr uint32_t low_msr_write_bitmap_offset = 2048;
      inline constexpr uint32_t high_msr_read_bitmap_offset = 1024;
      inline constexpr uint32_t high_msr_write_bitmap_offset = 3072;

      struct apic_base_t
      {
        static constexpr uint32_t msr_id = 0x0000001b;
        using result_type = apic_base_t;

        union
        {
          uint64_t flags;

          struct
          {
            uint64_t reserved_1 : 8;
            uint64_t bsp_flag : 1;
            uint64_t reserved_2 : 1;
            uint64_t enable_x2apic_mode : 1;
            uint64_t apic_global_enable : 1;
            uint64_t page_frame_number : 36;
          };
        };
      };

      struct debugctl_t
      {
        static constexpr uint32_t msr_id = 0x000001d9;
        using result_type = debugctl_t;

        union
        {
          uint64_t all;

          struct
          {
            /**
             * [Bit 0] Setting this bit to 1 enables the processor to record a running trace of the most recent branches taken by the
             * processor in the LBR stack.
             *
             * @remarks 06_01H
             */
            uint64_t lbr : 1;
            /**
             * [Bit 1] Setting this bit to 1 enables the processor to treat EFLAGS.TF as single-step on branches instead of single-step
             * on instructions.
             *
             * @remarks 06_01H
             */
            uint64_t btf : 1;
            uint64_t reserved_1 : 4;
            /**
             * [Bit 6] Setting this bit to 1 enables branch trace messages to be sent.
             *
             * @remarks 06_0EH
             */
            uint64_t tr : 1;
            /**
             * [Bit 7] Setting this bit enables branch trace messages (BTMs) to be logged in a BTS buffer.
             *
             * @remarks 06_0EH
             */
            uint64_t bts : 1;
            /**
             * [Bit 8] When clear, BTMs are logged in a BTS buffer in circular fashion. When this bit is set, an interrupt is generated
             * by the BTS facility when the BTS buffer is full.
             *
             * @remarks 06_0EH
             */
            uint64_t btint : 1;
            /**
             * [Bit 9] When set, BTS or BTM is skipped if CPL = 0.
             *
             * @remarks 06_0FH
             */
            uint64_t bts_off_os : 1;
            /**
             * [Bit 10] When set, BTS or BTM is skipped if CPL > 0.
             *
             * @remarks 06_0FH
             */
            uint64_t bts_off_usr : 1;
            /**
             * [Bit 11] When set, the LBR stack is frozen on a PMI request.
             *
             * @remarks If CPUID.01H: ECX[15] = 1 && CPUID.0AH: EAX[7:0] > 1
             */
            uint64_t freeze_lbrs_on_pmi : 1;
            /**
             * [Bit 12] When set, each ENABLE bit of the global counter control MSR are frozen (address 38FH) on a PMI request.
             *
             * @remarks If CPUID.01H: ECX[15] = 1 && CPUID.0AH: EAX[7:0] > 1
             */
            uint64_t freeze_perfmon_on_pmi : 1;
            /**
             * [Bit 13] When set, enables the logical processor to receive and generate PMI on behalf of the uncore.
             *
             * @remarks 06_1AH
             */
            uint64_t enable_uncore_pmi : 1;
            /**
             * [Bit 14] When set, freezes perfmon and trace messages while in SMM.
             *
             * @remarks If IA32_PERF_CAPABILITIES[12] = 1
             */
            uint64_t freeze_while_smm : 1;
            /**
             * [Bit 15] When set, enables DR7 debug bit on XBEGIN.
             *
             * @remarks If (CPUID.(EAX=07H, ECX=0):EBX[11] = 1)
             */
            uint64_t rtm_debug : 1;
          }fields;
        };
      };

      struct efer_t
      {
        static constexpr uint32_t msr_id = 0xc0000080;
        using result_type = efer_t;

        union
        {
          uint64_t all;

          struct
          {
            /**
             * @brief SYSCALL Enable <b>(R/W)</b>
             *
             * [Bit 0] Enables SYSCALL/SYSRET instructions in 64-bit mode.
             */
            uint64_t syscall_enable : 1;
            uint64_t reserved1 : 7;
            /**
             * @brief IA-32e Mode Enable <b>(R/W)</b>
             *
             * [Bit 8] Enables IA-32e mode operation.
             */
            uint64_t ia32e_mode_enable : 1;
            uint64_t reserved2 : 1;
            /**
             * @brief IA-32e Mode Active <b>(R)</b>
             *
             * [Bit 10] Indicates IA-32e mode is active when set.
             */
            uint64_t ia32e_mode_active : 1;
            /**
             * [Bit 11] Execute Disable Bit Enable.
             */
            uint64_t execute_disable_bit_enable : 1;
            uint64_t reserved3 : 52;
          }fields;
        };
      };

      struct feature_control_msr_t
      {
        static constexpr uint32_t msr_id = 0x03A;
        using result_type = feature_control_msr_t;

        union
        {
          uint64_t all;

          struct
          {
            /**
             * @brief Lock bit <b>(R/WO)</b>
             *
             * [Bit 0] When set, locks this MSR from being written; writes to this bit will result in GP(0).
             *
             * @note Once the Lock bit is set, the contents of this register cannot be modified. Therefore the lock bit must be set
             *       after configuring support for Intel Virtualization Technology and prior to transferring control to an option ROM or the
             *       OS. Hence, once the Lock bit is set, the entire IA32_FEATURE_CONTROL contents are preserved across RESET when PWRGOOD is
             *       not deasserted.
             * @remarks If any one enumeration condition for defined bit field position greater than bit 0 holds.
             */
            uint64_t lock : 1;
            /**
             * @brief Enable VMX inside SMX operation <b>(R/WL)</b>
             *
             * [Bit 1] This bit enables a system executive to use VMX in conjunction with SMX to support Intel(R) Trusted Execution
             * Technology. BIOS must set this bit only when the CPUID function 1 returns VMX feature flag and SMX feature flag set (ECX
             * bits 5 and 6 respectively).
             *
             * @remarks If CPUID.01H:ECX[5] = 1 && CPUID.01H:ECX[6] = 1
             */
            uint64_t enable_smx : 1;
            /**
             * @brief Enable VMX outside SMX operation <b>(R/WL)</b>
             *
             * [Bit 2] This bit enables VMX for a system executive that does not require SMX. BIOS must set this bit only when the
             * CPUID function 1 returns the VMX feature flag set (ECX bit 5).
             *
             * @remarks If CPUID.01H:ECX[5] = 1
             */
            uint64_t enable_vmxon : 1;
            uint64_t reserved_2 : 5;
            /**
             * @brief SENTER Local Function Enable <b>(R/WL)</b>
             *
             * [Bits 14:8] When set, each bit in the field represents an enable control for a corresponding SENTER function. This field
             * is supported only if CPUID.1:ECX.[bit 6] is set.
             *
             * @remarks If CPUID.01H:ECX[6] = 1
             */
            uint64_t enable_local_senter : 7;
            /**
             * @brief SENTER Global Enable <b>(R/WL)</b>
             *
             * [Bit 15] This bit must be set to enable SENTER leaf functions. This bit is supported only if CPUID.1:ECX.[bit 6] is set.
             *
             * @remarks If CPUID.01H:ECX[6] = 1
             */
            uint64_t enable_global_senter : 1;
            uint64_t reserved_3a : 16;
            uint64_t reserved_3b : 32;
          } fields;
        };
      };

      struct vmx_basic_msr_t
      {
        inline static constexpr uint32_t msr_id = 0x480;
        using result_type = vmx_basic_msr_t;

        union
        {
          uint64_t all;

          struct
          {
            /**
             * @brief VMCS revision identifier used by the processor
             *
             * [Bits 30:0] 31-bit VMCS revision identifier used by the processor. Processors that use the same VMCS revision identifier
             * use the same size for VMCS regions.
             */
            uint32_t revision_identifier : 31;
            /**
             * [Bit 31] Bit 31 is always 0.
             */
            uint32_t reserved_1 : 1;
            /**
             * @brief Size of the VMCS
             *
             * [Bits 44:32] Report the number of bytes that software should allocate for the VMXON region and any VMCS region. It is a
             * value greater than 0 and at most 4096 (bit 44 is set if and only if bits 43:32 are clear).
             */
            uint32_t region_size : 12;
            uint32_t region_clear : 1;
            uint32_t reserved_2 : 3;
            /**
              * @brief Width of physical address used for the VMCS
              *        - 0 -> limited to the available amount of physical RAM
              *        - 1 -> within the first 4 GB
              *
              * [Bit 48] Indicates the width of the physical addresses that may be used for the VMXON region, each VMCS, and data
              * structures referenced by pointers in a VMCS (I/O bitmaps, virtual-APIC page, MSR areas for VMX transitions). If the bit
              * is 0, these addresses are limited to the processor's physical-address width.2 If the bit is 1, these addresses are
              * limited to 32 bits. This bit is always 0 for processors that support Intel 64 architecture.
              */
            uint32_t supported_ia64 : 1;
            /**
             * @brief Whether the processor supports the dual-monitor treatment of system-management interrupts and system-management
             *        code (always 1)
             *
             * [Bit 49] Read as 1, the logical processor supports the dual-monitor treatment of system-management interrupts and
             * system-management mode.
             *
             * @see Vol3C[34.15(DUAL-MONITOR TREATMENT OF SMIs AND SMM)]
             */
            uint32_t supported_dual_moniter : 1;
            /**
             * @brief Memory type that must be used for the VMCS
             *
             * [Bits 53:50] Report the memory type that should be used for the VMCS, for data structures referenced by pointers in the
             * VMCS (I/O bitmaps, virtual-APIC page, MSR areas for VMX transitions), and for the MSEG header. If software needs to
             * access these data structures (e.g., to modify the contents of the MSR bitmaps), it can configure the paging structures
             * to map them into the linear-address space. If it does so, it should establish mappings that use the memory type reported
             * bits 53:50 in this MSR.
             * As of this writing, all processors that support VMX operation indicate the write-back type.
             */
            uint32_t memory_type : 4;
            /**
             * @brief Whether the processor provides additional information for exits due to INS/OUTS
             *
             * [Bit 54] When set to 1, the processor reports information in the VM-exit instruction-information field on VM exits due
             * to execution of the INS and OUTS instructions. This reporting is done only if this bit is read as 1.
             *
             * @see Vol3C[27.2.4(Information for VM Exits Due to Instruction Execution)]
             */
            uint32_t vmexit_report : 1;
            /**
             * @brief Whether default 1 bits in control MSRs (pin/proc/exit/entry) may be cleared to 0 and that 'true' control MSRs are
             *        supported
             *
             * [Bit 55] Is read as 1 if any VMX controls that default to 1 may be cleared to 0. It also reports support for the VMX
             * capability MSRs IA32_VMX_TRUE_PINBASED_CTLS, IA32_VMX_TRUE_PROCBASED_CTLS, IA32_VMX_TRUE_EXIT_CTLS, and
             * IA32_VMX_TRUE_ENTRY_CTLS.
             *
             * @see Vol3D[A.2(RESERVED CONTROLS AND DEFAULT SETTINGS)]
             * @see Vol3D[A.3.1(Pin-Based VM-Execution Controls)]
             * @see Vol3D[A.3.2(Primary Processor-Based VM-Execution Controls)]
             * @see Vol3D[A.4(VM-EXIT CONTROLS)]
             * @see Vol3D[A.5(VM-ENTRY CONTROLS)]
             */
            uint32_t vmx_capability_hint : 1;
            uint32_t reserved_3 : 8;
          } fields;
        };
      };

      // @see Vol3D[A.3.1(Pin-Based VM-Execution Controls)]
      struct vmx_pinbased_ctls_t
      {
        static constexpr uint32_t msr_id = 0x00000481;
        using result_type = vmx_pinbased_ctls_t;

        union
        {
          uint64_t all;

          struct
          {
            uint64_t external_interrupt_exiting : 1;
            uint64_t reserved_1 : 2;
            uint64_t nmi_exiting : 1;
            uint64_t reserved_2 : 1;
            uint64_t virtual_nmis : 1;
            uint64_t activate_vmx_preemption_timer : 1;
            uint64_t process_posted_interrupts : 1;
          }flags;
        };
      };

      // @see Vol3D[A.3.2(Primary Processor - Based VM - Execution Controls)]
      struct vmx_procbased_ctls_t
      {
        static constexpr uint32_t msr_id = 0x00000482;
        using result_type = vmx_procbased_ctls_t;
        union
        {
          uint64_t all;

          struct
          {
            uint64_t reserved_1 : 2;
            uint64_t interrupt_window_exiting : 1;
            uint64_t use_tsc_offsetting : 1;
            uint64_t reserved_2 : 3;
            uint64_t hlt_exiting : 1;
            uint64_t reserved_3 : 1;
            uint64_t invlpg_exiting : 1;
            uint64_t mwait_exiting : 1;
            uint64_t rdpmc_exiting : 1;
            uint64_t rdtsc_exiting : 1;
            uint64_t reserved_4 : 2;
            uint64_t cr3_load_exiting : 1;
            uint64_t cr3_store_exiting : 1;
            uint64_t reserved_5 : 2;
            uint64_t cr8_load_exiting : 1;
            uint64_t cr8_store_exiting : 1;
            uint64_t use_tpr_shadow : 1;
            uint64_t nmi_window_exiting : 1;
            uint64_t mov_dr_exiting : 1;
            uint64_t unconditional_io_exiting : 1;
            uint64_t use_io_bitmaps : 1;
            uint64_t reserved_6 : 1;
            uint64_t monitor_trap_flag : 1;
            uint64_t use_msr_bitmaps : 1;
            uint64_t monitor_exiting : 1;
            uint64_t pause_exiting : 1;
            uint64_t activate_secondary_controls : 1;
          }flags;
        };
      };

      // @see Vol3D[A.4(VM-EXIT CONTROLS)]
      struct vmx_exit_ctls_t
      {
        static constexpr uint32_t msr_id = 0x00000483;
        using result_type = vmx_exit_ctls_t;

        union
        {
          uint64_t all;

          struct
          {
            uint64_t reserved_1 : 2;
            uint64_t save_debug_controls : 1;
            uint64_t reserved_2 : 6;
            uint64_t ia32e_mode_host : 1;
            uint64_t reserved_3 : 2;
            uint64_t load_perf_global_ctrl : 1;
            uint64_t reserved_4 : 2;
            uint64_t acknowledge_interrupt_on_exit : 1;
            uint64_t reserved_5 : 2;
            uint64_t save_pat : 1;
            uint64_t load_pat : 1;
            uint64_t save_efer : 1;
            uint64_t load_efer : 1;
            uint64_t save_vmx_preemption_timer_value : 1;
            uint64_t clear_bndcfgs : 1;
            uint64_t conceal_vmx_from_pt : 1;
          }flags;
        };
      };

      union def_type_register_t
      {
        static constexpr uint32_t msr_id = 0x000002FF;
        using result_type = def_type_register_t;

        uint64_t all;

        struct
        {
          uint64_t default_memory_type : 3;
          uint64_t reserved_1 : 7;
          uint64_t fixed_range_mtrr_enable : 1;
          uint64_t mtrr_enable : 1;
          uint64_t reserved_2 : 52;
        }flags;
      };

      struct vmx_true_procbased_ctls
      {
        static constexpr uint32_t msr_id = 0x48e;
        using result_type = uint64_t;
      };

      struct star
      {
        static constexpr uint32_t msr_id = 0xc0000081;
        using result_type = uint64_t;
      };

      struct lstar
      {
        static constexpr uint32_t msr_id = 0xc0000082;
        using result_type = uint64_t;
      };

      struct cstar
      {
        static constexpr uint32_t msr_id = 0xc0000083;
        using result_type = uint64_t;
      };

      struct fmask
      {
        static constexpr uint32_t msr_id = 0xc0000084;
        using result_type = rflags_t;
      };

      struct fs_base
      {
        static constexpr uint32_t msr_id = 0xc0000100;
        using result_type = uint64_t;
      };

      struct gs_base
      {
        static constexpr uint32_t msr_id = 0xc0000101;
        using result_type = uint64_t;
      };

      union register_content
      {
        struct
        {
          uint32_t low;
          uint32_t high;
        };

        uint64_t all;
      };
    }
  }
}
