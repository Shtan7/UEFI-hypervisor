#pragma once
#include <map>
#include <string_view>

namespace hh
{
  template<class T> std::map<T, std::string_view> enum_map;

  template<class T>
  std::string_view enum_to_str(T enum_value) noexcept
  {
    try
    {
      return enum_map<T>.at(enum_value);
    }
    catch (...)
    {
      return "UNKNOWN VALUE";
    }
  }
}
