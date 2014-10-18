#ifndef DPE_SERVICE_DPE_SERVICE_H_
#define DPE_SERVICE_DPE_SERVICE_H_

#include "dpe_base/dpe_base.h"
#include "dpe_service/main/zserver.h"

namespace ds
{
// ipc message handling: bind and receive
// ipc message notification: bind and send
class DPEService : public base::MessageHandler
{
public:
  DPEService();
  ~DPEService();
  
  void Start();
  void WillStop();
  
private:
  void StopImpl();
  
private:
  int32_t handle_message(int32_t handle, const std::string& data) override;
private:
  std::string ipc_sub_address_;
  int32_t     ipc_sub_handle_;
  std::vector<ZServer*> server_list_;
};
}

#endif