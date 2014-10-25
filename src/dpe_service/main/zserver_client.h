#ifndef DPE_SERVICE_MAIN_ZSERVER_CLIENT_H_
#define DPE_SERVICE_MAIN_ZSERVER_CLIENT_H_

#include "dpe_base/dpe_base.h"

namespace ds
{
class ZServerClient : public base::RefCounted<ZServerClient>
{
public:
  ZServerClient(const std::string& address);
  ~ZServerClient();
  
  bool  CreateDPEDevice(base::ZMQCallBack callback);
  bool  SayHello(base::ZMQCallBack callback);
  
private:
  std::string                                 server_address_;
};

}

#endif