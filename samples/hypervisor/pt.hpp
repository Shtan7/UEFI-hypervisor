#pragma once
#include <cstdint>
#include "common.hpp"

namespace hh::pt
{
  /**
 * @brief Format of a 4-Level PML4 Entry (PML4E) that References a Page-Directory-Pointer Table
 */
  union pml4e_64
  {
    struct
    {
      /**
       * [Bit 0] present; must be 1 to reference a page-directory-pointer table.
       */
      uint64_t present : 1;

      /**
       * [Bit 1] Read/write; if 0, writes may not be allowed to the 512-GByte region controlled by this entry.
       *
       * @see Vol3A[4.6(Access Rights)]
       */
      uint64_t write : 1;

      /**
       * [Bit 2] User/supervisor; if 0, user-mode accesses are not allowed to the 512-GByte region controlled by this entry.
       *
       * @see Vol3A[4.6(Access Rights)]
       */
      uint64_t supervisor : 1;

      /**
       * [Bit 3] Page-level write-through; indirectly determines the memory type used to access the page-directory-pointer table
       * referenced by this entry.
       *
       * @see Vol3A[4.9.2(Paging and Memory Typing When the pat is Supported (Pentium III and More Recent Processor Families))]
       */
      uint64_t page_level_write_through : 1;

      /**
       * [Bit 4] Page-level cache disable; indirectly determines the memory type used to access the page-directory-pointer table
       * referenced by this entry.
       *
       * @see Vol3A[4.9.2(Paging and Memory Typing When the pat is Supported (Pentium III and More Recent Processor Families))]
       */
      uint64_t page_level_cache_disable : 1;

      /**
       * [Bit 5] accessed; indicates whether this entry has been used for linear-address translation.
       *
       * @see Vol3A[4.8(accessed and dirty flags)]
       */
      uint64_t accessed : 1;
      uint64_t reserved1 : 1;

      /**
       * [Bit 7] Reserved (must be 0).
       */
      uint64_t must_be_zero : 1;

      /**
       * [Bits 10:8] Ignored.
       */
      uint64_t ignored1 : 3;

      /**
       * [Bit 11] For ordinary paging, ignored; for HLAT paging, restart (if 1, linear-address translation is restarted with
       * ordinary paging)
       *
       * @see Vol3A[4.5.5(restart of HLAT Paging)]
       */
      uint64_t restart : 1;

      /**
       * [Bits 47:12] Physical address of 4-KByte aligned page-directory-pointer table referenced by this entry.
       */
      uint64_t page_frame_number : 36;
      uint64_t reserved2 : 4;

      /**
       * [Bits 62:52] Ignored.
       */
      uint64_t ignored2 : 11;

      /**
       * [Bit 63] If IA32_EFER.NXE = 1, execute-disable (if 1, instruction fetches are not allowed from the 512-GByte region
       * controlled by this entry); otherwise, reserved (must be 0).
       *
       * @see Vol3A[4.6(Access Rights)]
       */
      uint64_t execute_disable : 1;
    };

    uint64_t as_uint;
  };

  /**
   * @brief Format of a 4-Level Page-Directory-Pointer-Table Entry (PDPTE) that Maps a 1-GByte Page
   */
  union pdpte_1gb_64
  {
    struct
    {
      /**
       * [Bit 0] present; must be 1 to map a 1-GByte page.
       */
      uint64_t present : 1;

      /**
       * [Bit 1] Read/write; if 0, writes may not be allowed to the 1-GByte page referenced by this entry.
       *
       * @see Vol3A[4.6(Access Rights)]
       */
      uint64_t write : 1;

      /**
       * [Bit 2] User/supervisor; if 0, user-mode accesses are not allowed to the 1-GByte page referenced by this entry.
       *
       * @see Vol3A[4.6(Access Rights)]
       */
      uint64_t supervisor : 1;

      /**
       * [Bit 3] Page-level write-through; indirectly determines the memory type used to access the 1-GByte page referenced by
       * this entry.
       *
       * @see Vol3A[4.9.2(Paging and Memory Typing When the pat is Supported (Pentium III and More Recent Processor Families))]
       */
      uint64_t page_level_write_through : 1;

      /**
       * [Bit 4] Page-level cache disable; indirectly determines the memory type used to access the 1-GByte page referenced by
       * this entry.
       *
       * @see Vol3A[4.9.2(Paging and Memory Typing When the pat is Supported (Pentium III and More Recent Processor Families))]
       */
      uint64_t page_level_cache_disable : 1;

