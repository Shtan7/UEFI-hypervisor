#pragma once
#include <cstdint>
#include <memory>
#include "common.hpp"

namespace hh::ept
{
  enum class memory_type : uint8_t
  {
    uncacheable = 0x00000000,
    combining = 0x00000001,
    write_through = 0x00000004,
    write_protected = 0x00000005,
    write_back = 0x00000006,
    invalid = 0x000000FF,
  };

  union ept_address
  {
    struct
    {
      uint64_t page_offset : 12;
      uint64_t pml1_index : 9;
      uint64_t pml2_index : 9;
      uint64_t pml3_index : 9;
      uint64_t pml4_index : 9;
    };

    uint64_t all;
  };

  union ept_pml4
  {
    struct
    {
      /**
        * [Bit 0] Read access; indicates whether reads are allowed from the 512-GByte region controlled by this entry.
        */
      uint64_t read_access : 1;

      /**
        * [Bit 1] Write access; indicates whether writes are allowed from the 512-GByte region controlled by this entry.
        */
      uint64_t write_access : 1;

      /**
        * [Bit 2] If the "mode-based execute control for EPT" VM-execution control is 0, execute access; indicates whether
        * instruction fetches are allowed from the 512-GByte region controlled by this entry.
        * If that control is 1, execute access for supervisor-mode linear addresses; indicates whether instruction fetches are
        * allowed from supervisor-mode linear addresses in the 512-GByte region controlled by this entry.
        */
      uint64_t execute_access : 1;
      uint64_t reserved_1 : 5;

      /**
        * [Bit 8] If bit 6 of EPTP is 1, accessed flag for EPT; indicates whether software has accessed the 512-GByte region
        * controlled by this entry. Ignored if bit 6 of EPTP is 0.
        *
        * @see Vol3C[28.2.4(Accessed and Dirty Flags for EPT)]
        */
      uint64_t accessed : 1;
      uint64_t reserved_2 : 1;

      /**
        * [Bit 10] Execute access for user-mode linear addresses. If the "mode-based execute control for EPT" VM-execution control
        * is 1, indicates whether instruction fetches are allowed from user-mode linear addresses in the 512-GByte region
        * controlled by this entry. If that control is 0, this bit is ignored.
        */
      uint64_t user_mode_execute : 1;
      uint64_t reserved_3 : 1;

      /**
        * [Bits 47:12] Physical address of 4-KByte aligned EPT page-directory-pointer table referenced by this entry.
        */
      uint64_t page_frame_number : 36;
      uint64_t reserved_4 : 16;
    };

    uint64_t flags;
  };

  union epdpte_1gb
  {
    struct
    {
      /**
        * [Bit 0] Read access; indicates whether reads are allowed from the 1-GByte page referenced by this entry.
        */
      uint64_t read_access : 1;

      /**
        * [Bit 1] Write access; indicates whether writes are allowed from the 1-GByte page referenced by this entry.
        */
      uint64_t write_access : 1;

      /**
        * [Bit 2] If the "mode-based execute control for EPT" VM-execution control is 0, execute access; indicates whether
        * instruction fetches are allowed from the 1-GByte page controlled by this entry.
        * If that control is 1, execute access for supervisor-mode linear addresses; indicates whether instruction fetches are
        * allowed from supervisor-mode linear addresses in the 1-GByte page controlled by this entry.
        */
      uint64_t execute_access : 1;

      /**
        * [Bits 5:3] EPT memory type for this 1-GByte page.
        *
        * @see Vol3C[28.2.6(EPT and memory Typing)]
        */
      uint64_t memory_type : 3;

      /**
        * [Bit 6] Ignore PAT memory type for this 1-GByte page.
        *
        * @see Vol3C[28.2.6(EPT and memory Typing)]
        */
      uint64_t ignore_pat : 1;

      /**
        * [Bit 7] Must be 1 (otherwise, this entry references an EPT page directory).
        */
      uint64_t large_page : 1;

