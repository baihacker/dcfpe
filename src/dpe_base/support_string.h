#ifndef DPE_BASE_SUPPORT_STRING_H_
#define DPE_BASE_SUPPORT_STRING_H_

#include "third_party/chromium/build/build_config.h"

#include <string>

#include "third_party/chromium/base/strings/string16.h"

// NativeString is file path, environment variable and so on.
#if defined(OS_WIN)
  typedef std::wstring NativeString;
#elif defined(OS_POSIX)
  typedef std::string NativeString;
#endif
typedef NativeString::value_type NativeChar;

#endif