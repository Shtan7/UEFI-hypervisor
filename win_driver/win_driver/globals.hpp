#pragma once

namespace hh
{
  class memory_manager;

  namespace hook
  {
    class hook_builder;
  }

	namespace globals
  {
    inline hook::hook_builder* hook_builder = {};
    inline memory_manager* mem_manager = {};
  }
}
