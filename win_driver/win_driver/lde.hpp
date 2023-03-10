#pragma once
#define ZYDIS_STATIC_DEFINE
#include <Zycore/Format.h>
#include <Zycore/LibC.h>
#include <Zydis/Zydis.h>

namespace hh
{
  // Zydis disassembler is required for EPT hooks
  class disassembler
  {
  private:
    ZydisDecoder decoder_;

  public:
    disassembler() noexcept;
    uint32_t get_instructions_length(uint8_t* target_address, uint32_t min_acceptable_length) const;
  };
}
