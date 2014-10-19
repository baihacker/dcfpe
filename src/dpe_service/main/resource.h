#ifndef DPE_SERVICE_MAIN_RESOURCE_H_
#define DPE_SERVICE_MAIN_RESOURCE_H_

#include "dpe_base/dpe_base.h"
#include "process/process.h"

namespace ds
{
struct ResourceBase : public base::RefCounted<ResourceBase>
{
  virtual ~ResourceBase(){}
};
}

#endif