      /**
        * [Bit 8] If bit 6 of EPTP is 1, accessed flag for EPT; indicates whether software has accessed the 1-GByte page
        * referenced by this entry. Ignored if bit 6 of EPTP is 0.
        *
        * @see Vol3C[28.2.4(Accessed and Dirty Flags for EPT)]
        */
      uint64_t accessed : 1;

      /**
        * [Bit 9] If bit 6 of EPTP is 1, dirty flag for EPT; indicates whether software has written to the 1-GByte page referenced
        * by this entry. Ignored if bit 6 of EPTP is 0.
        *
        * @see Vol3C[28.2.4(Accessed and Dirty Flags for EPT)]
        */
      uint64_t dirty : 1;

      /**
        * [Bit 10] Execute access for user-mode linear addresses. If the "mode-based execute control for EPT" VM-execution control
        * is 1, indicates whether instruction fetches are allowed from user-mode linear addresses in the 1-GByte page controlled
        * by this entry. If that control is 0, this bit is ignored.
        */
      uint64_t user_mode_execute : 1;
      uint64_t reserved_1 : 19;

      /**
        * [Bits 47:30] Physical address of 4-KByte aligned EPT page-directory-pointer table referenced by this entry.
        */
      uint64_t page_frame_number : 18;
      uint64_t reserved_2 : 15;

      /**
        * [Bit 63] Suppress \#VE. If the "EPT-violation \#VE" VM-execution control is 1, EPT violations caused by accesses to this
        * page are convertible to virtualization exceptions only if this bit is 0. If "EPT-violation \#VE" VMexecution control is
        * 0, this bit is ignored.
        *
        * @see Vol3C[25.5.6.1(Convertible EPT Violations)]
        */
      uint64_t suppress_ve : 1;
    };

    uint64_t flags;
  };

  union epdpte
  {
    struct
    {
      /**
        * [Bit 0] Read access; indicates whether reads are allowed from the 1-GByte region controlled by this entry.
        */
      uint64_t read_access : 1;

      /**
        * [Bit 1] Write access; indicates whether writes are allowed from the 1-GByte region controlled by this entry.
        */
      uint64_t write_access : 1;

      /**
        * [Bit 2] If the "mode-based execute control for EPT" VM-execution control is 0, execute access; indicates whether
        * instruction fetches are allowed from the 1-GByte region controlled by this entry.
        * If that control is 1, execute access for supervisor-mode linear addresses; indicates whether instruction fetches are
        * allowed from supervisor-mode linear addresses in the 1-GByte region controlled by this entry.
        */
      uint64_t execute_access : 1;
      uint64_t reserved_1 : 5;

      /**
        * [Bit 8] If bit 6 of EPTP is 1, accessed flag for EPT; indicates whether software has accessed the 1-GByte region
        * controlled by this entry. Ignored if bit 6 of EPTP is 0.
        *
        * @see Vol3C[28.2.4(Accessed and Dirty Flags for EPT)]
        */
      uint64_t accessed : 1;
      uint64_t reserved_2 : 1;

      /**
        * [Bit 10] Execute access for user-mode linear addresses. If the "mode-based execute control for EPT" VM-execution control
        * is 1, indicates whether instruction fetches are allowed from user-mode linear addresses in the 1-GByte region controlled
        * by this entry. If that control is 0, this bit is ignored.
        */
      uint64_t user_mode_execute : 1;
      uint64_t reserved_3 : 1;

      /**
        * [Bits 47:12] Physical address of 4-KByte aligned EPT page-directory-pointer table referenced by this entry.
        */
      uint64_t page_frame_number : 36;
      uint64_t reserved_4 : 16;
    };

    uint64_t flags;
  };

  union epde_2mb
  {
    struct
    {
      /**
        * [Bit 0] Read access; indicates whether reads are allowed from the 2-MByte page referenced by this entry.
        */
      uint64_t read_access : 1;

      /**
        * [Bit 1] Write access; indicates whether writes are allowed from the 2-MByte page referenced by this entry.
        */
      uint64_t write_access : 1;

