#pragma once

using DWORD = unsigned long;
using LONG = long;
using LONGLONG = long long;
using PVOID = void*;

union LARGE_INTEGER
{
  struct
  {
    DWORD LowPart;
    LONG HighPart;
  } DUMMYSTRUCTNAME;

  struct
  {
    DWORD LowPart;
    LONG HighPart;
  } u;

  LONGLONG QuadPart;
};

using physical_address_t = LARGE_INTEGER;