      /**
       * [Bit 5] accessed; indicates whether software has accessed the 1-GByte page referenced by this entry.
       *
       * @see Vol3A[4.8(accessed and dirty flags)]
       */
      uint64_t accessed : 1;

      /**
       * [Bit 6] dirty; indicates whether software has written to the 1-GByte page referenced by this entry.
       *
       * @see Vol3A[4.8(accessed and dirty flags)]
       */
      uint64_t dirty : 1;

      /**
       * [Bit 7] Page size; must be 1 (otherwise, this entry references a page directory).
       */
      uint64_t large_page : 1;

      /**
       * [Bit 8] global; if CR4.PGE = 1, determines whether the translation is global; ignored otherwise.
       *
       * @see Vol3A[4.10(Caching Translation Information)]
       */
      uint64_t global : 1;

      /**
       * [Bits 10:9] Ignored.
       */
      uint64_t ignored1 : 2;

      /**
       * [Bit 11] For ordinary paging, ignored; for HLAT paging, restart (if 1, linear-address translation is restarted with
       * ordinary paging)
       *
       * @see Vol3A[4.5.5(restart of HLAT Paging)]
       */
      uint64_t restart : 1;

      /**
       * [Bit 12] Indirectly determines the memory type used to access the 1-GByte page referenced by this entry.
       *
       * @note The pat is supported on all processors that support 4-level paging.
       * @see Vol3A[4.9.2(Paging and Memory Typing When the pat is Supported (Pentium III and More Recent Processor Families))]
       */
      uint64_t pat : 1;
      uint64_t reserved1 : 17;

      /**
       * [Bits 47:30] Physical address of the 1-GByte page referenced by this entry.
       */
      uint64_t page_frame_number : 18;
      uint64_t reserved2 : 4;

      /**
       * [Bits 58:52] Ignored.
       */
      uint64_t ignored2 : 7;

      /**
       * [Bits 62:59] Protection key; if CR4.PKE = 1, determines the protection key of the page; ignored otherwise.
       *
       * @see Vol3A[4.6.2(Protection Keys)]
       */
      uint64_t protection_key : 4;

      /**
       * [Bit 63] If IA32_EFER.NXE = 1, execute-disable (if 1, instruction fetches are not allowed from the 1-GByte page
       * controlled by this entry); otherwise, reserved (must be 0).
       *
       * @see Vol3A[4.6(Access Rights)]
       */
      uint64_t execute_disable : 1;
    };

    uint64_t as_uint;
  };

  /**
   * @brief Format of a 4-Level Page-Directory-Pointer-Table Entry (PDPTE) that References a Page Directory
   */
  union pdpte_64
  {
    struct
    {
      /**
       * [Bit 0] present; must be 1 to reference a page directory.
       */
      uint64_t present : 1;

      /**
       * [Bit 1] Read/write; if 0, writes may not be allowed to the 1-GByte region controlled by this entry.
       *
       * @see Vol3A[4.6(Access Rights)]
       */
      uint64_t write : 1;

      /**
       * [Bit 2] User/supervisor; if 0, user-mode accesses are not allowed to the 1-GByte region controlled by this entry.
       *
       * @see Vol3A[4.6(Access Rights)]
       */
      uint64_t supervisor : 1;

      /**
       * [Bit 3] Page-level write-through; indirectly determines the memory type used to access the page directory referenced by
       * this entry.
       *
       * @see Vol3A[4.9.2(Paging and Memory Typing When the pat is Supported (Pentium III and More Recent Processor Families))]
       */
      uint64_t page_level_write_through : 1;

      /**
       * [Bit 4] Page-level cache disable; indirectly determines the memory type used to access the page directory referenced by
       * this entry.
       *
       * @see Vol3A[4.9.2(Paging and Memory Typing When the pat is Supported (Pentium III and More Recent Processor Families))]
       */
      uint64_t page_level_cache_disable : 1;

      /**
       * [Bit 5] accessed; indicates whether this entry has been used for linear-address translation.
       *
       * @see Vol3A[4.8(accessed and dirty flags)]
       */
      uint64_t accessed : 1;
      uint64_t reserved1 : 1;

      /**
       * [Bit 7] Page size; must be 0 (otherwise, this entry maps a 1-GByte page).
       */
      uint64_t large_page : 1;

      /**
       * [Bits 10:8] Ignored.
       */
      uint64_t ignored1 : 3;