      /**
        * [Bit 2] If the "mode-based execute control for EPT" VM-execution control is 0, execute access; indicates whether
        * instruction fetches are allowed from the 2-MByte page controlled by this entry.
        * If that control is 1, execute access for supervisor-mode linear addresses; indicates whether instruction fetches are
        * allowed from supervisor-mode linear addresses in the 2-MByte page controlled by this entry.
        */
      uint64_t execute_access : 1;

      /**
        * [Bits 5:3] EPT memory type for this 2-MByte page.
        *
        * @see Vol3C[28.2.6(EPT and memory Typing)]
        */
      uint64_t memory_type : 3;

      /**
        * [Bit 6] Ignore PAT memory type for this 2-MByte page.
        *
        * @see Vol3C[28.2.6(EPT and memory Typing)]
        */
      uint64_t ignore_pat : 1;

      /**
        * [Bit 7] Must be 1 (otherwise, this entry references an EPT page table).
        */
      uint64_t large_page : 1;

      /**
        * [Bit 8] If bit 6 of EPTP is 1, accessed flag for EPT; indicates whether software has accessed the 2-MByte page
        * referenced by this entry. Ignored if bit 6 of EPTP is 0.
        *
        * @see Vol3C[28.2.4(Accessed and Dirty Flags for EPT)]
        */
      uint64_t accessed : 1;

      /**
        * [Bit 9] If bit 6 of EPTP is 1, dirty flag for EPT; indicates whether software has written to the 2-MByte page referenced
        * by this entry. Ignored if bit 6 of EPTP is 0.
        *
        * @see Vol3C[28.2.4(Accessed and Dirty Flags for EPT)]
        */
      uint64_t dirty : 1;

      /**
        * [Bit 10] Execute access for user-mode linear addresses. If the "mode-based execute control for EPT" VM-execution control
        * is 1, indicates whether instruction fetches are allowed from user-mode linear addresses in the 2-MByte page controlled
        * by this entry. If that control is 0, this bit is ignored.
        */
      uint64_t user_mode_execute : 1;
      uint64_t reserved_1 : 10;

      /**
        * [Bits 47:21] Physical address of 4-KByte aligned EPT page-directory-pointer table referenced by this entry.
        */
      uint64_t page_frame_number : 27;
      uint64_t reserved_2 : 15;

      /**
        * [Bit 63] Suppress \#VE. If the "EPT-violation \#VE" VM-execution control is 1, EPT violations caused by accesses to this
        * page are convertible to virtualization exceptions only if this bit is 0. If "EPT-violation \#VE" VMexecution control is
        * 0, this bit is ignored.
        *
        * @see Vol3C[25.5.6.1(Convertible EPT Violations)]
        */
      uint64_t suppress_ve : 1;
    };

    uint64_t flags;
  };

  union epde
  {
    struct
    {
      /**
        * [Bit 0] Read access; indicates whether reads are allowed from the 2-MByte region controlled by this entry.
        */
      uint64_t read_access : 1;

      /**
        * [Bit 1] Write access; indicates whether writes are allowed from the 2-MByte region controlled by this entry.
        */
      uint64_t write_access : 1;

      /**
        * [Bit 2] If the "mode-based execute control for EPT" VM-execution control is 0, execute access; indicates whether
        * instruction fetches are allowed from the 2-MByte region controlled by this entry.
        * If that control is 1, execute access for supervisor-mode linear addresses; indicates whether instruction fetches are
        * allowed from supervisor-mode linear addresses in the 2-MByte region controlled by this entry.
        */
      uint64_t execute_access : 1;
      uint64_t reserved_1 : 5;

      /**
        * [Bit 8] If bit 6 of EPTP is 1, accessed flag for EPT; indicates whether software has accessed the 2-MByte region
        * controlled by this entry. Ignored if bit 6 of EPTP is 0.
        *
        * @see Vol3C[28.2.4(Accessed and Dirty Flags for EPT)]
        */
      uint64_t accessed : 1;
      uint64_t reserved_2 : 1;

