#ifndef DPE_SERVICE_DPE_SERVICE_H_
#define DPE_SERVICE_DPE_SERVICE_H_

#include "dpe_base/dpe_base.h"
#include "dpe_service/main/zserver.h"
#include "dpe_service/main/resource.h"
#include "dpe_service/main/compiler_resource.h"

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
  scoped_refptr<CompilerResource> CreateCompiler(
    const std::string& type, const std::string& version, int32_t arch, 
    int32_t language, const std::vector<base::FilePath>& source_file = std::vector<base::FilePath>());
  
private:
  int32_t handle_message(int32_t handle, const std::string& data) override;
  void DPEService::LoadCompilers(const base::FilePath& file);

private:
  std::string ipc_sub_address_;
  int32_t     ipc_sub_handle_;
  std::vector<ZServer*> server_list_;
  std::vector<CompilerConfiguration>  compilers_;
};
}

#endif