      /**
       * [Bit 11] For ordinary paging, ignored; for HLAT paging, restart (if 1, linear-address translation is restarted with
       * ordinary paging)
       *
       * @see Vol3A[4.5.5(restart of HLAT Paging)]
       */
      uint64_t restart : 1;

      /**
       * [Bits 47:12] Physical address of 4-KByte aligned page directory referenced by this entry.
       */
      uint64_t page_frame_number : 36;
      uint64_t reserved2 : 4;

      /**
       * [Bits 62:52] Ignored.
       */
      uint64_t ignored2 : 11;

      /**
       * [Bit 63] If IA32_EFER.NXE = 1, execute-disable (if 1, instruction fetches are not allowed from the 1-GByte region
       * controlled by this entry); otherwise, reserved (must be 0).
       *
       * @see Vol3A[4.6(Access Rights)]
       */
      uint64_t execute_disable : 1;
    };

    uint64_t as_uint;
  };

  /**
   * @brief Format of a 4-Level Page-Directory Entry that Maps a 2-MByte Page
   */
  union pde_2mb_64
  {
    struct
    {
      /**
       * [Bit 0] present; must be 1 to map a 2-MByte page.
       */
      uint64_t present : 1;

      /**
       * [Bit 1] Read/write; if 0, writes may not be allowed to the 2-MByte page referenced by this entry.
       *
       * @see Vol3A[4.6(Access Rights)]
       */
      uint64_t write : 1;

      /**
       * [Bit 2] User/supervisor; if 0, user-mode accesses are not allowed to the 2-MByte page referenced by this entry.
       *
       * @see Vol3A[4.6(Access Rights)]
       */
      uint64_t supervisor : 1;

      /**
       * [Bit 3] Page-level write-through; indirectly determines the memory type used to access the 2-MByte page referenced by
       * this entry.
       *
       * @see Vol3A[4.9.2(Paging and Memory Typing When the pat is Supported (Pentium III and More Recent Processor Families))]
       */
      uint64_t page_level_write_through : 1;

      /**
       * [Bit 4] Page-level cache disable; indirectly determines the memory type used to access the 2-MByte page referenced by
       * this entry.
       *
       * @see Vol3A[4.9.2(Paging and Memory Typing When the pat is Supported (Pentium III and More Recent Processor Families))]
       */
      uint64_t page_level_cache_disable : 1;

      /**
       * [Bit 5] accessed; indicates whether software has accessed the 2-MByte page referenced by this entry.
       *
       * @see Vol3A[4.8(accessed and dirty flags)]
       */
      uint64_t accessed : 1;

      /**
       * [Bit 6] dirty; indicates whether software has written to the 2-MByte page referenced by this entry.
       *
       * @see Vol3A[4.8(accessed and dirty flags)]
       */
      uint64_t dirty : 1;

      /**
       * [Bit 7] Page size; must be 1 (otherwise, this entry references a page directory).
       */
      uint64_t large_page : 1;

      /**
       * [Bit 8] global; if CR4.PGE = 1, determines whether the translation is global; ignored otherwise.
       *
       * @see Vol3A[4.10(Caching Translation Information)]
       */
      uint64_t global : 1;

      /**
       * [Bits 10:9] Ignored.
       */
      uint64_t ignored1 : 2;

      /**
       * [Bit 11] For ordinary paging, ignored; for HLAT paging, restart (if 1, linear-address translation is restarted with
       * ordinary paging)
       *
       * @see Vol3A[4.5.5(restart of HLAT Paging)]
       */
      uint64_t restart : 1;

      /**
       * [Bit 12] Indirectly determines the memory type used to access the 2-MByte page referenced by this entry.
       *
       * @note The pat is supported on all processors that support 4-level paging.
       * @see Vol3A[4.9.2(Paging and Memory Typing When the pat is Supported (Pentium III and More Recent Processor Families))]
       */
      uint64_t pat : 1;
      uint64_t reserved1 : 8;

      /**
       * [Bits 47:21] Physical address of the 2-MByte page referenced by this entry.
       */
      uint64_t page_frame_number : 27;
      uint64_t reserved2 : 4;

      /**
       * [Bits 58:52] Ignored.
       */
      uint64_t ignored2 : 7;

      /**
       * [Bits 62:59] Protection key; if CR4.PKE = 1, determines the protection key of the page; ignored otherwise.
       *
       * @see Vol3A[4.6.2(Protection Keys)]
       */
      uint64_t protection_key : 4;