      /**
        * [Bit 10] Execute access for user-mode linear addresses. If the "mode-based execute control for EPT" VM-execution control
        * is 1, indicates whether instruction fetches are allowed from user-mode linear addresses in the 2-MByte region controlled
        * by this entry. If that control is 0, this bit is ignored.
        */
      uint64_t user_mode_execute : 1;
      uint64_t reserved_3 : 1;

      /**
        * [Bits 47:12] Physical address of 4-KByte aligned EPT page table referenced by this entry.
        */
      uint64_t page_frame_number : 36;
      uint64_t reserved_4 : 16;
    };

    uint64_t flags;
  };

  union epte
  {
    struct
    {
      /**
        * [Bit 0] Read access; indicates whether reads are allowed from the 4-KByte page referenced by this entry.
        */
      uint64_t read_access : 1;

      /**
        * [Bit 1] Write access; indicates whether writes are allowed from the 4-KByte page referenced by this entry.
        */
      uint64_t write_access : 1;

      /**
        * [Bit 2] If the "mode-based execute control for EPT" VM-execution control is 0, execute access; indicates whether
        * instruction fetches are allowed from the 4-KByte page controlled by this entry.
        * If that control is 1, execute access for supervisor-mode linear addresses; indicates whether instruction fetches are
        * allowed from supervisor-mode linear addresses in the 4-KByte page controlled by this entry.
        */
      uint64_t execute_access : 1;

      /**
        * [Bits 5:3] EPT memory type for this 4-KByte page.
        *
        * @see Vol3C[28.2.6(EPT and memory Typing)]
        */
      uint64_t memory_type : 3;

      /**
        * [Bit 6] Ignore PAT memory type for this 4-KByte page.
        *
        * @see Vol3C[28.2.6(EPT and memory Typing)]
        */
      uint64_t ignore_pat : 1;
      uint64_t reserved_1 : 1;

      /**
        * [Bit 8] If bit 6 of EPTP is 1, accessed flag for EPT; indicates whether software has accessed the 4-KByte page
        * referenced by this entry. Ignored if bit 6 of EPTP is 0.
        *
        * @see Vol3C[28.2.4(Accessed and Dirty Flags for EPT)]
        */
      uint64_t accessed : 1;

      /**
        * [Bit 9] If bit 6 of EPTP is 1, dirty flag for EPT; indicates whether software has written to the 4-KByte page referenced
        * by this entry. Ignored if bit 6 of EPTP is 0.
        *
        * @see Vol3C[28.2.4(Accessed and Dirty Flags for EPT)]
        */
      uint64_t dirty : 1;

      /**
        * [Bit 10] Execute access for user-mode linear addresses. If the "mode-based execute control for EPT" VM-execution control
        * is 1, indicates whether instruction fetches are allowed from user-mode linear addresses in the 4-KByte page controlled
        * by this entry. If that control is 0, this bit is ignored.
        */
      uint64_t user_mode_execute : 1;
      uint64_t reserved_2 : 1;

      /**
        * [Bits 47:12] Physical address of the 4-KByte page referenced by this entry.
        */
      uint64_t page_frame_number : 36;
      uint64_t reserved_3 : 15;

      /**
        * [Bit 63] Suppress \#VE. If the "EPT-violation \#VE" VM-execution control is 1, EPT violations caused by accesses to this
        * page are convertible to virtualization exceptions only if this bit is 0. If "EPT-violation \#VE" VMexecution control is
        * 0, this bit is ignored.
        *
        * @see Vol3C[25.5.6.1(Convertible EPT Violations)]
        */
      uint64_t suppress_ve : 1;
    };

    uint64_t flags;
  };

  using pml4_pointer = ept_pml4;
  using pml3_pointer = epdpte;
  using pml2_entry = epde_2mb;
  using pml2_pointer = epde;
  using pml1_entry = epte;

  namespace vmm
  {
    // The number of 512GB PML4 entries in the page table/
    inline constexpr uint32_t pml4e_count = 512;

    // The number of 1GB PDPT entries in the page table per 512GB PML4 entry.
    inline constexpr uint32_t pml3e_count = 512;

