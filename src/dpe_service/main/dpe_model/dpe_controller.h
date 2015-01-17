#ifndef DPE_SERVICE_MAIN_DPE_MODEL_DPE_CONTROLLER_H_
#define DPE_SERVICE_MAIN_DPE_MODEL_DPE_CONTROLLER_H_

#include "dpe_base/dpe_base.h"
#include "dpe_service/main/dpe_model/dpe_device.h"
#include "dpe_service/main/dpe_model/dpe_project.h"
#include "dpe_service/main/dpe_model/dpe_compiler.h"
#include "dpe_service/main/dpe_model/dpe_scheduler.h"

namespace ds
{
class DPEController :
  public ResourceBase,
  public DPECompilerHost,
  public DPESchedulerHost
{
  friend class DPECompiler;
  friend class DPEScheduler;
public:
  DPEController(DPEService* dpe);
  ~DPEController();

  bool  AddRemoteDPEService(bool is_local, int32_t ip, int32_t port);
  bool  AddRemoteDPEService(bool is_local, const std::string& server_address);

  bool  Start(const base::FilePath& job_path);

  bool  Stop();

private:
  void  OnCompileError();
  void  OnCompileSuccess();
  
  void  OnRunningError();
  void  OnRunningSuccess();

private:
  DPEService*                                   dpe_;

  std::vector<std::pair<bool, std::string> >    servers_;
  scoped_refptr<DPEProject>                     dpe_project_;
  scoped_refptr<DPECompiler>                    dpe_compiler_;
  scoped_refptr<DPEScheduler>                   dpe_scheduler_;

  int32_t                                       job_state_;
};
}
#endif