#ifndef SRC_PROCESS_PROCESS_H
#define SRC_PROCESS_PROCESS_H

#include "dpe_base/utility_interface.h"

namespace process{
enum
{
  INTERFACE_PROCESS = 200,
  INTERFACE_PROCESS_OBSERVER = 201,
};

struct IProcessObserver : public IDPEUnknown
{
  virtual void OnStop(int32_t exit_code) = 0;
};

struct IProcess : public IDPEUnknown
{
  virtual int32_t AddObserver(IProcessObserver* observer) = 0;
  virtual int32_t RemoveObserver(IProcessObserver* observer) = 0;
  virtual int32_t SetImagePath(const wchar_t* image) = 0;
  virtual int32_t SetArgument(const wchar_t* arg) = 0;
  virtual int32_t AppendArgumentItem(const wchar_t* arg_item) = 0;
  
  virtual int32_t Start() = 0;
  virtual int32_t Stop() = 0;
};

}

#endif