    // Then number of 2MB Page Directory entries in the page table per 1GB PML3 entry.
    inline constexpr uint32_t pml2e_count = 512;

    // Then number of 4096 byte Page Table entries in the page table per 2MB PML2 entry when dynamically split.
    inline constexpr uint32_t pml1e_count = 512;

    struct dynamic_split
    {
      //The 4096 byte page table entries that correspond to the split 2MB table entry.
      DECLSPEC_ALIGN(common::page_size) pml1_entry pml1[pml1e_count];

      //The pointer to the 2MB entry in the page table which this split is servicing.
      union
      {
        pml2_entry* entry;
        pml2_pointer* pointer;
      };
    };

    struct page_table
    {
      /**
        * 28.2.2 Describes 512 contiguous 512GB memory regions each with 512 1GB regions.
        */
      DECLSPEC_ALIGN(common::page_size) pml4_pointer pml4[pml4e_count];

      /**
        * Describes exactly 512 contiguous 1GB memory regions within a our singular 512GB PML4 region.
        */
      DECLSPEC_ALIGN(common::page_size) pml3_pointer pml3[pml3e_count];

      /**
        * For each 1GB PML3 entry, create 512 2MB entries to map identity.
        * NOTE: We are using 2MB pages as the smallest paging size in our map, so we do not manage individiual 4096 byte pages.
        * Therefore, we do not allocate any PML1 (4096 byte) paging structures.
        */
      DECLSPEC_ALIGN(common::page_size) pml2_entry pml2[pml3e_count][pml2e_count];
    };
  }

  union eptp
  {
    struct
    {
      /**
        * [Bits 2:0] EPT paging-structure memory type:
        * - 0 = Uncacheable (UC)
        * - 6 = Write-back (WB)
        * Other values are reserved.
        *
        * @see Vol3C[28.2.6(EPT and memory Typing)]
        */
      uint64_t memory_type : 3;

      /**
        * [Bits 5:3] This value is 1 less than the EPT page-walk length.
        *
        * @see Vol3C[28.2.6(EPT and memory Typing)]
        */
      uint64_t page_walk_length : 3;

      /**
        * [Bit 6] Setting this control to 1 enables accessed and dirty flags for EPT.
        *
        * @see Vol3C[28.2.4(Accessed and Dirty Flags for EPT)]
        */
      uint64_t enable_access_and_dirty_flags : 1;
      uint64_t reserved_1 : 5;

      /**
        * [Bits 47:12] Bits N-1:12 of the physical address of the 4-KByte aligned EPT PML4 table.
        */
      uint64_t page_frame_number : 36;
      uint64_t reserved_2 : 16;
    };

    uint64_t flags;
  };

  struct invept_descriptor
  {
    uint64_t ept_pointer;
    uint64_t reserved; // Must be zero.
  };

  struct mttr_range_descriptor
  {
    uint64_t physical_base_address;
    uint64_t physical_end_address;
    memory_type memory_type;
  };

  struct ept_state
  {
    mttr_range_descriptor memory_ranges[9];	// Physical memory ranges described by the BIOS in the MTRRs. Used to build the EPT identity mapping.
    uint32_t number_of_enabled_memory_ranges;	// Number of memory ranges specified in MemoryRanges
    eptp ept_pointer;	// Extended-Page-Table Pointer 
    vmm::page_table* ept_page_table;  // Page table entries for EPT operation
  };

  union vmx_exit_qualification_ept_violation
  {
    struct
    {
      /**
        * [Bit 0] Set if the access causing the EPT violation was a data read.
        */
      uint64_t read_access : 1;

      /**
        * [Bit 1] Set if the access causing the EPT violation was a data write.
        */
      uint64_t write_access : 1;

      /**
        * [Bit 2] Set if the access causing the EPT violation was an instruction fetch.
        */
      uint64_t execute_access : 1;

      /**
        * [Bit 3] The logical-AND of bit 0 in the EPT paging-structure entries used to translate the guest-physical address of the
        * access causing the EPT violation (indicates whether the guest-physical address was readable).
        */
      uint64_t ept_readable : 1;