      /**
       * [Bit 63] If IA32_EFER.NXE = 1, execute-disable (if 1, instruction fetches are not allowed from the 2-MByte page
       * controlled by this entry); otherwise, reserved (must be 0).
       *
       * @see Vol3A[4.6(Access Rights)]
       */
      uint64_t execute_disable : 1;
    };

    uint64_t as_uint;
  };

  /**
   * @brief Format of a 4-Level Page-Directory Entry that References a Page Table
   */
  union pde_64
  {
    struct
    {
      /**
       * [Bit 0] present; must be 1 to reference a page table.
       */
      uint64_t present : 1;

      /**
       * [Bit 1] Read/write; if 0, writes may not be allowed to the 2-MByte region controlled by this entry.
       *
       * @see Vol3A[4.6(Access Rights)]
       */
      uint64_t write : 1;

      /**
       * [Bit 2] User/supervisor; if 0, user-mode accesses are not allowed to the 2-MByte region controlled by this entry.
       *
       * @see Vol3A[4.6(Access Rights)]
       */
      uint64_t supervisor : 1;

      /**
       * [Bit 3] Page-level write-through; indirectly determines the memory type used to access the page table referenced by this
       * entry.
       *
       * @see Vol3A[4.9.2(Paging and Memory Typing When the pat is Supported (Pentium III and More Recent Processor Families))]
       */
      uint64_t page_level_write_through : 1;

      /**
       * [Bit 4] Page-level cache disable; indirectly determines the memory type used to access the page table referenced by this
       * entry.
       *
       * @see Vol3A[4.9.2(Paging and Memory Typing When the pat is Supported (Pentium III and More Recent Processor Families))]
       */
      uint64_t page_level_cache_disable : 1;

      /**
       * [Bit 5] accessed; indicates whether this entry has been used for linear-address translation.
       *
       * @see Vol3A[4.8(accessed and dirty flags)]
       */
      uint64_t accessed : 1;
      uint64_t reserved1 : 1;

      /**
       * [Bit 7] Page size; must be 0 (otherwise, this entry maps a 2-MByte page).
       */
      uint64_t large_page : 1;

      /**
       * [Bits 10:8] Ignored.
       */
      uint64_t ignored1 : 3;

      /**
       * [Bit 11] For ordinary paging, ignored; for HLAT paging, restart (if 1, linear-address translation is restarted with
       * ordinary paging)
       *
       * @see Vol3A[4.5.5(restart of HLAT Paging)]
       */
      uint64_t restart : 1;

      /**
       * [Bits 47:12] Physical address of 4-KByte aligned page table referenced by this entry.
       */
      uint64_t page_frame_number : 36;
      uint64_t reserved2 : 4;

      /**
       * [Bits 62:52] Ignored.
       */
      uint64_t ignored2 : 11;

      /**
       * [Bit 63] If IA32_EFER.NXE = 1, execute-disable (if 1, instruction fetches are not allowed from the 2-MByte region
       * controlled by this entry); otherwise, reserved (must be 0).
       *
       * @see Vol3A[4.6(Access Rights)]
       */
      uint64_t execute_disable : 1;
    };

    uint64_t as_uint;
  };

  /**
   * @brief Format of a 4-Level Page-Table Entry that Maps a 4-KByte Page
   */
  union pte_64
  {
    struct
    {
      /**
       * [Bit 0] present; must be 1 to map a 4-KByte page.
       */
      uint64_t present : 1;

      /**
       * [Bit 1] Read/write; if 0, writes may not be allowed to the 4-KByte page referenced by this entry.
       *
       * @see Vol3A[4.6(Access Rights)]
       */
      uint64_t write : 1;

      /**
       * [Bit 2] User/supervisor; if 0, user-mode accesses are not allowed to the 4-KByte page referenced by this entry.
       *
       * @see Vol3A[4.6(Access Rights)]
       */
      uint64_t supervisor : 1;

      /**
       * [Bit 3] Page-level write-through; indirectly determines the memory type used to access the 4-KByte page referenced by
       * this entry.
       *
       * @see Vol3A[4.9.2(Paging and Memory Typing When the pat is Supported (Pentium III and More Recent Processor Families))]
       */
      uint64_t page_level_write_through : 1;

      /**
       * [Bit 4] Page-level cache disable; indirectly determines the memory type used to access the 4-KByte page referenced by
       * this entry.
       *
       * @see Vol3A[4.9.2(Paging and Memory Typing When the pat is Supported (Pentium III and More Recent Processor Families))]
       */
      uint64_t page_level_cache_disable : 1;

