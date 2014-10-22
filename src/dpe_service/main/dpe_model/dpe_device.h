#ifndef DPE_SERVICE_MAIN_DPE_MODEL_DPE_DEVICE_H_
#define DPE_SERVICE_MAIN_DPE_MODEL_DPE_DEVICE_H_

#include "dpe_service/main/resource.h"

namespace ds
{
class DPEDevice : public ResourceBase
{
public:
  virtual std::string GetReceiveAddress() = 0;
  virtual std::string GetSendAddress() = 0;
  virtual std::string   GetSession() = 0;

  virtual bool        OpenDevice(int32_t ip) = 0;
  virtual bool        CloseDevice() = 0;
  virtual void        SetHomePath(const base::FilePath& path) = 0;

};

}

#endif