      /**
        * [Bit 4] The logical-AND of bit 1 in the EPT paging-structure entries used to translate the guest-physical address of the
        * access causing the EPT violation (indicates whether the guest-physical address was writeable).
        */
      uint64_t ept_writeable : 1;

      /**
        * [Bit 5] The logical-AND of bit 2 in the EPT paging-structure entries used to translate the guest-physical address of the
        * access causing the EPT violation.
        * If the "mode-based execute control for EPT" VM-execution control is 0, this indicates whether the guest-physical address
        * was executable. If that control is 1, this indicates whether the guest-physical address was executable for
        * supervisor-mode linear addresses.
        */
      uint64_t ept_executable : 1;

      /**
        * [Bit 6] If the "mode-based execute control" VM-execution control is 0, the value of this bit is undefined. If that
        * control is 1, this bit is the logical-AND of bit 10 in the EPT paging-structures entries used to translate the
        * guest-physical address of the access causing the EPT violation. In this case, it indicates whether the guest-physical
        * address was executable for user-mode linear addresses.
        */
      uint64_t ept_executable_for_user_mode : 1;

      /**
        * [Bit 7] Set if the guest linear-address field is valid. The guest linear-address field is valid for all EPT violations
        * except those resulting from an attempt to load the guest PDPTEs as part of the execution of the MOV CR instruction.
        */
      uint64_t valid_guest_linear_address : 1;

      /**
        * [Bit 8] If bit 7 is 1:
        * - Set if the access causing the EPT violation is to a guest-physical address that is the translation of a linear
        * address.
        * - Clear if the access causing the EPT violation is to a paging-structure entry as part of a page walk or the update of
        * an accessed or dirty bit.
        * Reserved if bit 7 is 0 (cleared to 0).
        */
      uint64_t caused_by_translation : 1;

      /**
        * [Bit 9] This bit is 0 if the linear address is a supervisor-mode linear address and 1 if it is a user-mode linear
        * address. Otherwise, this bit is undefined.
        *
        * @remarks If bit 7 is 1, bit 8 is 1, and the processor supports advanced VM-exit information for EPT violations. (If
        *          CR0.PG = 0, the translation of every linear address is a user-mode linear address and thus this bit will be 1.)
        */
      uint64_t user_mode_linear_address : 1;

      /**
        * [Bit 10] This bit is 0 if paging translates the linear address to a read-only page and 1 if it translates to a
        * read/write page. Otherwise, this bit is undefined
        *
        * @remarks If bit 7 is 1, bit 8 is 1, and the processor supports advanced VM-exit information for EPT violations. (If
        *          CR0.PG = 0, every linear address is read/write and thus this bit will be 1.)
        */
      uint64_t readable_writable_page : 1;

      /**
        * [Bit 11] This bit is 0 if paging translates the linear address to an executable page and 1 if it translates to an
        * execute-disable page. Otherwise, this bit is undefined.
        *
        * @remarks If bit 7 is 1, bit 8 is 1, and the processor supports advanced VM-exit information for EPT violations. (If
        *          CR0.PG = 0, CR4.PAE = 0, or MSR_IA32_EFER.NXE = 0, every linear address is executable and thus this bit will be 0.)
        */
      uint64_t execute_disable_page : 1;

      /**
        * [Bit 12] NMI unblocking due to IRET.
        */
      uint64_t nmi_unblocking : 1;
      uint64_t reserved_1 : 51;
    };

    uint64_t flags;
  };

  struct pagefault_error_code
  {
    union
    {
      uint32_t all;

      struct
      {
        uint32_t present : 1;
        uint32_t write : 1;
        uint32_t user_mode_access : 1;
        uint32_t reserved_bit_violation : 1;
        uint32_t execute : 1;
        uint32_t protection_key_violation : 1;
        uint32_t reserved_1 : 9;
        uint32_t sgx_access_violation : 1;
        uint32_t reserved_2 : 16;
      } flags;
    };
  };
}