      /**
       * [Bit 5] accessed; indicates whether software has accessed the 4-KByte page referenced by this entry.
       *
       * @see Vol3A[4.8(accessed and dirty flags)]
       */
      uint64_t accessed : 1;

      /**
       * [Bit 6] dirty; indicates whether software has written to the 4-KByte page referenced by this entry.
       *
       * @see Vol3A[4.8(accessed and dirty flags)]
       */
      uint64_t dirty : 1;

      /**
       * [Bit 7] Indirectly determines the memory type used to access the 4-KByte page referenced by this entry.
       *
       * @see Vol3A[4.9.2(Paging and Memory Typing When the pat is Supported (Pentium III and More Recent Processor Families))]
       */
      uint64_t pat : 1;

      /**
       * [Bit 8] global; if CR4.PGE = 1, determines whether the translation is global; ignored otherwise.
       *
       * @see Vol3A[4.10(Caching Translation Information)]
       */
      uint64_t global : 1;

      /**
       * [Bits 10:9] Ignored.
       */

      uint64_t ignored1 : 2;
      /**
       * [Bit 11] For ordinary paging, ignored; for HLAT paging, restart (if 1, linear-address translation is restarted with
       * ordinary paging)
       *
       * @see Vol3A[4.5.5(restart of HLAT Paging)]
       */
      uint64_t restart : 1;

      /**
       * [Bits 47:12] Physical address of the 4-KByte page referenced by this entry.
       */
      uint64_t page_frame_number : 36;
      uint64_t reserved1 : 4;

      /**
       * [Bits 58:52] Ignored.
       */
      uint64_t ignored2 : 7;

      /**
       * [Bits 62:59] Protection key; if CR4.PKE = 1, determines the protection key of the page; ignored otherwise.
       *
       * @see Vol3A[4.6.2(Protection Keys)]
       */
      uint64_t protection_key : 4;

      /**
       * [Bit 63] If IA32_EFER.NXE = 1, execute-disable (if 1, instruction fetches are not allowed from the 1-GByte page
       * controlled by this entry); otherwise, reserved (must be 0).
       *
       * @see Vol3A[4.6(Access Rights)]
       */
      uint64_t execute_disable : 1;
    };

    uint64_t as_uint;
  };

  struct page_entry
  {
    union
    {
      uint64_t flags;

      pml4e_64     pml4;
      pdpte_1gb_64 pdpt_large; // 1GB
      pdpte_64     pdpt;
      pde_2mb_64   pd_large; // 2MB
      pde_64       pd;
      pte_64       pt;

      //
      // Common fields.
      //

      struct
      {
        uint64_t present : 1;
        uint64_t write : 1;
        uint64_t supervisor : 1;
        uint64_t page_level_write_through : 1;
        uint64_t page_level_cache_disable : 1;
        uint64_t accessed : 1;
        uint64_t dirty : 1;
        uint64_t large_page : 1;
        uint64_t global : 1;
        uint64_t ignored1 : 3;
        uint64_t page_frame_number : 36;
        uint64_t reserved1 : 4;
        uint64_t ignored2 : 7;
        uint64_t protection_key : 4;
        uint64_t execute_disable : 1;
      } fields;
    };
  };

  /* During the boot process os loader changes virtual address mappings. We can't change our addresses to new mappings because of vm exits,
    so we just create our own tables with identical mapping ( virtual address = physical address ). */
  constexpr uint32_t pt_pml4_count = 2;
  constexpr uint32_t pt_pml3_large_count = 512;
  constexpr uint32_t pt_pml3_count = 1;
  constexpr uint32_t pt_pml2_count = 32;
  constexpr uint32_t pt_pml1_count = 512;

  struct host_mapping_table
  {
    // host indentical mapping
    DECLSPEC_ALIGN(common::page_size) pml4e_64 pml4[pt_pml4_count];
    DECLSPEC_ALIGN(common::page_size) pdpte_1gb_64 pml3_large[pt_pml3_large_count];

    // tables for mapping guest addresses
    DECLSPEC_ALIGN(common::page_size) pdpte_64 pml3[pt_pml3_count];
    DECLSPEC_ALIGN(common::page_size) pde_64 pml2[pt_pml3_count][pt_pml2_count];
    DECLSPEC_ALIGN(common::page_size) pte_64 pml1[pt_pml3_count][pt_pml2_count][pt_pml1_count];
  };
}
